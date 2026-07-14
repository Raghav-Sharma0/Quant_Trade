# HFT Risk Engine & Latency Benchmarking Implementation Tasks

This task list outlines the remaining steps to build out the full **Risk Engine** checks and **Latency Percentiles** reporting within the C++ trading core.

---

## 📋 Phase 1: Risk Engine Enhancements

- [ ] **1. Link `RiskManager` with `PositionManager`**
  - Integrate `PositionManager` as a reference or member inside `RiskManager`.
  - Replace static/placeholder limit assertions with real-time exposure checks.
- [ ] **2. Complete the `on_trade` Fill Tracking**
  - Implement the body of `RiskManager::on_trade(client_id, trade)` to update client limits, current exposure, and current loss on filled positions.
- [ ] **3. Implement the Kill Switch Rule**
  - Add a boolean flag `kill_switch_active` (both globally and per-client).
  - Add check logic: immediately reject orders with `RejectReason::RISK_KILL_SWITCH` if the switch is active.
  - Implement a setter `set_kill_switch(client_id, status)`.
- [ ] **4. Implement Price Collars (Collar Circuit Breaker)**
  - Add a rule to reject orders priced too far from the reference market price.
  - Reject order if: `price > best_ask * (1 + collar)` or `price < best_bid * (1 - collar)`.
- [ ] **5. Implement Sliding-Window Rate Limiting**
  - Track timestamps of recently placed orders per client.
  - Reject orders if the count in the last `1 second` exceeds the allowed threshold (e.g., max 1000 orders/sec).

---

## 📋 Phase 2: Latency Percentiles (P50 - P99.9)

- [ ] **1. Build Latency Histograms / Samples**
  - Add a latency sampler to record individual execution timings (in CPU cycles or nanoseconds).
  - Implement a histogram bucket classifier or a sorted buffer to compute percentiles without allocating memory on the hot path.
- [ ] **2. Implement Percentile Calculation**
  - Add functions to compute **P50**, **P90**, **P95**, **P99**, and **P99.9** metrics from the recorded samples.
- [ ] **3. Update Benchmark Exporters**
  - Update `core-cpp/results/` exporters to format and output these percentile results in the output JSON.

---

## 📋 Phase 3: Validation and Verification

- [ ] **1. Update C++ Risk Tests (`tests/risk_test.cpp`)**
  - Write test assertions verifying:
    - Position/loss limit blocks.
    - Kill switch activation and blocks.
    - Rate limiter blocking burst traffic.
    - Price collar rejection.
- [ ] **2. Verify Benchmark Runs**
  - Compile and run tests to ensure all C++ risk checks execute in **under 100 nanoseconds**.
