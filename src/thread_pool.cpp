#include <algorithm>

#include "condition.hpp"
#include "thread_pool.hpp"

using namespace jms;
using namespace psx;
using namespace std;

namespace {
auto make_threads(psx::Mutex<deque<function<ThreadResult()>>> &tasks,
                  psx::Condition &new_task, size_t nthreads) {
  std::vector<Thread<monostate>> res{};
  res.reserve(nthreads);

  generate_n(back_inserter(res), nthreads, [&tasks, &new_task]() {
    return Thread<monostate>([&]() {
      ThreadResult res;

      do {
        Lock<deque<function<ThreadResult()>>> locked = new_task.wait_until(
            tasks.lock(), [](auto &it) { return !it.empty(); });

        auto task = std::move(locked->front());
        locked->pop_front();

        res = task();
      } while (res != ThreadResult::Break);

      return monostate{};
    });
  });

  return res;
}
} // namespace

ThreadPool::ThreadPool(size_t nthreads)
    : tasks{}, tasks_changed{},
      threads(make_threads(tasks, tasks_changed, nthreads)) {}

void ThreadPool::enqueue(std::function<ThreadResult()> job) {
  auto lock = tasks.lock();
  lock->emplace_back(std::move(job));
  tasks_changed.signal();
}

void ThreadPool::shutdown() {
  for (size_t i = 0; i < threads.size(); i++)
    enqueue([]() { return ThreadResult::Break; });
}
