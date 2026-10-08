#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_ATOMICSHAREDPTR_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_ATOMICSHAREDPTR_HPP

#include <atomic>
#include <memory>

// std::atomic<std::shared_ptr<T>> (C++20) is not implemented by Apple's libc++. There the same
// load/store/exchange interface is provided over the std::atomic_* free functions for shared_ptr.
#if defined(__cpp_lib_atomic_shared_ptr)
template <typename T>
using AtomicSharedPtr = std::atomic<std::shared_ptr<T>>;
#else
template <typename T>
class AtomicSharedPtr {
public:
    AtomicSharedPtr() = default;
    AtomicSharedPtr(std::shared_ptr<T> value) noexcept : pointer(std::move(value)) {}
    AtomicSharedPtr(const AtomicSharedPtr&) = delete;
    AtomicSharedPtr& operator=(const AtomicSharedPtr&) = delete;

    std::shared_ptr<T> load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
        return std::atomic_load_explicit(&pointer, order);
    }

    void store(std::shared_ptr<T> value, std::memory_order order = std::memory_order_seq_cst) noexcept {
        std::atomic_store_explicit(&pointer, std::move(value), order);
    }

    std::shared_ptr<T> exchange(std::shared_ptr<T> value, std::memory_order order = std::memory_order_seq_cst) noexcept {
        return std::atomic_exchange_explicit(&pointer, std::move(value), order);
    }

private:
    std::shared_ptr<T> pointer;
};
#endif

#endif
