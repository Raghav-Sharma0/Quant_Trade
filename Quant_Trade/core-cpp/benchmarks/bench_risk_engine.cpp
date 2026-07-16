// =============================================================================
//  bench_risk_engine.cpp — HFT Risk Engine Latency Benchmark
//
//  Professional benchmark configuration:
//    Compiler : GCC (__VERSION__)
//    Flags    : -O3 -march=native -funroll-loops
//    Warmup   : 100,000 iterations
//    Benchmark: 1,000,000 iterations
//    Thread   : Pinned to Core 2 (via sched_setaffinity)
//    Clock    : RDTSC (TSC) calibrated against CLOCK_MONOTONIC_RAW
//
//  Outputs P50 / P90 / P95 / P99 / P99.9 via zero-allocation histogram.
// =============================================================================

#define _GNU_SOURCE
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <memory>
#include <sched.h>          // sched_setaffinity — CPU pinning
#include <pthread.h>
#include <x86intrin.h>

#include "../include/common/types.hpp"
#include "../include/common/constants.hpp"
#include "../include/risk/position_manager.hpp"
#include "../include/risk/risk_manager.hpp"

using namespace hft;

// ─────────────────────────────────────────────────────────────────────────────
//  System Info Helpers
// ─────────────────────────────────────────────────────────────────────────────

// Read CPU model name from /proc/cpuinfo
static void get_cpu_name(char* buf, size_t buflen) {
    FILE* f = fopen("/proc/cpuinfo", "r");
    if (!f) { snprintf(buf, buflen, "Unknown"); return; }
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "model name", 10) == 0) {
            const char* colon = strchr(line, ':');
            if (colon) {
                snprintf(buf, buflen, "%s", colon + 2);
                // Strip trailing newline
                size_t len = strlen(buf);
                if (len > 0 && buf[len-1] == '\n') buf[len-1] = '\0';
            }
            break;
        }
    }
    fclose(f);
}

// Read current CPU frequency in MHz for a given core
static long get_cpu_mhz(int core_id) {
    char path[256];
    snprintf(path, sizeof(path),
        "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", core_id);
    FILE* f = fopen(path, "r");
    if (!f) {
        // Fallback: read from /proc/cpuinfo (MHz field)
        f = fopen("/proc/cpuinfo", "r");
        if (!f) return -1;
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "cpu MHz", 7) == 0) {
                const char* colon = strchr(line, ':');
                if (colon) {
                    fclose(f);
                    return (long)atof(colon + 2);
                }
            }
        }
        fclose(f);
        return -1;
    }
    long khz = 0;
    if (fscanf(f, "%ld", &khz) != 1) khz = -1000;
    fclose(f);
    return khz / 1000;  // kHz → MHz
}

// Pin current thread to a specific CPU core
static bool pin_to_core(int core_id) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core_id, &cpuset);
    return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
}

// Calibrate TSC frequency against CLOCK_MONOTONIC_RAW over 200ms
static double calibrate_ghz() {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    uint64_t c0 = __rdtsc();
    struct timespec sleep_ts { 0, 200'000'000L };
    nanosleep(&sleep_ts, nullptr);
    uint64_t c1 = __rdtsc();
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
    double elapsed_ns = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
    return static_cast<double>(c1 - c0) / elapsed_ns;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Histogram-based percentile tracker (zero heap allocation)
// ─────────────────────────────────────────────────────────────────────────────
struct LatencyHistogram {
    static constexpr int NUM_BUCKETS = 32;
    uint64_t buckets[NUM_BUCKETS] {};
    uint64_t total_count = 0;
    uint64_t min_cycles  = UINT64_MAX;
    uint64_t max_cycles  = 0;
    uint64_t sum_cycles  = 0;

    __attribute__((always_inline))
    void record(uint64_t cycles) noexcept {
        ++total_count;
        sum_cycles += cycles;
        if (__builtin_expect(cycles < min_cycles, 0)) min_cycles = cycles;
        if (__builtin_expect(cycles > max_cycles, 0)) max_cycles = cycles;
        int b = cycles > 0 ? (63 - __builtin_clzll(cycles)) : 0;
        if (b >= NUM_BUCKETS) b = NUM_BUCKETS - 1;
        ++buckets[b];
    }

    uint64_t percentile(double pct) const noexcept {
        uint64_t threshold  = static_cast<uint64_t>(total_count * pct / 100.0);
        uint64_t cumulative = 0;
        for (int b = 0; b < NUM_BUCKETS; ++b) {
            cumulative += buckets[b];
            if (cumulative >= threshold)
                return (1ULL << (b + 1));
        }
        return max_cycles;
    }

    double mean_cycles() const noexcept {
        return total_count > 0
               ? static_cast<double>(sum_cycles) / total_count
               : 0.0;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    // ── 1. Pin thread to Core 2 ──────────────────────────────────────────────
    const int BENCH_CORE = 2;
    bool pinned = pin_to_core(BENCH_CORE);

    // ── 2. Collect system info ───────────────────────────────────────────────
    char cpu_name[256] = "Unknown";
    get_cpu_name(cpu_name, sizeof(cpu_name));
    long cpu_mhz = get_cpu_mhz(BENCH_CORE);

    // ── 3. Calibrate TSC ─────────────────────────────────────────────────────
    const double GHZ = calibrate_ghz();

    // ── 4. Print professional config header ──────────────────────────────────
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║       HFT Core-CPP Risk Engine — Latency Benchmark          ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  CPU        : %-46s║\n", cpu_name);
    printf("║  Compiler   : GCC %-43s║\n", __VERSION__);
    printf("║  Flags      : -O3 -march=native -funroll-loops              ║\n");
    printf("║  Iterations : 1,000,000                                     ║\n");
    printf("║  Warm-up    : 100,000                                        ║\n");
    printf("║  Thread     : Pinned to Core %-31d║\n", BENCH_CORE);
    printf("║  CPU Pin    : %-46s║\n", pinned ? "SUCCESS" : "FAILED (running unpinned)");
    if (cpu_mhz > 0)
        printf("║  Frequency  : %ld MHz (current)%*s║\n", cpu_mhz,
               (int)(46 - snprintf(nullptr, 0, "%ld MHz (current)", cpu_mhz)), "");
    printf("║  TSC Rate   : %.4f GHz (calibrated)                    ║\n", GHZ);
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");

    // ── 5. Setup RiskManager with realistic limits ────────────────────────────
    auto pos_ptr = std::make_unique<PositionManager>();
    PositionManager& pos = *pos_ptr;
    RiskManager risk(pos);

    const ClientId CLI = 1;
    const SymbolId SYM = 0;

    risk.set_limit(CLI,
        /*max_pos=*/     100'000,
        /*max_notional=*/10'000'000,
        /*max_loss=*/    500'000,
        /*max_qty=*/     10'000);
    risk.set_collar_pct(CLI, 5);
    risk.set_rate_limit(CLI, /*max_orders=*/2'000'000, /*window_ns=*/1'000'000'000ULL);
    risk.on_market_update(SYM, /*bid=*/19900, /*ask=*/20000);

    // Typical order: BUY 100 @ $199 — within all limits
    Order order(42, 19900, 100, OrderSide::BUY, OrderType::LIMIT, SYM, CLI, 0);

    // ── 6. Warm-up (100,000 iterations) ─────────────────────────────────────
    printf("Warming up (100,000 iterations)...\n");
    for (int i = 0; i < 100'000; ++i) {
        volatile auto r = risk.check_order(order, SYM);
        (void)r;
    }
    printf("Warm-up complete.\n\n");

    // ── 7. Benchmark loop (1,000,000 iterations) ─────────────────────────────
    printf("Running benchmark (1,000,000 iterations)...\n");
    LatencyHistogram hist;

    for (int i = 0; i < 1'000'000; ++i) {
        _mm_lfence();
        uint64_t t0 = __rdtsc();
        _mm_lfence();

        volatile auto result = risk.check_order(order, SYM);
        (void)result;

        _mm_lfence();
        uint64_t t1 = __rdtsc();
        _mm_lfence();

        hist.record(t1 - t0);
    }

    // ── 8. Compute results ───────────────────────────────────────────────────
    auto to_ns = [&](uint64_t c) -> double { return c / GHZ; };
    auto to_ns_d = [&](double  c) -> double { return c / GHZ; };

    const uint64_t p50_c  = hist.percentile(50.0);
    const uint64_t p90_c  = hist.percentile(90.0);
    const uint64_t p95_c  = hist.percentile(95.0);
    const uint64_t p99_c  = hist.percentile(99.0);
    const uint64_t p999_c = hist.percentile(99.9);

    // ── 9. Print results table ───────────────────────────────────────────────
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║          check_order() Latency — 1,000,000 Samples          ║\n");
    printf("╠════════════════╦══════════════════╦═══════════════════════════╣\n");
    printf("║  Metric        ║  Cycles          ║  Nanoseconds              ║\n");
    printf("╠════════════════╬══════════════════╬═══════════════════════════╣\n");
    printf("║  Min           ║  %-16llu║  %-25.2f║\n",
           (unsigned long long)hist.min_cycles, to_ns(hist.min_cycles));
    printf("║  Mean          ║  %-16.1f║  %-25.2f║\n",
           hist.mean_cycles(), to_ns_d(hist.mean_cycles()));
    printf("╠════════════════╬══════════════════╬═══════════════════════════╣\n");
    printf("║  P50 (median)  ║  %-16llu║  %-25.2f║\n",
           (unsigned long long)p50_c, to_ns(p50_c));
    printf("║  P90           ║  %-16llu║  %-25.2f║\n",
           (unsigned long long)p90_c, to_ns(p90_c));
    printf("║  P95           ║  %-16llu║  %-25.2f║\n",
           (unsigned long long)p95_c, to_ns(p95_c));
    printf("║  P99           ║  %-16llu║  %-25.2f║\n",
           (unsigned long long)p99_c, to_ns(p99_c));
    printf("║  P99.9 (tail)  ║  %-16llu║  %-25.2f║\n",
           (unsigned long long)p999_c, to_ns(p999_c));
    printf("╠════════════════╬══════════════════╬═══════════════════════════╣\n");
    printf("║  Max (spike)   ║  %-16llu║  %-25.2f║\n",
           (unsigned long long)hist.max_cycles, to_ns(hist.max_cycles));
    printf("╚════════════════╩══════════════════╩═══════════════════════════╝\n");

    // ── 10. HFT compliance check ─────────────────────────────────────────────
    const bool p99_ok  = to_ns(p99_c)  < 500.0;
    const bool p999_ok = to_ns(p999_c) < 1000.0;
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                   HFT Compliance Check                      ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  P99  < 500 ns   : %-8s  (%.2f ns)                     ║\n",
           p99_ok  ? "PASS ✓" : "FAIL ✗", to_ns(p99_c));
    printf("║  P99.9 < 1000 ns : %-8s  (%.2f ns)                     ║\n",
           p999_ok ? "PASS ✓" : "FAIL ✗", to_ns(p999_c));
    printf("╚══════════════════════════════════════════════════════════════╝\n");

    // ── 11. JSON export for run_risk_bench.py ────────────────────────────────
    printf("\n=== JSON_EXPORT_BEGIN ===\n");
    printf("{\n");
    printf("  \"benchmark\": \"risk_engine_check_order\",\n");
    printf("  \"system\": {\n");
    printf("    \"cpu\": \"%s\",\n",         cpu_name);
    printf("    \"compiler\": \"GCC %s\",\n", __VERSION__);
    printf("    \"flags\": \"-O3 -march=native -funroll-loops\",\n");
    printf("    \"cpu_mhz\": %ld,\n",        cpu_mhz > 0 ? cpu_mhz : 0);
    printf("    \"tsc_ghz\": %.4f,\n",       GHZ);
    printf("    \"pinned_core\": %d,\n",     BENCH_CORE);
    printf("    \"pin_success\": %s\n",      pinned ? "true" : "false");
    printf("  },\n");
    printf("  \"run\": {\n");
    printf("    \"iterations\": 1000000,\n");
    printf("    \"warmup\": 100000\n");
    printf("  },\n");
    printf("  \"latency_ns\": {\n");
    printf("    \"min\":   %.2f,\n",  to_ns(hist.min_cycles));
    printf("    \"mean\":  %.2f,\n",  to_ns_d(hist.mean_cycles()));
    printf("    \"p50\":   %.2f,\n",  to_ns(p50_c));
    printf("    \"p90\":   %.2f,\n",  to_ns(p90_c));
    printf("    \"p95\":   %.2f,\n",  to_ns(p95_c));
    printf("    \"p99\":   %.2f,\n",  to_ns(p99_c));
    printf("    \"p99_9\": %.2f,\n",  to_ns(p999_c));
    printf("    \"max\":   %.2f\n",   to_ns(hist.max_cycles));
    printf("  },\n");
    printf("  \"compliance\": {\n");
    printf("    \"p99_under_500ns\":   %s,\n",  p99_ok  ? "true" : "false");
    printf("    \"p999_under_1000ns\": %s\n",   p999_ok ? "true" : "false");
    printf("  }\n");
    printf("}\n");
    printf("=== JSON_EXPORT_END ===\n");

    return 0;
}
