#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "../common/cacheline.hpp"
#include "../common/constants.hpp"

namespace hft {

// ============================================================================
// MPSCRingBuffer<T, Capacity>
//
// Multiple-Producer, Single-Consumer lock-free ring buffer.
//
// Design (CAS-based slot reservation):
//  - Each slot has an atomic sequence number acting as a ready flag.
//  - Producers compete via CAS on a shared head_ to claim a slot.
//  - After writing, producer publishes by storing (claimed_idx + 1) to
//    slot sequence — this is the "ready" signal for the consumer.
//  - Consumer checks slot sequence to detect in-order readiness.
//  - Power-of-two size for bitmask indexing.
//
// Throughput: suitable for O(10M) ops/sec per producer.
// ============================================================================
template <typename T, size_t Capacity = MPSC_BUFFER_SIZE>
class MPSCRingBuffer {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of two");
    static constexpr size_t MASK = Capacity - 1;

    struct Slot {
        alignas(CACHELINE_SIZE) std::atomic<size_t> sequence;
        T data;
    };

public:
    MPSCRingBuffer() noexcept {
        for (size_t i = 0; i < Capacity; ++i) {
            slots_[i].sequence.store(i, std::memory_order_relaxed);
        }
        head_.store(0, std::memory_order_relaxed);
        tail_.store(0, std::memory_order_relaxed);
    }

    MPSCRingBuffer(const MPSCRingBuffer&) = delete;
    MPSCRingBuffer& operator=(const MPSCRingBuffer&) = delete;

    // -------------------------------------------------------------------------
    // push — called by ANY producer thread (lock-free via CAS)
    // Returns true on success, false if full.
    // -------------------------------------------------------------------------
    [[nodiscard]] bool push(const T& item) noexcept {
        size_t head = head_.load(std::memory_order_relaxed);

        while (true) {
            Slot& slot = slots_[head & MASK];
            const size_t seq = slot.sequence.load(std::memory_order_acquire);
            const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(head);

            if (diff == 0) {
                // Slot is free — try to claim it
                if (head_.compare_exchange_weak(head, head + 1,
                        std::memory_order_relaxed, std::memory_order_relaxed)) {
                    // We own the slot
                    slot.data = item;
                    // Publish: sequence = head + 1 signals consumer
                    slot.sequence.store(head + 1, std::memory_order_release);
                    return true;
                }
                // CAS failed — another producer grabbed the slot; retry
            } else if (diff < 0) {
                // Buffer is full
                return false;
            } else {
                // Another producer is ahead; reload head and retry
                head = head_.load(std::memory_order_relaxed);
            }
        }
    }

    // -------------------------------------------------------------------------
    // pop — called by the SINGLE consumer thread
    // Returns true on success, false if empty.
    // -------------------------------------------------------------------------
    [[nodiscard]] bool pop(T& out) noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        Slot& slot = slots_[tail & MASK];
        const size_t seq = slot.sequence.load(std::memory_order_acquire);
        const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(tail + 1);

        if (diff == 0) {
            out = slot.data;
            // Signal that slot is free for next wrap-around
            slot.sequence.store(tail + Capacity, std::memory_order_release);
            tail_.store(tail + 1, std::memory_order_relaxed);
            return true;
        }
        // diff < 0 → empty; diff > 0 → producer hasn't finished writing yet
        return false;
    }

    // -------------------------------------------------------------------------
    // Diagnostics (approximate)
    // -------------------------------------------------------------------------
    [[nodiscard]] size_t size() const noexcept {
        return head_.load(std::memory_order_acquire)
             - tail_.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool   empty()    const noexcept { return size() == 0; }
    [[nodiscard]] size_t capacity() const noexcept { return Capacity; }

private:
    alignas(CACHELINE_SIZE) Slot slots_[Capacity];

    // head: claimed by producers (CAS)
    alignas(CACHELINE_SIZE) std::atomic<size_t> head_;
    // tail: advanced only by consumer
    alignas(CACHELINE_SIZE) std::atomic<size_t> tail_;
};

} // namespace hft
