#include <iostream>
#include <vector>
#include <x86intrin.h>
#include "../include/order_book/order_book.hpp"
#include "../include/common/types.hpp"
#include "../include/common/cacheline.hpp"
#include "../include/memory/object_pool.hpp"

using namespace hft;

int main() {
    OrderBook book(0);
    ObjectPool<OrderNode, 10000> node_pool;
    LatencyStats add_stats, lookup_stats, remove_stats;
    
    const int NUM_ORDERS = 10000;
    std::vector<OrderNode*> orders;
    
    for (int i = 0; i < NUM_ORDERS; ++i) {
        Order o(i, 100 + (i % 50), 100, i % 2 == 0 ? OrderSide::BUY : OrderSide::SELL, OrderType::LIMIT, 0, 1, 0);
        OrderNode* node = node_pool.construct();
        node->from_order(o);
        orders.push_back(node);
    }
    
    for (const auto& order : orders) {
        _mm_lfence();
        uint64_t start = hft::rdtsc();
        book.cancel_order(order->order_id);
        uint64_t end = hft::rdtscp();
        _mm_lfence();
        add_stats.record(end - start);
    }
    
    _mm_lfence();
    uint64_t start = __rdtsc();
    auto snap = book.snapshot();
    uint64_t end = __rdtscp(nullptr);
    _mm_lfence();
    lookup_stats.record(end - start);
    
    printf("=== Order Book Benchmark ===\n");
    printf("Add Order:\n");
    printf("  Min: %.2f ns\n", add_stats.min_cycles / 3.0);
    printf("  Max: %.2f ns\n", add_stats.max_cycles / 3.0);
    printf("  Avg: %.2f ns\n", add_stats.avg_ns(3.0));
    
    printf("\nMarket Snapshot:\n");
    printf("  Cycles: %.0f\n", lookup_stats.avg_cycles());
    printf("  Time: %.2f ns\n", lookup_stats.avg_ns(3.0));
    printf("  Best Bid: %u @ %u\n", snap.best_bid_qty, snap.best_bid_price);
    printf("  Best Ask: %u @ %u\n", snap.best_ask_qty, snap.best_ask_price);
    
    printf("\nBook Depth: BUY=%zu SELL=%zu\n", book.bid_levels(), book.ask_levels());
    
    return 0;
} // namespace hft