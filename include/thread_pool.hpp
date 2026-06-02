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
  psx::Mutex<std::deque<std::function<ThreadResult()>>> tasks;
  psx::Condition tasks_changed;
  std::vector<psx::Thread<std::monostate>> threads;

public:
  ThreadPool(size_t nthreads);

  void enqueue(std::function<ThreadResult()> job) noexcept;
  void shutdown() noexcept;
  void join() noexcept;

  size_t queue_size() { return tasks.lock()->size(); }
};
} // namespace jms
