#include "thread_pool.hpp"
#include "condition.hpp"

using namespace jms;
using namespace psx;
using namespace std;
using ThreadFunc = ThreadPool::ThreadFunc;

namespace {
inline auto extract_task(Mutex<deque<ThreadFunc>> &tasks, Condition &new_task) {
  auto locked =
      new_task.wait_until(tasks.lock(), [](auto &it) { return !it.empty(); });

  auto task = std::move(locked->front());
  locked->pop_front();

  return task;
}

inline std::vector<Thread<monostate>>
make_threads(psx::Mutex<deque<ThreadFunc>> &tasks, psx::Condition &new_task,
             size_t nthreads) {
  std::vector<Thread<monostate>> res{};
  res.reserve(nthreads);

  for (size_t i = 0; i < nthreads; i++) {
    res.emplace_back([&, i]() {
      ThreadResult res;

      do {
        auto task = extract_task(tasks, new_task);
        res = task(i);
      } while (res != ThreadResult::Break);

      return monostate{};
    });
  }

  return res;
}
} // namespace

ThreadPool::ThreadPool(size_t nthreads)
    : tasks{}, tasks_changed{},
      threads(make_threads(tasks, tasks_changed, nthreads)) {}

void ThreadPool::enqueue(ThreadFunc job) noexcept {
  auto lock = tasks.lock();
  lock->emplace_back(std::move(job));
  tasks_changed.signal();
}

void ThreadPool::shutdown() noexcept {
  for (size_t i = 0; i < threads.size(); i++)
    enqueue([](auto) { return ThreadResult::Break; });
}

void ThreadPool::join() noexcept {
  for (auto &i : threads)
    i.join();
}
