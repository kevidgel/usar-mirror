/**
 * @file queue.hpp
 *
 * RW locked queue for cross-thread stuff
 */
#pragma once

#include <optional>
#include <queue>
#include <shared_mutex>

/// Simple RW-locked dequeue with duration-based eviction. Unless otherwise mentioned, all methods acquire a lock.
template <typename T> class RWDeque {
public:
    RWDeque() = default;
    ~RWDeque() = default;

    struct Entry {
        std::chrono::system_clock::time_point timestamp;
        T val;
    };

    /// Check if dequeue is empty.
    bool empty() const;
    /// Check size of dequeue.
    size_t size() const;
    /// Get front of dequeue (can return std::nullopt).
    std::optional<Entry> peek_front() const;
    /// Get back of dequeue (can return std::nullopt).
    std::optional<Entry> peek_back() const;
    /// Get an index
    std::optional<Entry> peek(size_t i) const;
    /// Pop front of dequeue (can return std::nullopt).
    std::optional<Entry> pop_front();
    /// Pop back of dequeue (can return std::nullopt).
    std::optional<Entry> pop_back();
    /// Push to back of dequeue
    void push_back(const T &value);
    /// Push to front of dequeue
    void push_front(const T &value);
    /// Clear dequeue
    void clear();
    /// Evict all members before timestamp - duration
    void evict(std::chrono::milliseconds duration, std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now());
    /// Attempt to pop front with timeout
    template <typename Rep, typename Period>
    std::optional<T> try_pop_front_for(const std::chrono::duration<Rep, Period> &timeout);
    /// Attempt to pop back with timeout
    template <typename Rep, typename Period>
    std::optional<T> try_pop_back_for(const std::chrono::duration<Rep, Period> &timeout);

    /// I'm too lazy to make this private
    std::deque<Entry> dq;
    mutable std::shared_mutex rwlock;
};
