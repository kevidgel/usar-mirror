#include "rw_deque.hpp"

#include <glm/vec2.hpp>
#include <mutex>
#include <thread>

template <typename T> bool RWDeque<T>::empty() const {
    std::shared_lock lock(rwlock);
    return dq.empty();
}

template <typename T> size_t RWDeque<T>::size() const {
    std::shared_lock lock(rwlock);
    return dq.size();
}

template <typename T> std::optional<typename RWDeque<T>::Entry> RWDeque<T>::peek(size_t i) const {
    std::shared_lock lock(rwlock);
    if (dq.empty() || i >= dq.size()) {
        return std::nullopt;
    } else {
        return dq.at(i);
    }
}

template <typename T> std::optional<typename RWDeque<T>::Entry> RWDeque<T>::peek_front() const {
    std::shared_lock lock(rwlock);
    if (dq.empty()) {
        return std::nullopt;
    } else {
        return dq.front();
    }
}

template <typename T> std::optional<typename RWDeque<T>::Entry> RWDeque<T>::peek_back() const {
    std::shared_lock lock(rwlock);
    if (dq.empty()) {
        return std::nullopt;
    } else {
        return dq.back();
    }
}

template <typename T> std::optional<typename RWDeque<T>::Entry> RWDeque<T>::pop_front() {
    std::unique_lock lock(rwlock);
    if (dq.empty()) {
        return std::nullopt;
    } else {
        Entry value = dq.front();
        dq.pop_front();
        return value;
    }
}

template <typename T> std::optional<typename RWDeque<T>::Entry> RWDeque<T>::pop_back() {
    std::unique_lock lock(rwlock);
    if (dq.empty()) {
        return std::nullopt;
    } else {
        Entry value = dq.back();
        dq.pop_back();
        return value;
    }
}

template <typename T> void RWDeque<T>::push_back(const T &value) {
    std::unique_lock lock(rwlock);
    dq.push_back({std::chrono::system_clock::now(), value});
}

template <typename T> void RWDeque<T>::push_front(const T &value) {
    std::unique_lock lock(rwlock);
    dq.push_front({std::chrono::system_clock::now(), value});
}

template <typename T> void RWDeque<T>::clear() {
    std::unique_lock lock(rwlock);
    dq.clear();
}

template <typename T>
void RWDeque<T>::evict(std::chrono::milliseconds duration, std::chrono::system_clock::time_point timestamp) {
    std::unique_lock lock(rwlock);
    while (!dq.empty() && dq.back().timestamp <= timestamp - duration) {
        dq.pop_back();
    }
}

template <typename T>
template <typename Rep, typename Period>
std::optional<T> RWDeque<T>::try_pop_front_for(const std::chrono::duration<Rep, Period> &timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::unique_lock lock(rwlock);
            if (!dq.empty()) {
                Entry value = dq.front();
                dq.pop_front();
                return value.val;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return std::nullopt;
}

template <typename T>
template <typename Rep, typename Period>
std::optional<T> RWDeque<T>::try_pop_back_for(const std::chrono::duration<Rep, Period> &timeout) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::unique_lock lock(rwlock);
            if (!dq.empty()) {
                Entry value = dq.back();
                dq.pop_back();
                return value.val;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return std::nullopt;
}

template class RWDeque<std::vector<std::pair<uint8_t, glm::vec2>>>;
