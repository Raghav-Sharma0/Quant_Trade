#pragma once

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <new>
#include "../common/types.hpp"
#include "../common/cacheline.hpp"

namespace hft {

template <typename T, size_t Capacity>
class ObjectPool {
    static_assert(Capacity > 0, "Capacity must be positive");
    static_assert(sizeof(T) >= sizeof(void*), "T must be at least pointer-sized");

public:
    ObjectPool() noexcept {
        for (size_t i = 0; i < Capacity - 1; ++i) {
            *reinterpret_cast<void**>(&storage_[i]) = &storage_[i + 1];
        }
        *reinterpret_cast<void**>(&storage_[Capacity - 1]) = nullptr;
        head_ = &storage_[0];
        free_count_ = Capacity;
    }

    ObjectPool(const ObjectPool&) = delete;
    ObjectPool& operator=(const ObjectPool&) = delete;

    [[nodiscard]] T* allocate() noexcept {
        if (HFT_UNLIKELY(head_ == nullptr)) return nullptr;
        void* slot = head_;
        head_ = *reinterpret_cast<void**>(slot);
        --free_count_;
        return static_cast<T*>(slot);
    }

    template <typename... Args>
    [[nodiscard]] T* construct(Args&&... args) noexcept {
        T* ptr = allocate();
        if (HFT_UNLIKELY(!ptr)) return nullptr;
        return ::new (ptr) T(static_cast<Args&&>(args)...);
    }

    void deallocate(T* ptr) noexcept {
        assert(owns(ptr));
        *reinterpret_cast<void**>(ptr) = head_;
        head_ = ptr;
        ++free_count_;
    }

    void destroy(T* ptr) noexcept {
        ptr->~T();
        deallocate(ptr);
    }

    [[nodiscard]] size_t free_count()  const noexcept { return free_count_; }
    [[nodiscard]] size_t used_count()  const noexcept { return Capacity - free_count_; }
    [[nodiscard]] size_t capacity()    const noexcept { return Capacity; }
    [[nodiscard]] bool   empty()       const noexcept { return head_ == nullptr; }
    [[nodiscard]] bool   full()        const noexcept { return free_count_ == Capacity; }

    [[nodiscard]] bool owns(const T* ptr) const noexcept {
        return ptr >= &storage_[0] && ptr < &storage_[Capacity];
    }

private:
    alignas(CACHELINE_SIZE) T storage_[Capacity];
    void*  head_       = nullptr;
    size_t free_count_ = 0;
};

} // namespace hft
