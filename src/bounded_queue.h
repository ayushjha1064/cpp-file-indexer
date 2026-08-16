#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

// A multi-producer, multi-consumer queue with a fixed item capacity.
// close() stops new producers while allowing consumers to drain queued items.
template <typename T>
class BoundedBlockingQueue {
public:
    explicit BoundedBlockingQueue(size_t capacity) : capacity_(capacity) {
        if (capacity_ == 0) {
            throw std::invalid_argument("BoundedBlockingQueue capacity must be greater than zero.");
        }
    }

    BoundedBlockingQueue(const BoundedBlockingQueue&) = delete;
    BoundedBlockingQueue& operator=(const BoundedBlockingQueue&) = delete;

    void push(T item) {
        std::unique_lock<std::mutex> lock(mutex_);
        notFull_.wait(lock, [this] { return closed_ || queue_.size() < capacity_; });

        if (closed_) {
            throw std::logic_error("Cannot push to a closed BoundedBlockingQueue.");
        }

        queue_.push_back(std::move(item));
        lock.unlock();
        notEmpty_.notify_one();
    }

    // Returns nullopt only after close() and after all queued items are consumed.
    std::optional<T> pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        notEmpty_.wait(lock, [this] { return closed_ || !queue_.empty(); });

        if (queue_.empty()) {
            return std::nullopt;
        }

        T item = std::move(queue_.front());
        queue_.pop_front();
        lock.unlock();
        notFull_.notify_one();
        return item;
    }

    void close() noexcept {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            closed_ = true;
        }
        notEmpty_.notify_all();
        notFull_.notify_all();
    }

    bool isClosed() const noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        return closed_;
    }

private:
    const size_t capacity_;
    std::deque<T> queue_;
    mutable std::mutex mutex_;
    std::condition_variable notEmpty_;
    std::condition_variable notFull_;
    bool closed_ = false;
};
