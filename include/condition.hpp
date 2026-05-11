#include <cassert>
#include <pthread.h>

#include "mutex.hpp"

namespace psx {
template <typename T> class Condition {
  pthread_cond_t cond;

public:
  Condition() { pthread_cond_init(&cond, nullptr); }
  ~Condition() noexcept { assert(pthread_cond_destroy(&cond) >= 0); }
  void signal() noexcept { pthread_cond_signal(&cond); }
  void broacast() noexcept { pthread_cond_broadcast(&cond); }

  template <typename Fn> void wait_until(Lock<T> lock, Fn f) {
    while (!f(*lock))
      pthread_cond_wait(&cond, lock.mtx);
  }
};
} // namespace psx
