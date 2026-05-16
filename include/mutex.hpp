#pragma once
#include <algorithm>
#include <cassert>
#include <pthread.h>
#include <utility>

namespace psx {
template <typename T> class Mutex;

template <typename T> class Lock {
  pthread_mutex_t *mtx;
  T *t;

  Lock(pthread_mutex_t *mtx, T *t) : mtx(mtx), t(t) {
    assert(pthread_mutex_lock(mtx) >= 0);
  }

  Lock(const Lock &) = delete;
  Lock &operator=(const Lock &) = delete;
  friend class Mutex<T>;

public:
  Lock(Lock &&other) noexcept : mtx(other.mtx), t(other.t) {
    other.mtx = nullptr;
    other.t = nullptr;
  }

  Lock &operator=(Lock &&other) noexcept {
    if (&other == this)
      return *this;

    mtx = std::exchange(other.mtx, nullptr);
    t = std::exchange(other.t, nullptr);
  }

  T &operator*() { return *t; }
  T *operator->() { return t; }

  ~Lock() noexcept {
    if (!mtx)
      return;
    assert(pthread_mutex_unlock(mtx) >= 0);
  }
};

template <typename T> class Mutex {
  pthread_mutex_t mtx;
  T t;

public:
  template <typename... Args> Mutex(Args... args) : t(std::move(args)...) {
    // this never fails
    pthread_mutex_init(&mtx, nullptr);
  }

  Lock<T> lock() { return Lock(&mtx, &t); }

  ~Mutex() noexcept { assert(pthread_mutex_destroy(&mtx) >= 0); }
};

} // namespace psx
