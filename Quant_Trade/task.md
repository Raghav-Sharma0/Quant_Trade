# HFT Risk Engine & Latency Benchmarking Implementation Tasks

This task list outlines the remaining steps to build out the full **Risk Engine** checks and **Latency Percentiles** reporting within the C++ trading core.

---

## Phase 1: Risk Engine Enhancements

- [x] **1. Link `RiskManager` with `PositionManager`**
  - `RiskManager` now takes `PositionManager&` in its constructor.
  - `check_order()` reads live `net_qty` and `total_pnl()` directly from `PositionManager`.
- [x] **2. Complete the `on_trade` Fill Tracking**
  - `on_trade()` now forwards every fill to `PositionManager`, keeping `net_qty`, `avg_price`, `realized_pnl`, and `unrealized_pnl` accurate in real time.
- [x] **3. Implement the Kill Switch Rule**
  - Per-client `std::atomic<bool> kill_switch` added to `ClientLimits`.
  - Global `atomic<bool> global_kill_switch_` added — blocks ALL clients instantly.
  - `trigger_kill_switch(id)`, `reset_kill_switch(id)`, `trigger_global_kill_switch()`, `reset_global_kill_switch()` all implemented.
- [ ] **4. Implement Price Collars (Collar Circuit Breaker)**
  - Add a rule to reject orders priced too far from the reference market price.
  - Reject order if: `price > best_ask * (1 + collar)` or `price < best_bid * (1 - collar)`.
- [x] **5. Implement Sliding-Window Rate Limiting**
  - `SlidingWindowRateLimiter` added as a standalone struct with a fixed-size circular timestamp buffer.
  - O(1) amortized check using `CLOCK_MONOTONIC_RAW`.
  - Configurable per client via `set_rate_limit(client_id, max_orders, window_ns)`.

> **Bonus:** Added `EXCEEDS_NOTIONAL` capital cap — rejects orders where projected dollar exposure exceeds a dollar limit. This automatically adapts to share price (e.g., $200,000 cap = 1,000 shares of $200 stock vs 20,000 shares of a $10 stock).

---

## Phase 2: Latency Percentiles (P50 - P99.9)

- [ ] **1. Build Latency Histograms / Samples**
  - Add a latency sampler to record individual execution timings (in CPU cycles or nanoseconds).
  - Implement a histogram bucket classifier or a sorted buffer to compute percentiles without allocating memory on the hot path.
- [ ] **2. Implement Percentile Calculation**
  - Add functions to compute **P50**, **P90**, **P95**, **P99**, and **P99.9** metrics from the recorded samples.
- [ ] **3. Update Benchmark Exporters**
  - Update `core-cpp/results/` exporters to format and output these percentile results in the output JSON.

---

## Phase 3: Validation and Verification

- [x] **1. Update C++ Risk Tests (`tests/risk_test.cpp`)**
  - 11 tests passing:
    - Normal order approved.
    - Oversized order rejected (`EXCEEDS_ORDER_QTY`).
    - Notional cap enforced (`EXCEEDS_NOTIONAL`).
    - Position limit via live `PositionManager` (`EXCEEDS_POSITION`).
    - Loss cap via live PnL (`EXCEEDS_LOSS`).
    - Per-client kill switch blocks + reset (`KILL_SWITCH`).
    - Global kill switch blocks all + reset.
    - Rate limiter blocks burst + recovers after window expires (`RATE_LIMIT`).
- [ ] **2. Verify Benchmark Runs**
  - Compile and run tests to ensure all C++ risk checks execute in **under 100 nanoseconds**.
- [ ] **3. Price Collar Tests**
  - Write test assertions verifying price collar rejection once Task 4 is implemented.
