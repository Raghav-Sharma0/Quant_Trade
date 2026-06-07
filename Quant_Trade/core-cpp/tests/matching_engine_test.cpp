#include <cassert>
#include <iostream>
#include "../include/matching_engine/matching_engine.hpp"

using namespace hft;

int main() {
    printf("=== Matching Engine Tests ===\n");
    
    MatchingEngine engine(1);
    
    Order buy(1, 100, 100, OrderSide::BUY, 0);
    auto result1 = engine.match_order(buy);
    assert(result1.trades.empty());
    assert(result1.remaining_qty == 100);
    printf("✓ Buy order added to book\n");
    
    Order sell(2, 100, 50, OrderSide::SELL, 0);
    auto result2 = engine.match_order(sell);
    assert(result2.trades.size() == 1);
    assert(result2.trades[0].price == 100);
    assert(result2.trades[0].quantity == 50);
    assert(result2.remaining_qty == 0);
    printf("✓ Partial match works\n");
    
    Order sell2(3, 100, 60, OrderSide::SELL, 0);
    auto result3 = engine.match_order(sell2);
    assert(result3.trades.size() == 1);
    assert(result3.trades[0].quantity == 50);
    assert(result3.remaining_qty == 10);
    printf("✓ Remaining quantity added to book\n");
    
    auto md = engine.get_market_data(0);
    assert(md.best_ask_price == 100);
    assert(md.best_ask_qty == 10);
    printf("✓ Market data snapshot correct\n");
    
    printf("\nAll matching engine tests passed!\n");
    return 0;
} // namespace hft