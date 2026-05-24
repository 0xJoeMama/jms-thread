#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "util.hpp"

namespace psx {
class SocketException {
  std::string msg;

public:
  SocketException(std::string what) : msg(std::move(what)) {}

  const char *what() const noexcept { return msg.data(); }
};

class OwnedFd {
  int fd;

public:
  OwnedFd(int fd) : fd(fd) {
    if (fd < 0)
      throw SocketException(
          sutil::concat("invalid file descriptor:", strerror(errno)));
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

class PacketBuf {
  std::vector<uint8_t> bytes;

  void write_buffer(const void *v, size_t len) {
    size_t old_len = bytes.size();
    bytes.resize(old_len + len);
    memcpy(bytes.data() + old_len, v, len);
  }

public:
  PacketBuf() {}
  PacketBuf(const PacketBuf &) = delete;
  PacketBuf(std::vector<uint8_t> bytes) noexcept : bytes(std::move(bytes)) {}

  size_t size() const { return bytes.size(); }

  template <typename T> void write(const T &t);

  void write(const uint32_t &i) {
    uint32_t neti = htonl(i);
    write_buffer(&i, sizeof(neti));
  }

  void write(const std::string &s) {
    // TODO: this limits maximum string size of 4 GB which I doubt we will ever
    // go over
    write(static_cast<uint32_t>(s.size()));
    write_buffer(s.data(), s.size());
  }

  const void *byte_buf() const {
    return reinterpret_cast<const void *>(bytes.data());
  }
};

class TcpSocket {
  OwnedFd fd;

  void write_all(const void *data, size_t len) {
    size_t total_written = 0;

    while (total_written < len) {
      ssize_t written = write(fd.get_fd(), data, len - total_written);

      if (written < 0)
        throw SocketException(
            sutil::concat("failed to write data to socket:", strerror(errno)));

      total_written += written;
    }
  }

  void read_all(void *data, size_t len) {
    size_t total_read = 0;

    while (total_read < len) {
      ssize_t readed = read(fd.get_fd(), data, len - total_read);

      if (readed < 0)
        throw SocketException(
            sutil::concat("failed to write data to socket:", strerror(errno)));

      total_read += readed;
    }
  }

public:
  TcpSocket(OwnedFd fd) noexcept : fd(std::move(fd)) {}

  static TcpSocket connect(std::string ip_addr, short port) {
    // resolve domain name to ip
    addrinfo *info = nullptr;
    addrinfo hint{};

    hint.ai_family = AF_INET;
    hint.ai_socktype = SOCK_STREAM;
    int res;

    port = htons(port);

    if ((res = getaddrinfo(ip_addr.data(), reinterpret_cast<char *>(&port),
                           &hint, &info)) < 0)
      throw SocketException(sutil::concat("failed to resolve socket address",
                                          ip_addr, ":", gai_strerror(res)));

    for (addrinfo *ai = info; ai; ai = ai->ai_next) {
      OwnedFd fd = socket(AF_INET, SOCK_STREAM, 0);

      if (::connect(fd.get_fd(), ai->ai_addr, ai->ai_addrlen) < 0)
        throw SocketException(
            sutil::concat("failed to connect to TCP endpoint at", ip_addr,
                          "(port", port, "):", strerror(errno)));

      return TcpSocket(std::move(fd));
    }

    throw SocketException(sutil::concat(
        "failed to find appropriate connection address for TCP socket:",
        ip_addr, ":", ntohs(port)));
  }

  void write_buf(PacketBuf buf) {
    size_t len = buf.size();
    const void *data = buf.byte_buf();
    write_all(&len, sizeof(len));
    write_all(data, len);
  }

  PacketBuf read_buf() {
    size_t len;
    std::vector<uint8_t> bytes;

    read_all(&len, sizeof(len));
    bytes.resize(len);
    read_all(bytes.data(), len);

    return PacketBuf(std::move(bytes));
  }
};

class TcpListener {
  OwnedFd fd;

  TcpListener(OwnedFd fd) noexcept : fd(std::move(fd)) {}

  void write_all(const void *data, size_t len) {
    size_t total_written = 0;

    while (total_written < len) {
      ssize_t written = write(fd.get_fd(), data, len - total_written);

      if (written < 0)
        throw SocketException(
            sutil::concat("failed to write data to socket:", strerror(errno)));

      total_written += written;
    }
  }

  void read_all(void *data, size_t len) {
    size_t total_read = 0;

    while (total_read < len) {
      ssize_t readed = read(fd.get_fd(), data, len - total_read);

      if (readed < 0)
        throw SocketException(
            sutil::concat("failed to write data to socket:", strerror(errno)));

      total_read += readed;
    }
  }

public:
  static TcpListener listen(short port, int n = 128) {
    port = htons(port);

    OwnedFd fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (::bind(fd.get_fd(), reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) <
        0)
      throw SocketException(sutil::concat(
          "failed to bind TCP socket to port:", port, strerror(errno)));

    if (::listen(fd.get_fd(), n) < 0)
      throw SocketException(
          sutil::concat("failed to listen to TCP connections on port:", port,
                        strerror(errno)));

    return fd;
  }

  TcpSocket accept() {
    return TcpSocket(OwnedFd(::accept(fd.get_fd(), nullptr, nullptr)));
  }
};
} // namespace psx
