#pragma once

#include <mutex>
#include <queue>

namespace utils {

template <typename T>
class thread_safe_queue {
 public:
  using value_type = T;

 public:
  explicit thread_safe_queue() {}

  value_type pop() {
    std::unique_lock<std::mutex> lock(mutex_);
    auto value = queue_.front();
    queue_.pop();
    return value;
  }

  bool empty() {
    std::unique_lock<std::mutex> lock(mutex_);
    return queue_.empty();
  }

  void push(const value_type &value) {
    std::unique_lock<std::mutex> lock(mutex_);
    queue_.push(value);
  }

  void push(value_type &&value) {
    std::unique_lock<std::mutex> lock(mutex_);
    queue_.push(value);
  }

 private:
  std::mutex mutex_;
  std::queue<value_type> queue_;
};

} // namespace utils