#pragma once

#include <atomic>
#include <cstdint>
#include "../common/cacheline.hpp"

namespace hft {

// ============================================================================
// Sequencer
//
// Assigns globally unique, strictly monotonically increasing sequence numbers.
// Used by the gateway to stamp each inbound order before it enters the journal
// and then the matching engine.
//
// Deterministic replay: the sequence number IS the total order of events.
// Storing it on the Order means the journal can replay in exact order.
//
// Thread safety: next_sequence() uses fetch_add which is atomic/lock-free.
// Multiple callers receive distinct sequence numbers — correct for MPSC use.
// ============================================================================
class Sequencer {
public:
    explicit Sequencer(uint64_t start = 1) noexcept
        : seq_(start)
    {}

    // Not copyable
    Sequencer(const Sequencer&) = delete;
    Sequencer& operator=(const Sequencer&) = delete;

    // -------------------------------------------------------------------------
    // next_sequence — atomically increments and returns the next sequence number.
    // O(1), lock-free (uses hardware fetch-add).
    // -------------------------------------------------------------------------
    [[nodiscard]] uint64_t next_sequence() noexcept {
        return seq_.fetch_add(1, std::memory_order_relaxed);
    }

    // -------------------------------------------------------------------------
    // peek — returns the current sequence without incrementing.
    // Useful for logging / replay boundary detection.
    // -------------------------------------------------------------------------
    [[nodiscard]] uint64_t peek() const noexcept {
        return seq_.load(std::memory_order_acquire);
    }

    // -------------------------------------------------------------------------
    // reset — use ONLY for replay (single-threaded context)
    // -------------------------------------------------------------------------
    void reset(uint64_t val = 1) noexcept {
        seq_.store(val, std::memory_order_seq_cst);
    }

    // -------------------------------------------------------------------------
    // batch_claim — claims [seq, seq+count) atomically.
    // Returns the first sequence of the batch.
    // -------------------------------------------------------------------------
    [[nodiscard]] uint64_t batch_claim(uint64_t count) noexcept {
        return seq_.fetch_add(count, std::memory_order_relaxed);
    }

private:
    alignas(CACHELINE_SIZE) std::atomic<uint64_t> seq_;
    // Pad to avoid false sharing with adjacent members in containers
    char _pad[CACHELINE_SIZE - sizeof(std::atomic<uint64_t>)];
};

} // namespace hft
