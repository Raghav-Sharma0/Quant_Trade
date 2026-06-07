#pragma once

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <new>
#include "../common/cacheline.hpp"

namespace hft {

// ============================================================================
// ArenaAllocator
//
// A monotonic bump allocator backed by a fixed block of memory.
// - O(1) allocation
// - No runtime malloc/free
// - Thread-unsafe by design (used per-thread or with external lock)
// - Supports alignment
// - Supports reset() for bulk deallocation
//
// Usage:
//   alignas(64) char buf[4096];
//   ArenaAllocator arena(buf, 4096);
//   auto* p = arena.alloc<MyStruct>();
// ============================================================================
class ArenaAllocator {
public:
    ArenaAllocator() noexcept = default;

    ArenaAllocator(void* buf, size_t size) noexcept
        : base_(static_cast<char*>(buf))
        , capacity_(size)
        , offset_(0)
    {}

    // Non-copyable, non-movable (owns external memory view)
    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;

    // -------------------------------------------------------------------------
    // Raw allocation: returns aligned pointer or nullptr on OOM
    // -------------------------------------------------------------------------
    [[nodiscard]] void* alloc_raw(size_t bytes, size_t align = alignof(std::max_align_t)) noexcept {
        assert(base_ != nullptr);
        assert((align & (align - 1)) == 0); // must be power of 2

        // Align up
        const size_t aligned_offset = (offset_ + align - 1) & ~(align - 1);
        if (HFT_UNLIKELY(aligned_offset + bytes > capacity_)) {
            return nullptr;
        }
        void* ptr = base_ + aligned_offset;
        offset_   = aligned_offset + bytes;
        return ptr;
    }

    // -------------------------------------------------------------------------
    // Typed allocation with default construction
    // -------------------------------------------------------------------------
    template <typename T, typename... Args>
    [[nodiscard]] T* alloc(Args&&... args) noexcept {
        void* raw = alloc_raw(sizeof(T), alignof(T));
        if (HFT_UNLIKELY(!raw)) return nullptr;
        return ::new (raw) T(static_cast<Args&&>(args)...);
    }

    // -------------------------------------------------------------------------
    // Typed array allocation (default-initialized)
    // -------------------------------------------------------------------------
    template <typename T>
    [[nodiscard]] T* alloc_array(size_t count) noexcept {
        void* raw = alloc_raw(sizeof(T) * count, alignof(T));
        if (HFT_UNLIKELY(!raw)) return nullptr;
        T* arr = static_cast<T*>(raw);
        for (size_t i = 0; i < count; ++i) ::new (arr + i) T{};
        return arr;
    }

    // -------------------------------------------------------------------------
    // Reset — deallocates everything in O(1)
    // -------------------------------------------------------------------------
    void reset() noexcept { offset_ = 0; }

    // -------------------------------------------------------------------------
    // Diagnostics
    // -------------------------------------------------------------------------
    [[nodiscard]] size_t used()      const noexcept { return offset_; }
    [[nodiscard]] size_t capacity()  const noexcept { return capacity_; }
    [[nodiscard]] size_t remaining() const noexcept { return capacity_ - offset_; }

private:
    char*  base_     = nullptr;
    size_t capacity_ = 0;
    size_t offset_   = 0;
};

// ============================================================================
// HeapArena — owns its own heap allocation (for convenience in tests/startup)
// Uses a single large malloc at construction time; never malloc again.
// ============================================================================
class HeapArena {
public:
    explicit HeapArena(size_t capacity)
        : storage_(::operator new(capacity, std::align_val_t{CACHELINE_SIZE}))
        , arena_(storage_, capacity)
    {}

    ~HeapArena() {
        ::operator delete(storage_, std::align_val_t{CACHELINE_SIZE});
    }

    HeapArena(const HeapArena&) = delete;
    HeapArena& operator=(const HeapArena&) = delete;

    ArenaAllocator& allocator() noexcept { return arena_; }

    template <typename T, typename... Args>
    [[nodiscard]] T* alloc(Args&&... args) noexcept {
        return arena_.alloc<T>(static_cast<Args&&>(args)...);
    }

    template <typename T>
    [[nodiscard]] T* alloc_array(size_t count) noexcept {
        return arena_.alloc_array<T>(count);
    }

    void reset() noexcept { arena_.reset(); }

    [[nodiscard]] size_t used()      const noexcept { return arena_.used(); }
    [[nodiscard]] size_t capacity()  const noexcept { return arena_.capacity(); }

private:
    void*          storage_;
    ArenaAllocator arena_;
};

} // namespace hft
