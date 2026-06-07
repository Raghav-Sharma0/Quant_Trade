#pragma once

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <new>
#include "../common/types.hpp"
#include "../common/cacheline.hpp"

namespace hft {

// ============================================================================
// ObjectPool<T, Capacity>
//
// Fixed-size, lock-free-friendly free-list pool.
// - Preallocated: all objects reside in a static array
// - allocate() pops from intrusive free-list in O(1)
// - deallocate() pushes back in O(1)
// - Thread-unsafe by design (use per-thread or pair with external synchronization)
// - No heap allocation after construction
//
// The free-list is embedded in unused pool slots — zero extra memory overhead.
// ============================================================================
template <typename T, size_t Capacity>
class ObjectPool {
    static_assert(Capacity > 0, "Capacity must be positive");
    static_assert(sizeof(T) >= sizeof(void*), "T must be at least pointer-sized");

public:
    ObjectPool() noexcept {
        // Build the intrusive free-list through the storage
        for (size_t i = 0; i < Capacity - 1; ++i) {
            *reinterpret_cast<void**>(&storage_[i]) = &storage_[i + 1];
        }
        *reinterpret_cast<void**>(&storage_[Capacity - 1]) = nullptr;
        head_ = &storage_[0];
        free_count_ = Capacity;
    }

    // Not copyable/movable
    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    // -------------------------------------------------------------------------
    // allocate — returns pointer to uninitialized storage, or nullptr if full
    // -------------------------------------------------------------------------
    [[nodiscard]] T* allocate() noexcept {
        if (HFT_UNLIKELY(head_ == nullptr)) return nullptr;
        void* slot = head_;
        head_ = *reinterpret_cast<void**>(slot);
        --free_count_;
        return static_cast<T*>(slot);
    }

    // -------------------------------------------------------------------------
    // construct — allocate + placement-new
    // -------------------------------------------------------------------------
    template <typename... Args>
    [[nodiscard]] T* construct(Args&&... args) noexcept {
        T* ptr = allocate();
        if (HFT_UNLIKELY(!ptr)) return nullptr;
        return ::new (ptr) T(static_cast<Args&&>(args)...);
    }

    // -------------------------------------------------------------------------
    // deallocate — return slot to pool (does NOT call destructor)
    // -------------------------------------------------------------------------
    void deallocate(T* ptr) noexcept {
        assert(owns(ptr));
        *reinterpret_cast<void**>(ptr) = head_;
        head_ = ptr;
        ++free_count_;
    }

    // destroy — call destructor then return to pool
    void destroy(T* ptr) noexcept {
        ptr->~T();
        deallocate(ptr);
    }

    // -------------------------------------------------------------------------
    // Diagnostics
    // -------------------------------------------------------------------------
    [[nodiscard]] size_t free_count()  const noexcept { return free_count_; }
    [[nodiscard]] size_t used_count()  const noexcept { return Capacity - free_count_; }
    [[nodiscard]] size_t capacity()    const noexcept { return Capacity; }
    [[nodiscard]] bool   empty()       const noexcept { return head_ == nullptr; }
    [[nodiscard]] bool   full()        const noexcept { return free_count_ == Capacity; }

    [[nodiscard]] bool owns(const T* ptr) const noexcept {
        return ptr >= &storage_[0] && ptr < &storage_[Capacity];
    }

private:
    // All storage inline — no heap
    alignas(CACHELINE_SIZE) T storage_[Capacity];
    void*  head_       = nullptr;
    size_t free_count_ = 0;
};

} // namespace hft
