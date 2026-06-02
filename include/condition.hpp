#pragma once

#include <pthread.h>

#include "mutex.hpp"

namespace psx {
class Condition {
  pthread_cond_t cond;

public:
  Condition() { pthread_cond_init(&cond, nullptr); }
  ~Condition() noexcept { ensure(pthread_cond_destroy(&cond) >= 0); }
  void signal() noexcept { pthread_cond_signal(&cond); }
  void broacast() noexcept { pthread_cond_broadcast(&cond); }

  template <typename T, typename Fn> Lock<T> wait_until(Lock<T> lock, Fn f) {
    while (!f(*lock))
      pthread_cond_wait(&cond, lock.mtx);

    return lock;
  }
};
} // namespace psx
