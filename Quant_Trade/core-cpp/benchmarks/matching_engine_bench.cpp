#include <iostream>
#include <x86intrin.h>
#include <memory>
#include "../include/matching_engine/matching_engine.hpp"
#include "../include/common/types.hpp"
#include "../include/common/cacheline.hpp"

using namespace hft;

int main() {
    auto engine = std::make_unique<MatchingEngine>(1);
    LatencyStats match_stats;
    
    const int NUM_MATCHES = 10000;
    
    MatchResult result;
    
    for (int i = 0; i < NUM_MATCHES / 2; ++i) {
        Order buy(i, 100, 100, OrderSide::BUY, OrderType::LIMIT, 0, 1, 0);
        engine->submit_order(buy, result, 0);
    }
    
    for (int i = NUM_MATCHES / 2; i < NUM_MATCHES; ++i) {
        Order sell(i, 100, 100, OrderSide::SELL, OrderType::LIMIT, 0, 1, 0);
        _mm_lfence();
        uint64_t start = hft::rdtsc();
        engine->submit_order(sell, result, 0);
        uint64_t end = hft::rdtscp();
        _mm_lfence();
        match_stats.record(end - start);
    }
    
    auto md = engine->get_market_data(0);
    
    printf("=== Matching Engine Benchmark ===\n");
    printf("Matches: %lu\n", match_stats.count);
    printf("Min: %.2f ns\n", match_stats.min_cycles / 3.0);
    printf("Max: %.2f ns\n", match_stats.max_cycles / 3.0);
    printf("Avg: %.2f ns\n", match_stats.avg_ns(3.0));
    printf("Best Bid: %u @ %u\n", md.best_bid_qty, md.best_bid_price);
    printf("Best Ask: %u @ %u\n", md.best_ask_qty, md.best_ask_price);
    
    return 0;
} // namespace hft