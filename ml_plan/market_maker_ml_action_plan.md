# Market Maker — ML Pipeline Action Plan

A simulated HFT system built in C++, Go, and Python. This document covers the **ML pipeline** — Raghav's portion of the project, integrated via gRPC and WebSockets with Anshul's C++ trading core.

**Core rule across every phase:** ML is async-only. Predictions are cached. The hot path (order book, strategy engine, execution) never waits on ML.

---

## Phase 1 — Data Infrastructure (Week 1)

**Goal:** Connect to the exchange simulator's WebSocket feed and persist raw tick data to Parquet for ML training.

- Schema: `timestamp_ns, symbol, bid, ask, bid_sz, ask_sz, last_price, volume, sequence, seq_gap`
- Python async WebSocket subscriber with exponential backoff reconnection
- Buffered writes to Parquet via pyarrow (flush every 10k ticks or 5 seconds)
- File rotation after 500k ticks, one directory per symbol
- Tick validation: crossed book, price jumps, negative sizes — dropped and logged
- **Sequence gap handling:** gaps don't drop the tick — they set `seq_gap=True` on it. Phase 2 masks lookback features near these rows instead of silently corrupting them.
- **Replay logger (separate from tick recorder):** orders, fills, cancels, and strategy decisions saved independently for backtesting and debugging
- Target: collect ≥ 1M ticks before moving to Phase 2; prove the pipeline end-to-end on 50k–100k ticks first

---

## Phase 2 — Feature Engineering (Week 1–2)

**Goal:** Build microstructure features from raw ticks, with zero look-ahead bias.

- `spread = ask − bid`
- `mid = (bid + ask) / 2`, `microprice = size-weighted mid`
- `imbalance = (bid_sz − ask_sz) / (bid_sz + ask_sz)`
- `log_return_k = ln(mid_t / mid_{t−k})` for k ∈ {5, 10, 50 ticks}
- `rolling_vol = std(log_returns, window=20)`
- EWMA of spread, imbalance, and log returns at α = {0.01, 0.05, 0.1}
- **Gap safety:** rows within the lookback window of a `seq_gap=True` tick have their lookback features set to NaN. Instantaneous features (spread, mid, microprice, imbalance) are left valid since they only use the current tick.
- `feature_names()` ordering is the **proto contract** — this exact order is what `repeated float features` in the gRPC message maps to. Never reorder without bumping the pipeline version.
- `FeaturePipeline` is saved once and shared between training (batch `transform()`) and inference (streaming `transform_single()`) to guarantee parity.

**Critical:** never fit a scaler on the full dataset. Tree-based models (XGBoost/LightGBM) don't need normalization, which avoids this problem entirely.

---

## Phase 3 — Label Creation (Week 2)

**Goal:** Define prediction targets with zero look-ahead contamination and a transaction-cost floor.

- Prediction horizon N = 10 ticks (tune later)
- Binary label, cost-adjusted:
  ```
  label = 1 if (mid_{t+N} - mid_t) > α × spread + fees else 0
  ```
  α is a tunable multiplier — start near 1.0 and tune empirically. A raw `mid(t+N) > mid(t)` label trains on moves that aren't actually tradable after costs.
- 3-class option: up / flat / down, where flat = `|Δmid| < 0.5 × spread`. Cleaner signal than binary in noisy regimes.
- Mask any row where `seq_gap=True` appears within the forward window — those labels span missing data and are not trustworthy.
- pandas: `label = mid.shift(-N)`, then drop last N rows
- Audit every feature column to confirm no future data leaks in — this is the single most common HFT ML bug.

---

## Phase 4 — Model Training (Week 2–3)

**Goal:** Train a model that generalizes to unseen future data, and prove it beats triviality.

- Time split: 70% train / 15% val / 15% test — **never** random shuffle on time-series
- Walk-forward CV: `TimeSeriesSplit(n_splits=5)` from sklearn
- **Naive baselines first:** Always-Up, Random, Last-Return-Sign. Many HFT ML projects fail because the "real" model barely beats these. Establish the floor before training anything fancier.
- Baseline model: LogisticRegression. Target model: XGBoost or LightGBM
- Metrics: AUC-ROC, precision@signal, expected PnL net of transaction costs — not raw accuracy alone. A model can be directionally right 60% of the time and still lose money on tail moves.
- Run `feature_importances_`, prune weak features
- Save **all three** artifacts together:
  - `model.pkl`
  - `pipe.pkl` (the feature pipeline — must match what training used)
  - `metadata.json`: `{"features": [...], "horizon": 10, "train_start": "...", "train_end": "...", "model_version": "v1"}`

**Critical:** mismatched model/pipeline pairs are the most common silent inference bug. `metadata.json` becomes essential after the second or third retrain, once you can no longer remember which model was trained on what.

---

## Phase 5 — gRPC Inference Server (Week 3)

**Goal:** Serve predictions at < 1ms latency.

- Proto: `PredictionRequest{repeated float features}` → `PredictionResponse{float buy_prob, int direction}`
- Load model + pipeline once at startup — never per-request
- Inference: `pipe.transform(X)` → `model.predict_proba(X)[0][1]`
- Add a `HealthCheck` RPC so the Go client can detect ML degradation
- Benchmark: 10k requests, assert p99 < 1ms
- **Feature contract tests, run before every deployment:** assert `features[0] == spread`, `features[1] == imbalance`, etc. One dict shared between the C++ feature producer and the Python pipeline config is the cleanest way to keep this in sync. A single ordering mismatch silently destroys model performance with no errors thrown.

**Architecture note:** feature computation lives in one place — wherever the raw order-book data already is (the C++ side), not duplicated into Go. Go never touches feature logic; it only relays the cached prediction.

```
C++ computes raw features (it owns the order book state)
        ↓ gRPC (float array)
Python applies the saved FeaturePipeline (EWMA, rolling stats)
        ↓
Model inference
        ↓
Go relays the cached prediction — never recomputes anything
```

---

## Phase 6 — Go gRPC Client (Week 3–4)

**Goal:** Feed predictions into the Go backend — async, never blocking the hot path.

- Generate stubs: `protoc --go_out=. --go-grpc_out=. prediction.proto`
- Background goroutine: gRPC call → store in `sync/atomic.Value`
- Strategy reads `cachedPred.Load().(float64)` — zero blocking, zero waiting
- Per-call deadline: `context.WithTimeout(ctx, 10*time.Millisecond)`
- Fallback: if timeout or server down, `cachedPred = 0.5` (neutral signal)

The C++ strategy engine reads a cached float64 and never calls ML directly. If the ML service dies, trading continues uninterrupted with the last known cached prediction.

---

## Phase 7 — Monitoring & Retraining (Week 4+)

**Goal:** Detect model drift before it costs PnL — and alert on the metric that actually matters.

- Log every inference: `{timestamp, buy_prob, direction, feature_hash}`
- Log actual outcome N ticks later: `{timestamp, was_correct, realized_pnl}`
- **Alert on realized PnL and precision@signal on a rolling window — not a hardcoded accuracy threshold.** A model can be profitable at 51% accuracy and unprofitable at 60%, depending on which trades it's confident about.
- KL divergence on the `buy_prob` distribution vs. the training baseline, to catch drift before it shows up in PnL
- Weekly retrain cron on an expanding data window, saved as `model_v{n+1}.pkl` with a matching `metadata.json`
- Shadow test: run the new model alongside the old one for 24h before switching over

Concept drift is real in HFT — a model trained on calm markets degrades sharply once volatility spikes. The alerting threshold should be PnL-based, not accuracy-based.

---

## Rules That Apply Across Every Phase

1. **ML is async-only.** The hot path never waits on a prediction — it reads a cache.
2. **Never shuffle time-series data** for any train/test split or cross-validation fold.
3. **The proto definition is the contract** between the C++ feature producer and the Python model. Agree on feature order before Phase 5 starts, and write contract tests for it.
4. **Save the pipeline with the model, always.** `model.pkl` without `pipe.pkl` (and `metadata.json`) is not a deployable artifact.
5. **Feature computation lives in exactly one place** — next to the raw data it depends on. Never duplicated across languages.
6. **Prove the full pipeline end-to-end on 50k–100k ticks before scaling to 1M+.** This is the fastest path to a working system — optimize and scale only after every stage from exchange sim to gRPC prediction is connected and verified.
