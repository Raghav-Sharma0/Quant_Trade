#include <cassert>
#include <iostream>
#include <memory>
#include "../include/matching_engine/matching_engine.hpp"

using namespace hft;

int main() {
    printf("=== Matching Engine Tests ===\n");
    
    auto engine = std::make_unique<MatchingEngine>(1);
    
    MatchResult result1;
    Order buy(1, 100, 100, OrderSide::BUY, OrderType::LIMIT, 0, 1, 0);
    engine->submit_order(buy, result1, 0);
    assert(result1.fill_count == 0);
    printf("✓ Buy order added to book\n");
    
    MatchResult result2;
    Order sell(2, 100, 50, OrderSide::SELL, OrderType::LIMIT, 0, 2, 0);
    engine->submit_order(sell, result2, 0);
    assert(result2.fill_count == 1);
    assert(result2.reports[0].executed_price == 100);
    assert(result2.reports[0].executed_qty == 50);
    printf("✓ Partial match works\n");
    
    MatchResult result3;
    Order sell2(3, 100, 60, OrderSide::SELL, OrderType::LIMIT, 0, 2, 0);
    engine->submit_order(sell2, result3, 0);
    assert(result3.fill_count == 1);
    assert(result3.reports[0].executed_qty == 50);
    printf("✓ Remaining quantity added to book\n");
    
    auto md = engine->get_market_data(0);
    assert(md.best_ask_price == 100);
    assert(md.best_ask_qty == 10);
    printf("✓ Market data snapshot correct\n");
    
    printf("\nAll matching engine tests passed!\n");
    return 0;
} // namespace hft