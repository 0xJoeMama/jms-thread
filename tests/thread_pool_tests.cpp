#include "mutex.hpp"
#include "thread_pool.hpp"

int main(void) {
  psx::Mutex<int> m(0);

  jms::ThreadPool pool(12);
  pool.enqueue([]() {
    while (1)
      ;

    return jms::ThreadResult::Continue;
  });

  pool.join();
  return 0;
}
