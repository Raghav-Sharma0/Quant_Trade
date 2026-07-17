// =============================================================================
//  bench_risk_engine.cpp  —  HFT Risk Engine Latency Benchmark
//
//  Professional benchmark following Jane Street / HRT / Optiver internal style.
//
//  Design principles:
//    - Zero heap allocation on hot path (static sample array)
//    - RDTSC with LFENCE serialisation barriers
//    - Welford's online algorithm for numerically-stable variance
//    - Exact percentiles from fully-sorted sample array (post-loop)
//    - CPU pinning via sched_setaffinity
//    - TSC calibrated against CLOCK_MONOTONIC_RAW
//
//  Metrics reported:
//    min, mean, stddev, variance, CV, p50, p90, p95, p99, p99.9, max,
//    throughput, outlier count, terminal histogram, spike warning
// =============================================================================

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <memory>
#include <sched.h>
#include <pthread.h>
#include <x86intrin.h>

#include "../include/common/types.hpp"
#include "../include/common/constants.hpp"
#include "../include/risk/position_manager.hpp"
#include "../include/risk/risk_manager.hpp"

using namespace hft;

// ─────────────────────────────────────────────────────────────────────────────
//  Constants
// ─────────────────────────────────────────────────────────────────────────────
static constexpr int      WARMUP_ITERS = 100'000;
static constexpr int      BENCH_ITERS  = 1'000'000;
static constexpr int      BENCH_CORE   = 2;

// Static sample storage — 8 MB, zero heap allocation on hot path.
static uint64_t g_samples[BENCH_ITERS];

// ─────────────────────────────────────────────────────────────────────────────
//  System helpers
// ─────────────────────────────────────────────────────────────────────────────
static void get_cpu_name(char* buf, size_t n) {
    FILE* f = fopen("/proc/cpuinfo", "r");
    if (!f) { snprintf(buf, n, "Unknown"); return; }
    char line[256];
    buf[0] = '\0';
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "model name", 10) == 0) {
            const char* c = strchr(line, ':');
            if (c) {
                snprintf(buf, n, "%s", c + 2);
                size_t l = strlen(buf);
                if (l && buf[l-1] == '\n') buf[l-1] = '\0';
            }
            break;
        }
    }
    fclose(f);
}

static long get_cpu_mhz(int core) {
    char path[256];
    snprintf(path, sizeof(path),
             "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq", core);
    FILE* f = fopen(path, "r");
    if (!f) {
        f = fopen("/proc/cpuinfo", "r");
        if (!f) return -1;
        char line[128];
        while (fgets(line, sizeof(line), f)) {
            if (strncmp(line, "cpu MHz", 7) == 0) {
                const char* c = strchr(line, ':');
                fclose(f);
                return c ? (long)atof(c + 2) : -1;
            }
        }
        fclose(f);
        return -1;
    }
    long khz = 0;
    fscanf(f, "%ld", &khz);
    fclose(f);
    return khz / 1000;
}

static bool pin_to_core(int core) {
    cpu_set_t s; CPU_ZERO(&s); CPU_SET(core, &s);
    return pthread_setaffinity_np(pthread_self(), sizeof(s), &s) == 0;
}

static double calibrate_ghz() {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC_RAW, &t0);
    uint64_t c0 = __rdtsc();
    struct timespec sl{0, 200'000'000L};
    nanosleep(&sl, nullptr);
    uint64_t c1 = __rdtsc();
    clock_gettime(CLOCK_MONOTONIC_RAW, &t1);
    double ns = (t1.tv_sec - t0.tv_sec) * 1e9 + (t1.tv_nsec - t0.tv_nsec);
    return (c1 - c0) / ns;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Stats helpers (all operate on sorted g_samples)
// ─────────────────────────────────────────────────────────────────────────────
static uint64_t exact_pct(double p) {
    size_t idx = (size_t)(p / 100.0 * BENCH_ITERS);
    if (idx >= (size_t)BENCH_ITERS) idx = BENCH_ITERS - 1;
    return g_samples[idx];
}

// ─────────────────────────────────────────────────────────────────────────────
//  Terminal histogram (linear buckets, P0–P99.9 range)
// ─────────────────────────────────────────────────────────────────────────────
static void print_histogram(double ghz) {
    static constexpr int NUM_BUCKETS = 16;
    static constexpr int BAR_WIDTH   = 40;

    const uint64_t lo  = g_samples[0];
    const uint64_t hi  = exact_pct(99.9);
    if (hi <= lo) return;

    const uint64_t bw = (hi - lo + NUM_BUCKETS - 1) / NUM_BUCKETS;
    uint64_t counts[NUM_BUCKETS] {};

    // Count samples (only up to P99.9 to avoid outlier domination)
    const size_t pct999_idx = (size_t)(0.999 * BENCH_ITERS);
    for (size_t i = 0; i < pct999_idx; ++i) {
        int b = (int)((g_samples[i] - lo) / bw);
        if (b >= NUM_BUCKETS) b = NUM_BUCKETS - 1;
        ++counts[b];
    }

    uint64_t max_count = *std::max_element(counts, counts + NUM_BUCKETS);
    if (max_count == 0) return;

    printf("\n  Latency Distribution (P0 to P99.9, %d buckets)\n\n", NUM_BUCKETS);
    for (int b = 0; b < NUM_BUCKETS; ++b) {
        double lo_ns = (lo + b * bw) / ghz;
        int bar = (int)((double)counts[b] / max_count * BAR_WIDTH);
        printf("  %6.0f ns | ", lo_ns);
        for (int j = 0; j < bar; ++j) printf("\xe2\x96\x88"); // UTF-8 block █
        printf("  (%llu)\n", (unsigned long long)counts[b]);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  main
// ─────────────────────────────────────────────────────────────────────────────
int main() {
    // ── 1. Environment setup ─────────────────────────────────────────────────
    bool   pinned  = pin_to_core(BENCH_CORE);
    char   cpu[256]; get_cpu_name(cpu, sizeof(cpu));
    long   mhz     = get_cpu_mhz(BENCH_CORE);
    double GHZ     = calibrate_ghz();

    // ── 2. Config header ─────────────────────────────────────────────────────
    printf("==============================================================\n");
    printf("  HFT Core-CPP  |  Risk Engine Latency Benchmark\n");
    printf("==============================================================\n");
    printf("  CPU        : %s\n", cpu);
    printf("  Compiler   : GCC %s\n", __VERSION__);
    printf("  Flags      : -O3 -march=native -funroll-loops\n");
    printf("  Iterations : %d (warm-up: %d)\n", BENCH_ITERS, WARMUP_ITERS);
    printf("  Thread     : Core %d  [pin: %s]\n",
           BENCH_CORE, pinned ? "OK" : "FAILED");
    if (mhz > 0)
        printf("  Frequency  : %ld MHz (current)\n", mhz);
    printf("  TSC rate   : %.4f GHz (calibrated)\n", GHZ);
    printf("==============================================================\n\n");

    // ── 3. Risk engine setup ─────────────────────────────────────────────────
    auto pos_ptr = std::make_unique<PositionManager>();
    PositionManager& pos = *pos_ptr;
    RiskManager risk(pos);

    const ClientId CLI = 1;
    const SymbolId SYM = 0;
    risk.set_limit(CLI, 100'000, 10'000'000, 500'000, 10'000);
    risk.set_collar_pct(CLI, 5);
    risk.set_rate_limit(CLI, 2'000'000, 1'000'000'000ULL);
    risk.on_market_update(SYM, 19900, 20000);

    Order order(42, 19900, 100, OrderSide::BUY, OrderType::LIMIT, SYM, CLI, 0);

    // ── 4. Warm-up ───────────────────────────────────────────────────────────
    printf("  [1/3] Warming up (%d iters)...\n", WARMUP_ITERS);
    for (int i = 0; i < WARMUP_ITERS; ++i) {
        volatile auto r = risk.check_order(order, SYM);
        (void)r;
    }

    // ── 5. Benchmark loop — hot path ─────────────────────────────────────────
    // Welford's online algorithm — single pass, numerically stable
    double   welf_mean = 0.0, welf_M2 = 0.0;
    uint64_t welf_n    = 0;

    printf("  [2/3] Benchmarking (%d iters)...\n", BENCH_ITERS);

    struct timespec wall0, wall1;
    clock_gettime(CLOCK_MONOTONIC_RAW, &wall0);

    for (int i = 0; i < BENCH_ITERS; ++i) {
        _mm_lfence();
        uint64_t t0 = __rdtsc();
        _mm_lfence();

        volatile auto result = risk.check_order(order, SYM);
        (void)result;

        _mm_lfence();
        uint64_t t1 = __rdtsc();
        _mm_lfence();

        uint64_t delta = t1 - t0;
        g_samples[i]   = delta;

        // Welford update (no overflow, numerically stable)
        ++welf_n;
        double d1    = (double)delta - welf_mean;
        welf_mean   += d1 / (double)welf_n;
        double d2    = (double)delta - welf_mean;
        welf_M2     += d1 * d2;
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &wall1);
    double elapsed_s = (wall1.tv_sec - wall0.tv_sec)
                     + (wall1.tv_nsec - wall0.tv_nsec) * 1e-9;

    // ── 6. Post-processing (outside hot path) ────────────────────────────────
    printf("  [3/3] Sorting samples & computing statistics...\n\n");
    std::sort(g_samples, g_samples + BENCH_ITERS);

    // Exact percentiles (sorted array)
    const double p50_c  = (double)exact_pct(50.0);
    const double p90_c  = (double)exact_pct(90.0);
    const double p95_c  = (double)exact_pct(95.0);
    const double p99_c  = (double)exact_pct(99.0);
    const double p999_c = (double)exact_pct(99.9);
    const double min_c  = (double)g_samples[0];
    const double max_c  = (double)g_samples[BENCH_ITERS - 1];

    // Variance / stddev / CV from Welford result
    double variance_c = (welf_n > 1) ? welf_M2 / (double)(welf_n - 1) : 0.0;
    double stddev_c   = std::sqrt(variance_c);
    double cv_pct     = (welf_mean > 0) ? (stddev_c / welf_mean) * 100.0 : 0.0;

    // ns conversion
    auto ns  = [&](double c) { return c / GHZ; };

    // Throughput
    double checks_per_sec = BENCH_ITERS / elapsed_s;

    // Outlier detection (Tukey fence: Q3 + 3 × IQR)
    uint64_t q1            = exact_pct(25.0);
    uint64_t q3            = exact_pct(75.0);
    uint64_t iqr           = q3 > q1 ? q3 - q1 : 0;
    uint64_t outlier_fence = q3 + 3 * iqr;
    int      outlier_count = 0;
    for (int i = BENCH_ITERS - 1; i >= 0; --i) {
        if (g_samples[i] <= outlier_fence) break;
        ++outlier_count;
    }

    // ── 7. Results table ─────────────────────────────────────────────────────
  
    printf("  check_order() — %d samples\n", BENCH_ITERS);

    printf("  %-20s %12s  %12s\n", "Metric", "Cycles", "Nanoseconds");
    printf("  %-20s %12s  %12s\n",
           "--------------------", "------------", "------------");
    printf("  %-20s %12.0f  %12.2f ns\n", "Min",          min_c,      ns(min_c));
    printf("  %-20s %12.2f  %12.2f ns\n", "Mean",         welf_mean,  ns(welf_mean));
    printf("  %-20s %12.2f  %12.2f ns\n", "Std Dev",      stddev_c,   ns(stddev_c));
    printf("  %-20s %12.2f  %12.2f ns^2\n","Variance",    variance_c, ns(stddev_c) * ns(stddev_c));
    printf("  %-20s %11.2f%%\n",           "Coeff of Var",cv_pct);
    printf("  %-20s %12s  %12s\n", "", "", "");
    printf("  %-20s %12.0f  %12.2f ns\n", "P50 (median)", p50_c,  ns(p50_c));
    printf("  %-20s %12.0f  %12.2f ns\n", "P90",          p90_c,  ns(p90_c));
    printf("  %-20s %12.0f  %12.2f ns\n", "P95",          p95_c,  ns(p95_c));
    printf("  %-20s %12.0f  %12.2f ns\n", "P99",          p99_c,  ns(p99_c));
    printf("  %-20s %12.0f  %12.2f ns\n", "P99.9 (tail)", p999_c, ns(p999_c));
    printf("  %-20s %12.0f  %12.2f ns\n", "Max",          max_c,  ns(max_c));
    printf("  %-20s %12s  %12s\n", "", "", "");
    printf("  %-20s %12d\n",  "Outliers (3xIQR)", outlier_count);
    printf("  %-20s %12.2f ns\n", "Outlier fence",  (double)outlier_fence / GHZ);
    printf("  %-20s %12.6f s\n", "Total elapsed",   elapsed_s);
    printf("  %-20s %12.2f M/s\n","Throughput",      checks_per_sec / 1e6);
    printf("==============================================================\n");

    // ── 8. HFT compliance check ──────────────────────────────────────────────
    bool p99_ok  = ns(p99_c)  < 500.0;
    bool p999_ok = ns(p999_c) < 1000.0;
    printf("\n  HFT Compliance:\n");
    printf("    P99  < 500 ns  : %s (%.2f ns)\n",
           p99_ok  ? "PASS" : "FAIL", ns(p99_c));
    printf("    P99.9 < 1000 ns: %s (%.2f ns)\n",
           p999_ok ? "PASS" : "FAIL", ns(p999_c));

    // ── 9. Spike warning ─────────────────────────────────────────────────────
    double max_ns   = ns(max_c);
    double p999_ns  = ns(p999_c);
    double spike_ratio = (p999_ns > 0) ? max_ns / p999_ns : 0.0;

    if (spike_ratio > 100.0) {
        printf("\n  WARNING — Abnormal latency spike detected\n");
        printf("    Max latency  : %.2f us\n", max_ns / 1000.0);
        printf("    P99.9 latency: %.2f ns\n", p999_ns);
        printf("    Spike ratio  : %.0fx above P99.9\n", spike_ratio);
        printf("    Likely cause : OS scheduler interrupt, context switch,\n");
        printf("                   TLB miss, page fault, or NUMA migration.\n");
        printf("    Suggestion   : Run with 'isolcpus=2 nohz_full=2 rcu_nocbs=2'\n");
        printf("                   or use SCHED_FIFO priority on this core.\n");
    }

    // ── 10. Terminal histogram ────────────────────────────────────────────────
    print_histogram(GHZ);

    // ── 11. JSON export ───────────────────────────────────────────────────────
    printf("\n=== JSON_EXPORT_BEGIN ===\n");
    printf("{\n");
    printf("  \"benchmark\": \"risk_engine_check_order\",\n");
    printf("  \"system\": {\n");
    printf("    \"cpu\": \"%s\",\n",           cpu);
    printf("    \"compiler\": \"GCC %s\",\n", __VERSION__);
    printf("    \"flags\": \"-O3 -march=native -funroll-loops\",\n");
    printf("    \"cpu_mhz\": %ld,\n",          mhz > 0 ? mhz : 0);
    printf("    \"tsc_ghz\": %.4f,\n",         GHZ);
    printf("    \"pinned_core\": %d,\n",       BENCH_CORE);
    printf("    \"pin_success\": %s\n",        pinned ? "true" : "false");
    printf("  },\n");
    printf("  \"run\": {\n");
    printf("    \"iterations\": %d,\n",        BENCH_ITERS);
    printf("    \"warmup\": %d,\n",            WARMUP_ITERS);
    printf("    \"elapsed_s\": %.6f\n",        elapsed_s);
    printf("  },\n");
    printf("  \"latency\": {\n");
    printf("    \"min\":      %.2f,\n",  ns(min_c));
    printf("    \"mean\":     %.2f,\n",  ns(welf_mean));
    printf("    \"stddev\":   %.2f,\n",  ns(stddev_c));
    printf("    \"variance\": %.4f,\n",  ns(stddev_c) * ns(stddev_c));
    printf("    \"cv_pct\":   %.2f,\n",  cv_pct);
    printf("    \"p50\":      %.2f,\n",  ns(p50_c));
    printf("    \"p90\":      %.2f,\n",  ns(p90_c));
    printf("    \"p95\":      %.2f,\n",  ns(p95_c));
    printf("    \"p99\":      %.2f,\n",  ns(p99_c));
    printf("    \"p99_9\":    %.2f,\n",  ns(p999_c));
    printf("    \"max\":      %.2f\n",   ns(max_c));
    printf("  },\n");
    printf("  \"throughput\": {\n");
    printf("    \"checks_per_second\": %.0f\n", checks_per_sec);
    printf("  },\n");
    printf("  \"outliers\": {\n");
    printf("    \"count\":        %d,\n",   outlier_count);
    printf("    \"fence_ns\":     %.2f,\n", (double)outlier_fence / GHZ);
    printf("    \"spike_ratio\":  %.1f,\n", spike_ratio);
    printf("    \"spike_warned\": %s\n",    spike_ratio > 100.0 ? "true" : "false");
    printf("  },\n");
    printf("  \"compliance\": {\n");
    printf("    \"p99_under_500ns\":    %s,\n", p99_ok  ? "true" : "false");
    printf("    \"p999_under_1000ns\":  %s\n",  p999_ok ? "true" : "false");
    printf("  }\n");
    printf("}\n");
    printf("=== JSON_EXPORT_END ===\n");

    return 0;
}
