# HFT Risk Engine & Latency Benchmarking Implementation Tasks

This task list outlines the remaining steps to build out the full **Risk Engine** checks and **Latency Percentiles** reporting within the C++ trading core.

---

## Phase 1: Risk Engine Enhancements — ✅ COMPLETE

- [x] **1. Link `RiskManager` with `PositionManager`**
  - `RiskManager` now takes `PositionManager&` in its constructor.
  - `check_order()` reads live `net_qty` and `total_pnl()` directly from `PositionManager`.
- [x] **2. Complete the `on_trade` Fill Tracking**
  - `on_trade()` forwards every fill to `PositionManager`, keeping `net_qty`, `avg_price`, `realized_pnl`, and `unrealized_pnl` accurate in real time.
- [x] **3. Implement the Kill Switch Rule**
  - Per-client `std::atomic<bool> kill_switch` added to `ClientLimits`.
  - Global `atomic<bool> global_kill_switch_` — blocks ALL clients instantly.
  - `trigger_kill_switch(id)`, `reset_kill_switch(id)`, `trigger_global_kill_switch()`, `reset_global_kill_switch()` implemented.
- [x] **4. Implement Price Collars (Collar Circuit Breaker)**
  - Per-symbol `ref_bid_[MAX_SYMBOLS]` and `ref_ask_[MAX_SYMBOLS]` stored in `RiskManager`.
  - `on_market_update(sym, bid, ask)` updates reference prices on every tick.
  - BUY rejected if `price > best_ask × (1 + collar_pct/100)`.
  - SELL rejected if `price < best_bid × (1 - collar_pct/100)`.
  - Configurable per client via `set_collar_pct(client_id, pct)`.
- [x] **5. Implement Sliding-Window Rate Limiting**
  - `SlidingWindowRateLimiter` with O(1) amortized circular timestamp buffer.
  - Uses `CLOCK_MONOTONIC_RAW` for nanosecond timestamps.
  - Configurable via `set_rate_limit(client_id, max_orders, window_ns)`.

> **Bonus:** Added `EXCEEDS_NOTIONAL` capital-based dollar exposure cap that automatically adapts to share price.

> **Full check_order() pipeline (cheapest → most expensive):**
> Kill Switch → Order Qty Cap → Rate Limiter → Price Collar → Notional Cap → Position Limit → Loss Cap

---

## Phase 2: Latency Percentiles — ✅ COMPLETE

- [x] **1. Build Latency Samples (zero-allocation hot path)**
  - Static `uint64_t g_samples[1'000'000]` — no heap allocation on hot path.
  - RDTSC with `_mm_lfence` serialisation barriers around each measurement.
  - Welford's online algorithm for numerically-stable mean and variance in a single pass.
- [x] **2. Implement Exact Percentile Calculation**
  - Samples sorted once (post-loop) via `std::sort`.
  - Exact **P50**, **P90**, **P95**, **P99**, **P99.9** from sorted array index lookup.
  - Also reports: Std Dev, Variance, Coefficient of Variation, Throughput (M checks/sec).
- [x] **3. Benchmark Exporters Updated**
  - Terminal histogram (linear buckets, P0–P99.9 range, bar chart).
  - Spike warning (Tukey fence outlier detection — flags spikes > 100× P99.9).
  - JSON export to `core-cpp/results/risk_bench_<timestamp>.json`.
  - Script `core-cpp/scripts/run_risk_bench.py` builds, runs, parses, and saves results.

---

## Phase 3: Validation and Verification — ✅ COMPLETE

- [x] **1. Update C++ Risk Tests (`tests/risk_test.cpp`) — 15 tests passing**
  - Normal order approved.
  - Oversized order rejected (`EXCEEDS_ORDER_QTY`).
  - Notional cap enforced (`EXCEEDS_NOTIONAL`).
  - Position limit via live `PositionManager` (`EXCEEDS_POSITION`).
  - Loss cap via live PnL (`EXCEEDS_LOSS`).
  - Per-client kill switch blocks + reset (`KILL_SWITCH`).
  - Global kill switch blocks all + reset.
  - Rate limiter blocks burst + recovers after window expires (`RATE_LIMIT`).
  - Price collar: BUY at market price approved; fat-finger BUY rejected (`PRICE_COLLAR`).
  - Price collar: SELL at market price approved; erroneous SELL rejected (`PRICE_COLLAR`).

- [x] **2. Benchmark Verified**
  - `bench_risk_engine` compiled with `-O3 -march=native -funroll-loops`.
  - Thread pinned to Core 2.
  - **P99 = 102.56 ns** → PASS (< 500 ns target).
  - **P99.9 = 410.26 ns** → PASS (< 1000 ns target).

- [x] **3. Price Collar Tests** — covered in Tests 9a–9d above.
