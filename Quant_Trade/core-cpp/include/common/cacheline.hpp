#pragma once

#include <cstddef>
#include <cstdint>
#include <new>   // std::hardware_destructive_interference_size (C++17)

namespace hft {

// ============================================================================
// Canonical cache-line size
// On x86-64 this is 64 bytes; verify at compile time if possible.
// ============================================================================
inline constexpr size_t CACHELINE_SIZE = 64;

// ============================================================================
// Helper macro: annotate a member / local variable to be cache-line aligned
// Usage: CACHELINE_ALIGNED int counter;
// ============================================================================
#define CACHELINE_ALIGNED alignas(::hft::CACHELINE_SIZE)

// ============================================================================
// Padding structure
// Pad a struct to be a multiple of a cache line.
// Usage:
//   struct Foo {
//       std::atomic<uint64_t> head;
//       CachelinePad<sizeof(std::atomic<uint64_t>)> _pad;
//   };
// ============================================================================
template <size_t UsedBytes>
struct CachelinePad {
    static_assert(UsedBytes < CACHELINE_SIZE, "UsedBytes must be < CACHELINE_SIZE");
    static constexpr size_t PAD_SIZE = CACHELINE_SIZE - UsedBytes;
    char _pad[PAD_SIZE];
};

// Specialisation for exactly-aligned types (no pad needed).
template <>
struct CachelinePad<0> {};

// ============================================================================
// False-sharing guard wrapper
// Wraps a value T so it occupies a whole cache line and prevents false sharing.
//
// Usage:
//   FalseSharingGuard<std::atomic<uint64_t>> producer_seq;
//   FalseSharingGuard<std::atomic<uint64_t>> consumer_seq;
// ============================================================================
template <typename T>
struct alignas(CACHELINE_SIZE) FalseSharingGuard {
    T value{};

    // Allow transparent access
    T& operator*()              noexcept { return value; }
    const T& operator*()  const noexcept { return value; }
    T* operator->()             noexcept { return &value; }
    const T* operator->() const noexcept { return &value; }

private:
    // Pad to full cache line
    char _pad[CACHELINE_SIZE - (sizeof(T) % CACHELINE_SIZE == 0
                                ? CACHELINE_SIZE
                                : sizeof(T) % CACHELINE_SIZE)];
};

// Specialize when sizeof(T) is already a multiple of CACHELINE_SIZE
template <typename T>
    requires (sizeof(T) % CACHELINE_SIZE == 0)
struct alignas(CACHELINE_SIZE) FalseSharingGuard<T> {
    T value{};

    T& operator*()              noexcept { return value; }
    const T& operator*()  const noexcept { return value; }
    T* operator->()             noexcept { return &value; }
    const T* operator->() const noexcept { return &value; }
};

// ============================================================================
// Prefetch helpers
// ============================================================================
[[gnu::always_inline]] inline void prefetch_read(const void* ptr) noexcept {
    __builtin_prefetch(ptr, 0 /*read*/, 3 /*high temporal locality*/);
}

[[gnu::always_inline]] inline void prefetch_write(const void* ptr) noexcept {
    __builtin_prefetch(ptr, 1 /*write*/, 3);
}

// ============================================================================
// Branch prediction hints
// ============================================================================
#define HFT_LIKELY(x)   __builtin_expect(!!(x), 1)
#define HFT_UNLIKELY(x) __builtin_expect(!!(x), 0)

// ============================================================================
// Memory fence helpers
// ============================================================================
[[gnu::always_inline]] inline void cpu_relax() noexcept {
    __asm__ volatile("pause" ::: "memory");
}

[[gnu::always_inline]] inline void compiler_barrier() noexcept {
    __asm__ volatile("" ::: "memory");
}

// ============================================================================
// RDTSC (Read Time-Stamp Counter)
// ============================================================================
[[nodiscard, gnu::always_inline]] inline uint64_t rdtsc() noexcept {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

// rdtscp serializes (prevents out-of-order execution across the fence)
[[nodiscard, gnu::always_inline]] inline uint64_t rdtscp() noexcept {
    uint32_t lo, hi, aux;
    __asm__ volatile("rdtscp" : "=a"(lo), "=d"(hi), "=c"(aux) :: "memory");
    return (static_cast<uint64_t>(hi) << 32) | lo;
}

// Load fence before measurement start
[[gnu::always_inline]] inline void lfence() noexcept {
    __asm__ volatile("lfence" ::: "memory");
}

} // namespace hft
