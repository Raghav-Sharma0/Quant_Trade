#include <cassert>
#include <iostream>
#include "../include/order_book/order_book.hpp"

using namespace hft;

int main() {
    printf("=== Order Book Tests ===\n");
    
    OrderBook book(1);
    
    Order buy1(1, 100, 50, OrderSide::BUY, 1);
    Order buy2(2, 99, 100, OrderSide::BUY, 1);
    Order sell1(3, 101, 75, OrderSide::SELL, 2);
    Order sell2(4, 102, 25, OrderSide::SELL, 2);
    
    book.add_order(buy1);
    book.add_order(buy2);
    book.add_order(sell1);
    book.add_order(sell2);
    
    auto snap = book.get_snapshot();
    assert(snap.best_bid_price == 100);
    assert(snap.best_ask_price == 101);
    assert(snap.best_bid_qty == 50);
    assert(snap.best_ask_qty == 75);
    printf("✓ Market snapshot correct\n");
    
    assert(book.buy_depth() == 2);
    assert(book.sell_depth() == 2);
    printf("✓ Book depth correct\n");
    
    bool removed = book.remove_order(buy1);
    assert(removed == true);
    assert(book.buy_depth() == 1);
    printf("✓ Remove order works\n");
    
    snap = book.get_snapshot();
    assert(snap.best_bid_price == 99);
    printf("✓ Best price updated after removal\n");
    
    book.clear();
    assert(book.buy_depth() == 0);
    assert(book.sell_depth() == 0);
    printf("✓ Clear works\n");
    
    printf("\nAll order book tests passed!\n");
    return 0;
} // namespace hft

