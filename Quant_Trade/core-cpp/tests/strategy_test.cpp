#include <cassert>
#include <iostream>
#include "../include/strategy/strategy.hpp"

using namespace hft;

int main() {
    printf("=== Strategy Tests ===\n");
    
    MarketData md(0);
    md.best_bid_price = 100;
    md.best_ask_price = 105;
    md.best_bid_qty = 1000;
    md.best_ask_qty = 1000;
    md.symbol_id = 0;
    
    SimpleSpreadStrategy strategy(1);
    
    auto order1 = strategy.generate_order(md);
    assert(order1.has_value());
    assert(order1->side == static_cast<uint8_t>(OrderSide::BUY));
    printf("✓ Strategy generates buy order on wide spread\n");
    
    MarketData tight_md(0);
    tight_md.best_bid_price = 100;
    tight_md.best_ask_price = 101;
    tight_md.best_bid_qty = 1000;
    tight_md.best_ask_qty = 1000;
    tight_md.symbol_id = 0;
    
    auto order2 = strategy.generate_order(tight_md);
    assert(!order2.has_value());
    printf("✓ Strategy skips tight spread\n");
    
    printf("\nAll strategy tests passed!\n");
    return 0;
} // namespace hft