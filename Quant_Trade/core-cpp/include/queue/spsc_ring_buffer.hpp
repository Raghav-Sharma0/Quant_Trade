#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include "../common/cacheline.hpp"
#include "../common/constants.hpp"

namespace hft {

// ============================================================================
// SPSCRingBuffer<T, Capacity>
//
// Single-Producer, Single-Consumer lock-free ring buffer.
//
// Design:
//  - Power-of-two capacity → bitmask indexing (no modulo)
//  - Producer and consumer sequence numbers are on separate cache lines
//    to eliminate false sharing
//  - Acquire/Release memory ordering for correctness with minimal fencing
//  - No heap allocation after construction
//  - Zero-copy: push copies T into slot; pop copies out (or use push_ptr/pop_ptr
//    for in-place access)
//
// API:
//  push(const T&)  — returns false if full
//  pop(T&)         — returns false if empty
//  size()          — approximate count (can be stale)
//  empty()         — approximate
// ============================================================================
template <typename T, size_t Capacity = SPSC_BUFFER_SIZE>
class SPSCRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of two");
    static constexpr size_t MASK = Capacity - 1;

public:
    SPSCRingBuffer() noexcept
        : producer_seq_(0), consumer_seq_(0)
    {}

    // Non-copyable/non-movable
    SPSCRingBuffer(const SPSCRingBuffer&) = delete;
    SPSCRingBuffer& operator=(const SPSCRingBuffer&) = delete;

    // -------------------------------------------------------------------------
    // push — called by the single producer thread
    // Returns true on success, false if the buffer is full.
    // -------------------------------------------------------------------------
    [[nodiscard]] bool push(const T& item) noexcept {
        const size_t prod = producer_seq_.load(std::memory_order_relaxed);
        const size_t next = prod + 1;

        // Check if there is room (consumer lags by at most Capacity)
        if (HFT_UNLIKELY(next - consumer_seq_.load(std::memory_order_acquire) > Capacity)) {
            return false; // full
        }

        slots_[prod & MASK] = item;

        // Release: the write to the slot must be visible before we publish
        producer_seq_.store(next, std::memory_order_release);
        return true;
    }

    // -------------------------------------------------------------------------
    // push (move variant)
    // -------------------------------------------------------------------------
    [[nodiscard]] bool push(T&& item) noexcept {
        const size_t prod = producer_seq_.load(std::memory_order_relaxed);
        const size_t next = prod + 1;

        if (HFT_UNLIKELY(next - consumer_seq_.load(std::memory_order_acquire) > Capacity)) {
            return false;
        }

        slots_[prod & MASK] = static_cast<T&&>(item);
        producer_seq_.store(next, std::memory_order_release);
        return true;
    }

    // -------------------------------------------------------------------------
    // pop — called by the single consumer thread
    // Returns true on success, false if empty.
    // -------------------------------------------------------------------------
    [[nodiscard]] bool pop(T& out) noexcept {
        const size_t cons = consumer_seq_.load(std::memory_order_relaxed);

        // Check if producer has published a new item
        if (HFT_UNLIKELY(producer_seq_.load(std::memory_order_acquire) == cons)) {
            return false; // empty
        }

        out = slots_[cons & MASK];

        // Release: signal that we've consumed this slot
        consumer_seq_.store(cons + 1, std::memory_order_release);
        return true;
    }

    // -------------------------------------------------------------------------
    // size — approximate (may be stale); safe to call from any thread
    // -------------------------------------------------------------------------
    [[nodiscard]] size_t size() const noexcept {
        const size_t prod = producer_seq_.load(std::memory_order_acquire);
        const size_t cons = consumer_seq_.load(std::memory_order_acquire);
        return prod - cons;
    }

    [[nodiscard]] bool   empty()    const noexcept { return size() == 0; }
    [[nodiscard]] size_t capacity() const noexcept { return Capacity; }

private:
    // Slots: shared data, laid out contiguously for cache efficiency
    alignas(CACHELINE_SIZE) T slots_[Capacity];

    // Producer sequence: written only by producer
    alignas(CACHELINE_SIZE) std::atomic<size_t> producer_seq_;

    // Consumer sequence: written only by consumer
    // Separated by a full cache line from producer_seq_ to prevent false sharing
    alignas(CACHELINE_SIZE) std::atomic<size_t> consumer_seq_;
};

} // namespace hft
