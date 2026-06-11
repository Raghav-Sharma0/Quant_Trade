#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include "../common/cacheline.hpp"
#include "../common/constants.hpp"

namespace hft {

template <typename T, size_t Capacity = DISRUPTOR_SIZE>
class Disruptor {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of two");
    static constexpr size_t MASK = Capacity - 1;

public:
    class ConsumerBarrier {
    public:
        explicit ConsumerBarrier(const Disruptor& d) noexcept
            : disruptor_(d), next_seq_(0)
        {}

        [[nodiscard]] int64_t wait_for(int64_t desired_seq) const noexcept {
            int64_t available;
            while ((available = disruptor_.producer_seq_.load(std::memory_order_acquire))
                   < desired_seq) {
                cpu_relax();
            }
            return available;
        }

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

    Disruptor() noexcept : producer_seq_(-1) {}

    Disruptor(const Disruptor&) = delete;
    Disruptor& operator=(const Disruptor&) = delete;

    [[nodiscard]] int64_t claim_next(int64_t consumer_min_cursor) noexcept {
        const int64_t next = producer_seq_.load(std::memory_order_relaxed) + 1;

        while (next - consumer_min_cursor >= static_cast<int64_t>(Capacity)) {
            cpu_relax();
        }
        return next;
    }

    [[nodiscard]] int64_t try_claim(int64_t consumer_min_cursor) noexcept {
        const int64_t next = producer_seq_.load(std::memory_order_relaxed) + 1;
        if (next - consumer_min_cursor >= static_cast<int64_t>(Capacity)) {
            return -1;
        }
        return next;
    }

    void publish(int64_t seq) noexcept {
        producer_seq_.store(seq, std::memory_order_release);
    }

    [[nodiscard]] T& get(int64_t seq) noexcept {
        return slots_[static_cast<size_t>(seq) & MASK];
    }

    [[nodiscard]] const T& get(int64_t seq) const noexcept {
        return slots_[static_cast<size_t>(seq) & MASK];
    }

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
