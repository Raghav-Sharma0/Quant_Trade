#include <cassert>
#include <cstdio>
#include <memory>
#include <ctime>    // nanosleep

#include "../include/risk/position_manager.hpp"
#include "../include/risk/risk_manager.hpp"

using namespace hft;

int main() {
    printf("=== Risk Manager Tests (Phase 1 + Kill Switch + Notional Cap) ===\n\n");

    // Allocate PositionManager on HEAP (10000×1024 array = ~560 MB, too big for stack)
    auto pos_ptr = std::make_unique<PositionManager>();
    PositionManager& pos = *pos_ptr;
    RiskManager risk(pos);

    const SymbolId SYM = 0;
    const ClientId CLI = 1;

    // ------------------------------------------------------------------
    // Set limits for CLI:
    //   max_position  = 1,000 shares
    //   max_notional  = $200,000   (dollar exposure cap)
    //   max_loss      = $50,000
    //   max_order_qty = 500 shares
    // ------------------------------------------------------------------
    risk.set_limit(CLI,
        /*max_pos=*/     1'000,
        /*max_notional=*/200'000,
        /*max_loss=*/     50'000,
        /*max_qty=*/         500);

    // ── Test 1: Normal order approved ────────────────────────────────────
    // BUY 100 @ price 100  → notional = $10,000  → well within all limits
    Order o1(1, /*price*/100, /*qty*/100, OrderSide::BUY, OrderType::LIMIT, SYM, CLI, 0);
    assert(risk.check_order(o1, SYM) == RiskManager::RiskCheckResult::APPROVED);
    printf("PASS  Test 1: Normal order approved\n");

    // ── Test 2: Single order exceeds max_order_qty (500) ─────────────────
    Order o2(2, 100, 501, OrderSide::BUY, OrderType::LIMIT, SYM, CLI, 0);
    assert(risk.check_order(o2, SYM) == RiskManager::RiskCheckResult::EXCEEDS_ORDER_QTY);
    printf("PASS  Test 2: Oversized order rejected (EXCEEDS_ORDER_QTY)\n");

    // ── Test 3: Notional (capital) cap ───────────────────────────────────
    // max_notional = $200,000
    // BUY 400 @ price $200 = $80,000 notional (fine)
    // Another BUY 400 @ price $200 would project to 800 × $200 = $160,000 (fine)
    // BUY 500 @ price $500 = 500 × $500 = $250,000 → exceeds $200,000 cap
    Order o3(3, /*price*/500, /*qty*/500, OrderSide::BUY, OrderType::LIMIT, SYM, CLI, 0);
    assert(risk.check_order(o3, SYM) == RiskManager::RiskCheckResult::EXCEEDS_NOTIONAL);
    printf("PASS  Test 3: Notional cap enforced — $250k > $200k limit (EXCEEDS_NOTIONAL)\n");

    // ── Test 4: Position limit check via live PositionManager ────────────
    // max_position = 1,000 shares
    // Simulate 800 shares already filled
    risk.on_trade(CLI, SYM, OrderSide::BUY, 100, 800);
    assert(pos.get_net_qty(CLI, SYM) == 800);

    // BUY 300 more @ price $100 → projected net = 1,100 > 1,000 limit
    Order o4(4, 100, 300, OrderSide::BUY, OrderType::LIMIT, SYM, CLI, 0);
    assert(risk.check_order(o4, SYM) == RiskManager::RiskCheckResult::EXCEEDS_POSITION);
    printf("PASS  Test 4: Share position limit via live PositionManager (EXCEEDS_POSITION)\n");

    // ── Test 5: Loss cap via live PnL ────────────────────────────────────
    const ClientId CLI2 = 2;
    risk.set_limit(CLI2, 1'000'000, 10'000'000, /*max_loss=*/50, 1'000'000);

    // Buy 100 @ price 200 → avg_cost = 200
    risk.on_trade(CLI2, SYM, OrderSide::BUY, 200, 100);
    // Market drops to 100 → unrealised loss = (100-200)*100 = -10,000
    risk.on_market_update(SYM, /*bid=*/99, /*ask=*/100);

    Order o5(5, 100, 1, OrderSide::BUY, OrderType::LIMIT, SYM, CLI2, 0);
    assert(risk.check_order(o5, SYM) == RiskManager::RiskCheckResult::EXCEEDS_LOSS);
    printf("PASS  Test 5: Loss cap via live PnL (EXCEEDS_LOSS)\n");

    // ── Test 6: Per-client Kill Switch ───────────────────────────────────
    const ClientId CLI3 = 3;
    risk.set_limit(CLI3, 1'000'000, 10'000'000, 1'000'000, 1'000'000);

    Order o6_before(6, 100, 10, OrderSide::BUY, OrderType::LIMIT, SYM, CLI3, 0);
    assert(risk.check_order(o6_before, SYM) == RiskManager::RiskCheckResult::APPROVED);

    // Trigger kill switch for CLI3
    risk.trigger_kill_switch(CLI3);

    Order o6_after(7, 100, 10, OrderSide::BUY, OrderType::LIMIT, SYM, CLI3, 0);
    assert(risk.check_order(o6_after, SYM) == RiskManager::RiskCheckResult::KILL_SWITCH);
    printf("PASS  Test 6: Per-client kill switch blocks orders (KILL_SWITCH)\n");

    // Reset and verify trading resumes
    risk.reset_kill_switch(CLI3);
    assert(risk.check_order(o6_before, SYM) == RiskManager::RiskCheckResult::APPROVED);
    printf("PASS  Test 6b: Kill switch reset — trading resumed\n");

    // ── Test 7: Global Kill Switch ───────────────────────────────────────
    risk.trigger_global_kill_switch();

    // ALL clients blocked — even one with no other limits hit
    Order o7(8, 100, 1, OrderSide::BUY, OrderType::LIMIT, SYM, CLI3, 0);
    assert(risk.check_order(o7, SYM) == RiskManager::RiskCheckResult::KILL_SWITCH);
    printf("PASS  Test 7: Global kill switch blocks all clients\n");

    risk.reset_global_kill_switch();
    assert(risk.check_order(o7, SYM) == RiskManager::RiskCheckResult::APPROVED);
    printf("PASS  Test 7b: Global kill switch reset — trading resumed\n");

    // ── Test 8: Rate Limiter (sliding window) ───────────────────────────────
    // Set max 3 orders per 500ms window for CLI4
    const ClientId CLI4 = 4;
    risk.set_limit(CLI4, 1'000'000, 100'000'000, 1'000'000, 1'000'000);
    risk.set_rate_limit(CLI4, /*max_orders=*/3, /*window_ns=*/500'000'000ULL); // 3 per 500ms

    Order o8a(10, 100, 1, OrderSide::BUY, OrderType::LIMIT, SYM, CLI4, 0);
    Order o8b(11, 100, 1, OrderSide::BUY, OrderType::LIMIT, SYM, CLI4, 0);
    Order o8c(12, 100, 1, OrderSide::BUY, OrderType::LIMIT, SYM, CLI4, 0);
    Order o8d(13, 100, 1, OrderSide::BUY, OrderType::LIMIT, SYM, CLI4, 0); // this should be blocked

    assert(risk.check_order(o8a, SYM) == RiskManager::RiskCheckResult::APPROVED);
    assert(risk.check_order(o8b, SYM) == RiskManager::RiskCheckResult::APPROVED);
    assert(risk.check_order(o8c, SYM) == RiskManager::RiskCheckResult::APPROVED);
    assert(risk.check_order(o8d, SYM) == RiskManager::RiskCheckResult::RATE_LIMIT);
    printf("PASS  Test 8: Rate limiter blocks 4th order (3/500ms window) (RATE_LIMIT)\n");

    // Wait for the window to expire, then orders should be allowed again
    struct timespec sleep_ts {0, 550'000'000L}; // 550ms
    nanosleep(&sleep_ts, nullptr);

    assert(risk.check_order(o8a, SYM) == RiskManager::RiskCheckResult::APPROVED);
    printf("PASS  Test 8b: After window expires, orders approved again\n");

    // ── Test 9: Price Collar ─────────────────────────────────────────────
    // Market: bid=195, ask=200. Collar = 5%.
    //   BUY ceiling  = 200 * 1.05 = 210 → any BUY price > 210 is rejected.
    //   SELL floor   = 195 * 0.95 = ~185 → any SELL price < 185 is rejected.
    const ClientId CLI5 = 5;
    risk.set_limit(CLI5, 1'000'000, 100'000'000, 1'000'000, 1'000'000);
    risk.set_collar_pct(CLI5, 5);  // 5% price collar
    risk.on_market_update(SYM, /*bid=*/195, /*ask=*/200);

    // Test 9a: Normal BUY at $205 (within 5% of $200 ask) → approved
    Order o9a(20, /*price*/205, /*qty*/1, OrderSide::BUY, OrderType::LIMIT, SYM, CLI5, 0);
    assert(risk.check_order(o9a, SYM) == RiskManager::RiskCheckResult::APPROVED);
    printf("PASS  Test 9a: BUY at $205 (within 5%% collar of $200 ask) approved\n");

    // Test 9b: Fat-finger BUY at $2000 (10x the ask) → PRICE_COLLAR
    Order o9b(21, /*price*/2000, /*qty*/1, OrderSide::BUY, OrderType::LIMIT, SYM, CLI5, 0);
    assert(risk.check_order(o9b, SYM) == RiskManager::RiskCheckResult::PRICE_COLLAR);
    printf("PASS  Test 9b: Fat-finger BUY at $2000 rejected (PRICE_COLLAR)\n");

    // Test 9c: Normal SELL at $190 (within 5% of $195 bid) → approved
    Order o9c(22, /*price*/190, /*qty*/1, OrderSide::SELL, OrderType::LIMIT, SYM, CLI5, 0);
    assert(risk.check_order(o9c, SYM) == RiskManager::RiskCheckResult::APPROVED);
    printf("PASS  Test 9c: SELL at $190 (within 5%% collar of $195 bid) approved\n");

    // Test 9d: Erroneous SELL at $1 (far below bid) → PRICE_COLLAR
    Order o9d(23, /*price*/1, /*qty*/1, OrderSide::SELL, OrderType::LIMIT, SYM, CLI5, 0);
    assert(risk.check_order(o9d, SYM) == RiskManager::RiskCheckResult::PRICE_COLLAR);
    printf("PASS  Test 9d: Erroneous SELL at $1 rejected (PRICE_COLLAR)\n");

    printf("\nAll Phase 1 + Kill Switch + Notional Cap + Rate Limiter + Price Collar tests passed!\n");
    return 0;
}