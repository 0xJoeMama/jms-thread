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
  psx::Condition new_task;
  std::vector<psx::Thread<std::monostate>> threads;

public:
  ThreadPool(size_t nthreads);

  void enqueue(std::function<ThreadResult()> job);
  void shutdown();
};
} // namespace jms
