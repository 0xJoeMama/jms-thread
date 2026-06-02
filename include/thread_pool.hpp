#pragma once

#include <deque>
#include <variant>

#include "condition.hpp"
#include "mutex.hpp"
#include "thread.hpp"

namespace jms {
enum class ThreadResult {
  Continue,
  Break,
};

class ThreadPool {
public:
  using ThreadFunc = std::function<ThreadResult(size_t)>;

private:
  psx::Mutex<std::deque<ThreadFunc>> tasks;
  psx::Condition tasks_changed;
  std::vector<psx::Thread<std::monostate>> threads;

public:
  ThreadPool(size_t nthreads);
  ThreadPool(const ThreadPool &) = delete;
  ThreadPool(ThreadPool &&) = default;

  ThreadPool &operator=(const ThreadPool &) = delete;

  void enqueue(ThreadFunc job) noexcept;
  void shutdown() noexcept;
  void join() noexcept;

  size_t queue_size() { return tasks.lock()->size(); }
  size_t nthreads() const { return threads.size(); }

  unsigned long long thread_id(size_t i) const {
    return threads.at(i).thread_id();
  }
};
} // namespace jms
