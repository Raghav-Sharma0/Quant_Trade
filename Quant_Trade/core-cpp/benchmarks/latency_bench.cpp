#include <iostream>
#include <chrono>
#include <x86intrin.h>
#include "../include/common/types.hpp"

using namespace hft;

uint64_t measure_rdtsc_overhead() {
    _mm_lfence();
    uint64_t start = __rdtsc();
    _mm_lfence();
    uint64_t end = __rdtscp(nullptr);
    _mm_lfence();
    return end - start;
}

int main() {
    const int ITERATIONS = 1000000;
    LatencyStats overhead;
    LatencyStats allocation;
    
    for (int i = 0; i < ITERATIONS; ++i) {
        uint64_t cycles = measure_rdtsc_overhead();
        overhead.record(cycles);
    }
    
    for (int i = 0; i < 10000; ++i) {
        Order order;
        _mm_lfence();
        uint64_t start = __rdtsc();
        order.order_id = i;
        order.price = 100;
        order.quantity = 10;
        uint64_t end = __rdtscp(nullptr);
        _mm_lfence();
        allocation.record(end - start);
    }
    
    printf("=== RDTSC Overhead ===\n");
    printf("Min: %.2f ns (%.0f cycles)\n", overhead.min_cycles / 3.0, overhead.min_cycles);
    printf("Max: %.2f ns (%.0f cycles)\n", overhead.max_cycles / 3.0, overhead.max_cycles);
    printf("Avg: %.2f ns (%.0f cycles)\n", overhead.avg_ns(3.0), overhead.avg_cycles());
    
    printf("\n=== Order Construction ===\n");
    printf("Min: %.2f ns (%.0f cycles)\n", allocation.min_cycles / 3.0, allocation.min_cycles);
    printf("Max: %.2f ns (%.0f cycles)\n", allocation.max_cycles / 3.0, allocation.max_cycles);
    printf("Avg: %.2f ns (%.0f cycles)\n", allocation.avg_ns(3.0), allocation.avg_cycles());
    
    return 0;
} // namespace hft