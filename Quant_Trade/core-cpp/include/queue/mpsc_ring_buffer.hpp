#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "../common/cacheline.hpp"
#include "../common/constants.hpp"

namespace hft {

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

    [[nodiscard]] bool push(const T& item) noexcept {
        size_t head = head_.load(std::memory_order_relaxed);

        while (true) {
            Slot& slot = slots_[head & MASK];
            const size_t seq = slot.sequence.load(std::memory_order_acquire);
            const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(head);

            if (diff == 0) {
                if (head_.compare_exchange_weak(head, head + 1,
                        std::memory_order_relaxed, std::memory_order_relaxed)) {
                    slot.data = item;
                    slot.sequence.store(head + 1, std::memory_order_release);
                    return true;
                }
            } else if (diff < 0) {
                return false;
            } else {
                head = head_.load(std::memory_order_relaxed);
            }
        }
    }

    [[nodiscard]] bool pop(T& out) noexcept {
        const size_t tail = tail_.load(std::memory_order_relaxed);
        Slot& slot = slots_[tail & MASK];
        const size_t seq = slot.sequence.load(std::memory_order_acquire);
        const intptr_t diff = static_cast<intptr_t>(seq) - static_cast<intptr_t>(tail + 1);

        if (diff == 0) {
            out = slot.data;
            slot.sequence.store(tail + Capacity, std::memory_order_release);
            tail_.store(tail + 1, std::memory_order_relaxed);
            return true;
        }
        return false;
    }

    [[nodiscard]] size_t size() const noexcept {
        return head_.load(std::memory_order_acquire)
             - tail_.load(std::memory_order_acquire);
    }

    [[nodiscard]] bool   empty()    const noexcept { return size() == 0; }
    [[nodiscard]] size_t capacity() const noexcept { return Capacity; }

private:
    alignas(CACHELINE_SIZE) Slot slots_[Capacity];

    alignas(CACHELINE_SIZE) std::atomic<size_t> head_;
    alignas(CACHELINE_SIZE) std::atomic<size_t> tail_;
};

} // namespace hft
