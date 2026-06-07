#include <cassert>
#include <iostream>
#include "../include/order_book/order_book.hpp"

using namespace hft;

int main() {
    printf("=== Order Book Tests ===\n");
    
    OrderBook book(1);
    ObjectPool<OrderNode, 1000> node_pool;
    
    Order o_buy1(1, 100, 50, OrderSide::BUY, OrderType::LIMIT, 1, 1, 0);
    OrderNode* buy1 = node_pool.construct();
    buy1->from_order(o_buy1);

    Order o_buy2(2, 99, 100, OrderSide::BUY, OrderType::LIMIT, 1, 1, 0);
    OrderNode* buy2 = node_pool.construct();
    buy2->from_order(o_buy2);

    Order o_sell1(3, 101, 75, OrderSide::SELL, OrderType::LIMIT, 1, 2, 0);
    OrderNode* sell1 = node_pool.construct();
    sell1->from_order(o_sell1);

    Order o_sell2(4, 102, 25, OrderSide::SELL, OrderType::LIMIT, 1, 2, 0);
    OrderNode* sell2 = node_pool.construct();
    sell2->from_order(o_sell2);
    
    book.add_order(buy1);
    book.add_order(buy2);
    book.add_order(sell1);
    book.add_order(sell2);
    
    auto snap = book.snapshot();
    assert(snap.best_bid_price == 100);
    assert(snap.best_ask_price == 101);
    assert(snap.best_bid_qty == 50);
    assert(snap.best_ask_qty == 75);
    printf("✓ Market snapshot correct\n");
    
    assert(book.bid_levels() == 2);
    assert(book.ask_levels() == 2);
    printf("✓ Book depth correct\n");
    
    OrderNode* removed = book.cancel_order(1);
    assert(removed == buy1);
    node_pool.destroy(removed);
    assert(book.bid_levels() == 1);
    printf("✓ Remove order works\n");
    
    snap = book.snapshot();
    assert(snap.best_bid_price == 99);
    printf("✓ Best price updated after removal\n");
    
    book.clear();
    assert(book.bid_levels() == 0);
    assert(book.ask_levels() == 0);
    printf("✓ Clear works\n");
    
    printf("\nAll order book tests passed!\n");
    return 0;
} // namespace hft

