#pragma once

#include <cstring>
#include <functional>
#include <iostream>
#include <optional>
#include <pthread.h>
#include <string>

#include "util.hpp"

namespace psx {
class ThreadException {
  std::string _what;

public:
  ThreadException(std::string s) : _what(std::move(s)) {}
  const char *what() noexcept { return _what.data(); }
};

class JoinException : public ThreadException {
public:
  JoinException(std::string s) : ThreadException(std::move(s)) {}
};

class CreateException : public ThreadException {
public:
  CreateException(std::string s) : ThreadException(std::move(s)) {}
};

class DetachException : public ThreadException {
public:
  DetachException(std::string s) : ThreadException(std::move(s)) {}
};

template <typename T> class Thread {
  std::optional<pthread_t> t;

  static void *start_thread(void *arg) {
    auto *alloced_f = reinterpret_cast<std::function<T()> *>(arg);
    auto *res = new T(std::move((*alloced_f)()));
    delete alloced_f;

    return res;
  }

public:
  Thread(std::function<T()> f) {
    auto *alloced_f = new std::function<T()>(std::move(f));

    pthread_t pthread;
    int ret;
    if ((ret = pthread_create(&pthread, nullptr, start_thread, alloced_f)))
      throw CreateException(
          sutil::concat("failed to create new thread: ", strerror(ret)));

    t = pthread;
  }

  Thread(const Thread &) = delete;
  Thread(Thread &&other) noexcept : t(other.t) { other.t = -1; }

  Thread &operator=(const Thread &) = delete;
  Thread &operator=(Thread &&) = delete;

  T join() {
    void *res = nullptr;
    if (pthread_join(*t, &res))
      throw JoinException(sutil::concat("failed to join thread ", *t));

    t = -1;
    T *t_res = reinterpret_cast<T *>(res);
    T t_out = std::move(*t_res);
    delete t_res;

    return std::move(t_out);
  }

  void detach() {
    if (!t)
      throw DetachException("cannot detach an already moved thread");

    int ret;
    if ((ret = pthread_detach(*t)))
      throw DetachException(
          sutil::concat("failed to detach thread", *t, ":", strerror(ret)));

    t = std::nullopt;
  }

  ~Thread() noexcept {
    if (!t)
      return;

    if (pthread_cancel(*t))
      std::cerr << "WARN: failed to cancel thread " << *t << std::endl;
  }
};
} // namespace psx
