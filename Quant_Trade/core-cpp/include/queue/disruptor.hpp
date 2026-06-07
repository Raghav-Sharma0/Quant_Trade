#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include "../common/cacheline.hpp"
#include "../common/constants.hpp"

namespace hft {

// ============================================================================
// Disruptor<T, Capacity>
//
// LMAX Disruptor-inspired single-producer multi-consumer ring buffer.
//
// Key concepts:
//  - producer_seq_: the sequence the producer has published up to.
//  - Each consumer maintains its own sequence cursor (ConsumerBarrier).
//  - Consumers can batch-consume all available events up to producer_seq_.
//  - No locks anywhere on the hot path.
//  - Power-of-two ring buffer with bitmask indexing.
//
// Usage pattern:
//   // Producer
//   auto seq = disruptor.claim_next();       // claim a slot
//   disruptor.get(seq) = MyEvent{...};       // write event
//   disruptor.publish(seq);                  // make visible to consumers
//
//   // Consumer (via ConsumerBarrier)
//   ConsumerBarrier barrier(disruptor);
//   int64_t available = barrier.wait_for(next_to_consume);
//   for (int64_t s = next_to_consume; s <= available; ++s) {
//       process(disruptor.get(s));
//   }
//   barrier.commit(available);
// ============================================================================
template <typename T, size_t Capacity = DISRUPTOR_SIZE>
class Disruptor {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of two");
    static constexpr size_t MASK = Capacity - 1;

public:
    // -------------------------------------------------------------------------
    // ConsumerBarrier — one per consumer thread
    // -------------------------------------------------------------------------
    class ConsumerBarrier {
    public:
        explicit ConsumerBarrier(const Disruptor& d) noexcept
            : disruptor_(d), next_seq_(0)
        {}

        // Spin until at least `desired_seq` is available.
        // Returns the highest sequence number available.
        // For a yielding version, add cpu_relax() inside the loop.
        [[nodiscard]] int64_t wait_for(int64_t desired_seq) const noexcept {
            int64_t available;
            while ((available = disruptor_.producer_seq_.load(std::memory_order_acquire))
                   < desired_seq) {
                cpu_relax();
            }
            return available;
        }

        // Try once without spinning — returns -1 if not available yet
        [[nodiscard]] int64_t try_get(int64_t desired_seq) const noexcept {
            const int64_t available = disruptor_.producer_seq_.load(std::memory_order_acquire);
            return available >= desired_seq ? available : -1;
        }

        void commit(int64_t seq) noexcept {
            cursor_.store(seq, std::memory_order_release);
            next_seq_ = seq + 1;
        }

        [[nodiscard]] int64_t cursor()   const noexcept { return cursor_.load(std::memory_order_relaxed); }
        [[nodiscard]] int64_t next_seq() const noexcept { return next_seq_; }

    private:
        const Disruptor&                      disruptor_;
        alignas(CACHELINE_SIZE) std::atomic<int64_t> cursor_{-1};
        int64_t                               next_seq_ = 0;
    };

    // -------------------------------------------------------------------------
    Disruptor() noexcept : producer_seq_(-1) {}

    Disruptor(const Disruptor&) = delete;
    Disruptor& operator=(const Disruptor&) = delete;

    // -------------------------------------------------------------------------
    // claim_next — producer claims the next sequence slot
    // Returns the sequence number for which the producer should write.
    // BLOCKING: spins if the ring is full (consumer too far behind).
    // -------------------------------------------------------------------------
    [[nodiscard]] int64_t claim_next(int64_t consumer_min_cursor) noexcept {
        const int64_t next = producer_seq_.load(std::memory_order_relaxed) + 1;

        // Spin until consumer has advanced enough to free a slot
        while (next - consumer_min_cursor >= static_cast<int64_t>(Capacity)) {
            cpu_relax();
        }
        return next;
    }

    // Non-blocking claim: returns -1 if ring is full
    [[nodiscard]] int64_t try_claim(int64_t consumer_min_cursor) noexcept {
        const int64_t next = producer_seq_.load(std::memory_order_relaxed) + 1;
        if (next - consumer_min_cursor >= static_cast<int64_t>(Capacity)) {
            return -1;
        }
        return next;
    }

    // -------------------------------------------------------------------------
    // publish — make a sequence visible to consumers
    // -------------------------------------------------------------------------
    void publish(int64_t seq) noexcept {
        producer_seq_.store(seq, std::memory_order_release);
    }

    // -------------------------------------------------------------------------
    // get — access the event at a sequence number (no bounds check in release)
    // -------------------------------------------------------------------------
    [[nodiscard]] T& get(int64_t seq) noexcept {
        return slots_[static_cast<size_t>(seq) & MASK];
    }

    [[nodiscard]] const T& get(int64_t seq) const noexcept {
        return slots_[static_cast<size_t>(seq) & MASK];
    }

    // -------------------------------------------------------------------------
    // Convenience: single-shot publish (claim + write + publish)
    // -------------------------------------------------------------------------
    bool publish_one(const T& item, int64_t consumer_min_cursor) noexcept {
        const int64_t next = producer_seq_.load(std::memory_order_relaxed) + 1;
        if (next - consumer_min_cursor >= static_cast<int64_t>(Capacity)) {
            return false;
        }
        slots_[static_cast<size_t>(next) & MASK] = item;
        producer_seq_.store(next, std::memory_order_release);
        return true;
    }

    [[nodiscard]] int64_t producer_cursor() const noexcept {
        return producer_seq_.load(std::memory_order_acquire);
    }

    [[nodiscard]] size_t  capacity()        const noexcept { return Capacity; }

private:
    alignas(CACHELINE_SIZE) T               slots_[Capacity];
    alignas(CACHELINE_SIZE) std::atomic<int64_t> producer_seq_;
};

} // namespace hft
