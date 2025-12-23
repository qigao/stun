/*
 * Flex Engine - Arena Allocator
 *
 * Zero-allocation memory management using linear memory pools.
 * Perfect for DSL parsing, animation objects, and temporary allocations.
 */

#pragma once

#include "memory_pool.h"
#include <cstddef>
#include <cstring>
#include <utility>
#include <new>

namespace flex {

// ============================================================================
// ArenaAllocator - Wrapper around MemoryPool for C++ objects
// ============================================================================

class ArenaAllocator {
private:
    MemoryPool* pool_;

public:
    ArenaAllocator() : pool_(nullptr) {}

    explicit ArenaAllocator(size_t size) {
        pool_ = pool_create(size);
    }

    ~ArenaAllocator() {
        if (pool_) {
            pool_destroy(pool_);
        }
    }

    // Non-copyable, non-movable
    ArenaAllocator(const ArenaAllocator&) = delete;
    ArenaAllocator& operator=(const ArenaAllocator&) = delete;
    ArenaAllocator(ArenaAllocator&&) = delete;
    ArenaAllocator& operator=(ArenaAllocator&&) = delete;

    // Allocate raw memory
    void* allocate(size_t size, size_t alignment = 8) {
        if (!pool_) return nullptr;

        // Apply alignment
        size_t aligned_size = (size + alignment - 1) & ~(alignment - 1);
        return pool_alloc(pool_, aligned_size);
    }

    // Allocate and construct object
    template<typename T, typename... Args>
    T* create(Args&&... args) {
        void* mem = allocate(sizeof(T));
        if (!mem) return nullptr;
        return new (mem) T(std::forward<Args>(args)...);
    }

    // Allocate array of objects
    template<typename T>
    T* create_array(size_t count) {
        void* mem = allocate(sizeof(T) * count, alignof(T));
        if (!mem) return nullptr;

        T* array = reinterpret_cast<T*>(mem);
        for (size_t i = 0; i < count; i++) {
            new (&array[i]) T();
        }
        return array;
    }

    // Copy string into pool
    const char* copy_string(const char* str) {
        if (!str) return nullptr;
        size_t len = std::strlen(str);
        char* copy = (char*)allocate(len + 1);
        if (copy) {
            std::memcpy(copy, str, len + 1);
        }
        return copy;
    }

    // Duplicate string with length
    const char* copy_string(const char* str, size_t len) {
        if (!str) return nullptr;
        char* copy = (char*)allocate(len + 1);
        if (copy) {
            std::memcpy(copy, str, len);
            copy[len] = '\0';
        }
        return copy;
    }

    // Reset pool (reuse memory)
    void reset() {
        if (pool_) {
            pool_reset(pool_);
        }
    }

    // Get statistics
    size_t used() {
        return pool_ ? pool_get_used(pool_) : 0;
    }

    size_t available() {
        return pool_ ? pool_get_available(pool_) : 0;
    }

    size_t peak_used() {
        return pool_ ? pool_get_peak(pool_) : 0;
    }

    size_t size() const {
        return pool_ ? pool_->size : 0;
    }

    uint64_t alloc_count() const {
        return pool_ ? pool_->alloc_count : 0;
    }

    // Mark/rewind support for stack-style allocations
    size_t mark() {
        return pool_ ? pool_mark(pool_) : 0;
    }

    void rewind(size_t mark) {
        if (pool_) {
            pool_rewind(pool_, mark);
        }
    }

    // Initialize pool with specific size
    void init(size_t size) {
        if (pool_) {
            pool_destroy(pool_);
        }
        pool_ = pool_create(size);
    }

    // Check if initialized
    bool is_valid() const {
        return pool_ != nullptr;
    }
};

// ============================================================================
// Pool-based smart pointer (no deleter, uses pool)
// ============================================================================

template<typename T>
class PoolPtr {
private:
    T* ptr_;

public:
    using ElementType = T;

    PoolPtr() : ptr_(nullptr) {}
    explicit PoolPtr(T* p) : ptr_(p) {}

    // No deleter - objects are managed by pool
    ~PoolPtr() = default;

    T* get() const { return ptr_; }
    T& operator*() const { return *ptr_; }
    T* operator->() const { return ptr_; }

    explicit operator bool() const { return ptr_ != nullptr; }

    // Copyable because we don't own the resource (arena does)
    PoolPtr(const PoolPtr& other) = default;
    PoolPtr& operator=(const PoolPtr& other) = default;

    // Movable
    PoolPtr(PoolPtr&& other) noexcept : ptr_(other.ptr_) {
        other.ptr_ = nullptr;
    }

    PoolPtr& operator=(PoolPtr&& other) noexcept {
        if (this != &other) {
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }
};

// ============================================================================
// Pool-allocated vector (uses arena for elements)
// ============================================================================

template<typename T>
class PoolVector {
private:
    T* data_;
    size_t size_;
    size_t capacity_;
    ArenaAllocator* allocator_;

public:
    // Iterator types for std:: algorithms
    using value_type = T;
    using size_type = size_t;
    using difference_type = std::ptrdiff_t;
    using reference = T&;
    using const_reference = const T&;
    using pointer = T*;
    using const_pointer = const T*;

    PoolVector() : data_(nullptr), size_(0), capacity_(0), allocator_(nullptr) {}

    explicit PoolVector(ArenaAllocator& alloc)
        : data_(nullptr), size_(0), capacity_(0), allocator_(&alloc) {}

    PoolVector(const PoolVector&) = delete;
    PoolVector& operator=(const PoolVector&) = delete;

    PoolVector(PoolVector&& other) noexcept
        : data_(other.data_), size_(other.size_), capacity_(other.capacity_),
          allocator_(other.allocator_) {
        other.data_ = nullptr;
        other.size_ = 0;
        other.capacity_ = 0;
        other.allocator_ = nullptr;
    }

    ~PoolVector() {
        // Objects are destroyed but memory stays in pool
        clear();
    }

    void clear() {
        for (size_t i = 0; i < size_; i++) {
            data_[i].~T();
        }
        size_ = 0;
    }

    // Erase element at index
    void erase(size_t index) {
        if (index >= size_) return;

        // Destroy element
        data_[index].~T();

        // Move elements after index
        for (size_t i = index; i < size_ - 1; i++) {
            new (&data_[i]) T(std::move(data_[i + 1]));
            data_[i + 1].~T();
        }

        size_--;
    }

    // Erase elements in range [first, last)
    void erase(size_t first, size_t last) {
        if (first >= size_) return;
        if (last > size_) last = size_;
        if (first >= last) return;

        size_t count = last - first;

        // Destroy elements in range
        for (size_t i = first; i < last; i++) {
            data_[i].~T();
        }

        // Move elements after range
        for (size_t i = first; i < size_ - count; i++) {
            new (&data_[i]) T(std::move(data_[i + count]));
            data_[i + count].~T();
        }

        size_ -= count;
    }

    // Remove elements matching predicate (using erase-remove idiom)
    template<typename Predicate>
    void remove_if(Predicate pred) {
        size_t new_size = 0;
        for (size_t i = 0; i < size_; i++) {
            if (!pred(data_[i])) {
                if (new_size != i) {
                    new (&data_[new_size]) T(std::move(data_[i]));
                    data_[i].~T();
                }
                new_size++;
            } else {
                data_[i].~T();
            }
        }
        size_ = new_size;
    }

 

    void push_back(const T& value) {
        if (size_ >= capacity_) {
            grow();
        }
        new (&data_[size_++]) T(value);
    }

    void push_back(T&& value) {
        if (size_ >= capacity_) {
            grow();
        }
        new (&data_[size_++]) T(std::move(value));
    }

    template<typename... Args>
    void emplace_back(Args&&... args) {
        if (size_ >= capacity_) {
            grow();
        }
        new (&data_[size_++]) T(std::forward<Args>(args)...);
    }

    size_t size() const { return size_; }
    size_t capacity() const { return capacity_; }

    T& operator[](size_t index) { return data_[index]; }
    const T& operator[](size_t index) const { return data_[index]; }

    T* data() { return data_; }
    const T* data() const { return data_; }

    T& back() { return data_[size_ - 1]; }
    const T& back() const { return data_[size_ - 1]; }

    // Iterator support for range-based for loops
    T* begin() { return data_; }
    const T* begin() const { return data_; }
    const T* cbegin() const { return data_; }

    T* end() { return data_ + size_; }
    const T* end() const { return data_ + size_; }
    const T* cend() const { return data_ + size_; }

private:
    void grow() {
        size_t new_capacity = capacity_ == 0 ? 1 : capacity_ * 2;
        T* new_data = allocator_->create_array<T>(new_capacity);

        // Move existing elements
        for (size_t i = 0; i < size_; i++) {
            new (&new_data[i]) T(std::move(data_[i]));
            data_[i].~T();
        }

        capacity_ = new_capacity;
        data_ = new_data;
    }
};

} // namespace flex
