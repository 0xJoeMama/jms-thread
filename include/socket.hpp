#include <arpa/inet.h>
#include <cstring>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "fd.hpp"
#include "util.hpp"

namespace psx {
class SocketException : public std::exception {
  std::string msg;

public:
  SocketException(std::string what) : msg(std::move(what)) {}
  const char *what() const noexcept override { return msg.data(); }
};

class SockEof : public SocketException {
public:
  SockEof() : SocketException("Reached End of File") {}
};

class SocketCreationException : public ErrnoException {
public:
  using ErrnoException::ErrnoException;

  SocketCreationException(const ErrnoException &e)
      : SocketCreationException(e.get_errnum()) {}
};

class WriteBuf {
  std::vector<uint8_t> bytes;

  void write_buffer(const void *v, size_t len) {
    size_t old_len = bytes.size();
    bytes.resize(old_len + len);
    memcpy(bytes.data() + old_len, v, len);
  }

public:
  WriteBuf() {}
  WriteBuf(const WriteBuf &) = delete;
  WriteBuf(WriteBuf &&) = default;

  size_t size() const { return bytes.size(); }

  void write(const uint32_t &i) {
    uint32_t neti = htonl(i);
    write_buffer(&neti, sizeof(neti));
  }

  void write(std::string_view s) {
    // TODO: this limits maximum string size of 4 GB which I doubt we will ever
    // go over
    write(static_cast<uint32_t>(s.size()));
    write_buffer(s.data(), s.size());
  }

  const void *byte_buf() const {
    return reinterpret_cast<const void *>(bytes.data());
  }
};

class ReadBuf {
  std::vector<uint8_t> bytes;
  size_t cursor;

  void read_buffer(void *v, size_t len) {
    if (cursor + len > bytes.size())
      throw SocketException("Buffer overflow");

    memcpy(v, bytes.data() + cursor, len);
    cursor += len;
  }

public:
  ReadBuf(const ReadBuf &) = delete;
  ReadBuf(std::vector<uint8_t> bytes) noexcept
      : bytes(std::move(bytes)), cursor(0) {}

  ~ReadBuf() noexcept {
    if (cursor < bytes.size())
      warn("read buffer had remaining elements on exit");
  }

  uint32_t read_int() {
    uint32_t i;
    read_buffer(&i, sizeof(i));
    i = ntohl(i);

    return i;
  }

  std::string read_string() {
    uint32_t len = read_int();
    std::string s(len, 0);
    read_buffer(s.data(), len);
    return s;
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

      if (readed == 0)
        throw SockEof();

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

    std::string port_s = std::to_string(port);

    dbg(sutil::format("connecting to {}:{}", ip_addr, port_s));
    if ((res = getaddrinfo(ip_addr.data(), port_s.c_str(), &hint, &info)) < 0)
      throw SocketException(sutil::concat("failed to resolve socket address",
                                          ip_addr, ":", gai_strerror(res)));

    for (addrinfo *ai = info; ai; ai = ai->ai_next) {
      OwnedFd fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);

      if (::connect(fd.get_fd(), ai->ai_addr, ai->ai_addrlen) < 0) {
        warn(sutil::format("failed to resolve domain name: {}",
                           strerror(errno)));
        continue;
      }

      freeaddrinfo(info);
      return TcpSocket(std::move(fd));
    }

    freeaddrinfo(info);

    throw SocketException(sutil::format(
        "failed to find appropriate connection address for TCP socket: {}:{}",
        ip_addr, port_s));
  }

  void write_buf(WriteBuf buf) {
    size_t len = buf.size();
    const void *data = buf.byte_buf();
    write_all(&len, sizeof(len));
    write_all(data, len);
  }

  void write_string(std::string_view s) {
    WriteBuf buf;
    buf.write(s);
    write_buf(std::move(buf));
  }

  ReadBuf read_buf() {
    size_t len;
    std::vector<uint8_t> bytes;

    read_all(&len, sizeof(len));
    bytes.resize(len);
    read_all(bytes.data(), len);

    return ReadBuf(std::move(bytes));
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
    OwnedFd fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);

    int opt = 1;
    if (::setsockopt(fd.get_fd(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) <
        0)
      throw SocketException(sutil::concat("failed to set REUSEADDR for socket:",
                                          strerror(errno)));

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
    try {
      return TcpSocket(OwnedFd(::accept4(fd.get_fd(), nullptr, nullptr, SOCK_CLOEXEC)));
    } catch (const ErrnoException &e) {
      throw SocketCreationException(e);
    }
  }
};
} // namespace psx
