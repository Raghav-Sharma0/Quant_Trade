#pragma once

#include "../common/types.hpp"
#include "../common/constants.hpp"
#include "position_manager.hpp"
#include <vector>
#include <atomic>
#include <cstdint>
#include <ctime>       // clock_gettime

namespace hft {

// =============================================================================
//  RiskManager — Pre-trade risk gate integrated with live PositionManager.
//
//  Checks (in order of cheapest → most expensive):
//    1. Kill switch          → KILL_SWITCH       (atomic flag, ~1 ns)
//    2. Order quantity limit → EXCEEDS_ORDER_QTY (integer compare)
//    3. Rate limiter         → RATE_LIMIT        (sliding window, O(1) amortized)
//    4. Notional cap         → EXCEEDS_NOTIONAL  (price × qty dollar cap)
//    5. Share position limit → EXCEEDS_POSITION  (live qty from PositionManager)
//    6. Loss cap             → EXCEEDS_LOSS      (live PnL from PositionManager)
//
//  on_trade()         — call after every fill (forwards to PositionManager).
//  on_market_update() — call on every tick to refresh unrealised PnL.
// =============================================================================

// -----------------------------------------------------------------------------
//  SlidingWindowRateLimiter
//
//  Tracks the count of order submissions within a rolling time window using
//  a circular timestamp buffer.
//
//  Complexity: O(1) amortized — each entry is inserted once and evicted once.
//  Memory    : CAPACITY × 8 bytes per client (default = 2048 × 8 = 16 KB).
//
//  All fields are mutable so the limiter can be updated inside a const
//  check_order() — this is intentional; the rate state IS the side-effect.
//
//  Real-world analogy:
//    Like a bouncer at a nightclub with a clicker — they count how many people
//    entered in the last 60 seconds. If the count hits the limit, the next
//    person is turned away until someone from an hour ago "falls off" the list.
// -----------------------------------------------------------------------------
struct SlidingWindowRateLimiter {
    static constexpr uint32_t CAPACITY = 4096; // max trackable orders per window

    mutable uint64_t timestamps[CAPACITY] {};  // circular buffer of order ns timestamps
    mutable uint32_t head     = 0;             // oldest entry (evict from here)
    mutable uint32_t tail     = 0;             // next write position
    mutable uint32_t count    = 0;             // entries currently inside the window

    uint32_t max_orders = 1000;                // max orders allowed per window
    uint64_t window_ns  = 1'000'000'000ULL;   // window size (default = 1 second)

    // Returns true if the order is within the rate limit, false if rejected.
    // Records the timestamp if allowed.
    [[nodiscard]]
    bool check_and_record(uint64_t now_ns) const noexcept {
        // Step 1: Evict entries older than the window from the front.
        // Because timestamps are added in chronological order, oldest is at head.
        while (count > 0 && (now_ns - timestamps[head]) >= window_ns) {
            head = (head + 1) % CAPACITY;
            --count;
        }

        // Step 2: Reject if at the limit.
        if (count >= max_orders) {
            return false;
        }

        // Step 3: Record this order's timestamp in the circular buffer.
        timestamps[tail] = now_ns;
        tail  = (tail + 1) % CAPACITY;
        ++count;
        return true;
    }
};

class RiskManager {
private:
    // -------------------------------------------------------------------------
    // Per-client configuration limits + rate limiter state.
    // -------------------------------------------------------------------------
    struct ClientLimits {
        int64_t  max_position  = DEFAULT_MAX_POSITION;
        int64_t  max_notional  = 10'000'000;          // $10M default capital cap
        int64_t  max_loss      = DEFAULT_MAX_LOSS;
        uint32_t max_order_qty = DEFAULT_MAX_ORDER_QTY;
        uint32_t collar_pct    = DEFAULT_PRICE_COLLAR_PCT; // % away from market price

        std::atomic<bool>        kill_switch {false};
        SlidingWindowRateLimiter rate_limiter {};

        // atomic + large array — not copyable
        ClientLimits() = default;
        ClientLimits(const ClientLimits&) = delete;
        ClientLimits& operator=(const ClientLimits&) = delete;
    };

    std::vector<ClientLimits> limits_;
    std::atomic<bool>         global_kill_switch_ {false};

    // Per-symbol reference bid/ask prices — updated on every market tick.
    // Used by the price collar check to detect fat-finger / erroneous orders.
    // Price = 0 means no reference has been set yet (collar check is skipped).
    Price ref_bid_[MAX_SYMBOLS] {};
    Price ref_ask_[MAX_SYMBOLS] {};

    PositionManager& pos_mgr_;

    // -------------------------------------------------------------------------
    // Monotonic nanosecond clock — used by rate limiter.
    // CLOCK_MONOTONIC_RAW: unaffected by NTP adjustments.
    // -------------------------------------------------------------------------
    static uint64_t now_ns() noexcept {
        struct timespec ts {};
        clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
        return static_cast<uint64_t>(ts.tv_sec) * 1'000'000'000ULL
               + static_cast<uint64_t>(ts.tv_nsec);
    }

public:
    enum class RiskCheckResult : uint8_t {
        APPROVED          = 0,
        KILL_SWITCH       = 1,
        EXCEEDS_ORDER_QTY = 2,
        RATE_LIMIT        = 3,   // too many orders per second
        PRICE_COLLAR      = 4,   // fat-finger: price too far from market
        EXCEEDS_NOTIONAL  = 5,
        EXCEEDS_POSITION  = 6,
        EXCEEDS_LOSS      = 7,
    };

    explicit RiskManager(PositionManager& pos_mgr,
                         std::size_t      max_clients = 10'000)
        : limits_(max_clients), pos_mgr_(pos_mgr)
    {}

    ~RiskManager() = default;

    RiskManager(const RiskManager&)            = delete;
    RiskManager& operator=(const RiskManager&) = delete;

    // -------------------------------------------------------------------------
    // check_order() — O(1) amortized pre-trade gate.
    //
    // NOTE: The rate limiter records the timestamp of every APPROVED order
    // as a side-effect inside this call (hence the mutable fields).
    // -------------------------------------------------------------------------
    [[nodiscard]]
    RiskCheckResult check_order(const Order& order, SymbolId sym) const noexcept {

        // ── 1. Global kill switch (~1 ns) ─────────────────────────────────────
        if (global_kill_switch_.load(std::memory_order_relaxed)) {
            return RiskCheckResult::KILL_SWITCH;
        }

        if (order.client_id >= limits_.size()) {
            return RiskCheckResult::APPROVED;
        }

        const auto& lim = limits_[order.client_id];

        // ── 2. Per-client kill switch ─────────────────────────────────────────
        if (lim.kill_switch.load(std::memory_order_relaxed)) {
            return RiskCheckResult::KILL_SWITCH;
        }

        // ── 3. Single-order quantity cap ──────────────────────────────────────
        if (order.quantity > lim.max_order_qty) {
            return RiskCheckResult::EXCEEDS_ORDER_QTY;
        }

        // ── 4. Rate limiter (sliding window, O(1) amortized) ──────────────────
        // Records timestamp on success — this is the only "write" in check_order.
        // Real-world use: prevents order-spam bots from hitting the exchange
        // and triggering SEC Rule 15c3-5 compliance violations.
        if (!lim.rate_limiter.check_and_record(now_ns())) {
            return RiskCheckResult::RATE_LIMIT;
        }

        // ── 5. Price Collar (circuit breaker) ─────────────────────────────────
        // Rejects orders priced too far from the live best bid/ask.
        // Protects against fat-finger errors (e.g. $2000 instead of $200).
        //
        //   BUY  must not pay more than:  best_ask × (100 + collar_pct) / 100
        //   SELL must not receive less:   best_bid × (100 - collar_pct) / 100
        //
        // Skipped if no reference price has been set yet (ref = 0).
        if (sym < MAX_SYMBOLS && lim.collar_pct > 0) {
            if (order.is_buy()) {
                const Price ref = ref_ask_[sym];
                if (ref > 0) {
                    const Price ceiling = ref + (ref * lim.collar_pct) / 100;
                    if (order.price > ceiling) {
                        return RiskCheckResult::PRICE_COLLAR;
                    }
                }
            } else {
                const Price ref = ref_bid_[sym];
                if (ref > 0) {
                    const Price floor_price = ref - (ref * lim.collar_pct) / 100;
                    if (order.price < floor_price) {
                        return RiskCheckResult::PRICE_COLLAR;
                    }
                }
            }
        }

        // ── 5. Notional (capital) cap ─────────────────────────────────────────
        const int64_t live_net     = pos_mgr_.get_net_qty(order.client_id, sym);
        const int64_t delta        = order.is_buy()
                                     ? static_cast<int64_t>(order.quantity)
                                     : -static_cast<int64_t>(order.quantity);
        const int64_t proj_net     = live_net + delta;
        const int64_t abs_proj_net = proj_net >= 0 ? proj_net : -proj_net;
        const int64_t proj_notional = abs_proj_net * static_cast<int64_t>(order.price);

        if (proj_notional > lim.max_notional) {
            return RiskCheckResult::EXCEEDS_NOTIONAL;
        }

        // ── 6. Raw share position cap ─────────────────────────────────────────
        if (proj_net >  lim.max_position ||
            proj_net < -lim.max_position) {
            return RiskCheckResult::EXCEEDS_POSITION;
        }

        // ── 7. Loss cap ───────────────────────────────────────────────────────
        const int64_t total_pnl =
            pos_mgr_.get_position(order.client_id, sym).total_pnl();
        if (-total_pnl >= lim.max_loss) {
            return RiskCheckResult::EXCEEDS_LOSS;
        }

        return RiskCheckResult::APPROVED;
    }

    // -------------------------------------------------------------------------
    // Trade / market update forwarding
    // -------------------------------------------------------------------------
    void on_trade(ClientId  client_id, SymbolId sym,
                  OrderSide side,      Price    price,
                  Quantity  qty) noexcept {
        pos_mgr_.on_trade(client_id, sym, side, price, qty);
    }

    // Update PnL mark-to-market AND reference prices for price collar.
    // Call this on every market tick.
    //   sym          — symbol index
    //   bid          — current best bid price
    //   ask          — current best ask price
    //   market_price — typically (bid+ask)/2 used for unrealised PnL
    void on_market_update(SymbolId sym, Price bid, Price ask) noexcept {
        if (sym < MAX_SYMBOLS) {
            ref_bid_[sym] = bid;
            ref_ask_[sym] = ask;
        }
        // Use ask as the mark price for PnL (conservative for long positions).
        const Price mark = ask > 0 ? ask : bid;
        pos_mgr_.mark_all(sym, mark);
    }

    // =========================================================================
    //  Kill Switch controls
    // =========================================================================
    void trigger_global_kill_switch() noexcept {
        global_kill_switch_.store(true, std::memory_order_release);
    }
    void trigger_kill_switch(ClientId id) noexcept {
        if (id < limits_.size())
            limits_[id].kill_switch.store(true, std::memory_order_release);
    }
    void reset_kill_switch(ClientId id) noexcept {
        if (id < limits_.size())
            limits_[id].kill_switch.store(false, std::memory_order_release);
    }
    void reset_global_kill_switch() noexcept {
        global_kill_switch_.store(false, std::memory_order_release);
    }

    // =========================================================================
    //  Rate limiter configuration
    // =========================================================================

    // Set the rate limit for a client.
    //   max_orders_per_window — e.g. 500 (orders)
    //   window_ns             — e.g. 1,000,000,000 (1 second)
    void set_rate_limit(ClientId client_id,
                        uint32_t max_orders_per_window,
                        uint64_t window_ns = 1'000'000'000ULL) noexcept {
        if (client_id < limits_.size()) {
            auto& rl        = limits_[client_id].rate_limiter;
            rl.max_orders   = max_orders_per_window;
            rl.window_ns    = window_ns;
        }
    }

    // Set price collar percentage for a client.
    //   collar_pct = 5 means orders rejected if price is >5% away from market.
    //   Set to 0 to disable the collar check for this client.
    void set_collar_pct(ClientId client_id, uint32_t collar_pct) noexcept {
        if (client_id < limits_.size()) {
            limits_[client_id].collar_pct = collar_pct;
        }
    }

    // =========================================================================
    //  Limit configuration
    // =========================================================================
    void set_limit(ClientId client_id,
                   int64_t  max_pos,
                   int64_t  max_notional,
                   int64_t  max_loss,
                   uint32_t max_qty = DEFAULT_MAX_ORDER_QTY) noexcept {
        if (client_id < limits_.size()) {
            auto& l         = limits_[client_id];
            l.max_position  = max_pos;
            l.max_notional  = max_notional;
            l.max_loss      = max_loss;
            l.max_order_qty = max_qty;
        }
    }

    void reset_client(ClientId client_id) noexcept {
        pos_mgr_.reset_client(client_id);
    }
};

} // namespace hft