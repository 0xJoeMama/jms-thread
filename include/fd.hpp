#pragma once

#include "util.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <unistd.h>

namespace psx {
class ErrnoException : public std::runtime_error {
  int errnum;

public:
  ErrnoException(int errnum)
      : std::runtime_error(strerror(errnum)), errnum(errnum) {}
  ErrnoException(std::string msg, int errnum)
      : std::runtime_error(sutil::format("{}: {}", msg, strerror(errnum))),
        errnum(errnum) {}
  ErrnoException(std::string msg) : psx::ErrnoException(msg, errno) {}
  ErrnoException() : ErrnoException(errno) {}

  int get_errnum() const { return errnum; }
};

class OwnedFd {
  int fd;

public:
  OwnedFd(int fd) : fd(fd) {
    if (fd < 0)
      throw ErrnoException();
  }

  OwnedFd(OwnedFd &&fd) noexcept : fd(fd.fd) { fd.fd = -1; }

  ~OwnedFd() noexcept {
    if (fd < 0)
      return;

    if (close(fd) < 0)
      std::cerr << strerror(errno) << std::endl;
  }

  OwnedFd &operator=(const OwnedFd &) = delete;
  OwnedFd &operator=(OwnedFd &&) = delete;

  int get_fd() const { return fd; }
};
} // namespace psx
