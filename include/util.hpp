#pragma once

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

#define unreachable __builtin_unreachable
#define todo(s) (assert(0 && s && "not implemented yet"))

namespace config {
constexpr int OPEN_FDS_ON_WALK = 4;
constexpr const char *JMS_IN = "jms_in";
constexpr const char *JMS_OUT = "jms_out";
constexpr const char *SHUTDOWN = "shutdown";
} // namespace config

template <typename... Ts> struct Match : Ts... {
  using Ts::operator()...;
};

template <typename... Ts> Match(Ts...) -> Match<Ts...>;

template <typename T> void drop(T &&) {}

namespace sutil {
template <typename I> std::string concat_iter(const I &v) {
  std::ostringstream os{};

  for (const auto &i : v)
    os << i << " ";

  return os.str();
}

template <char delim = ' ', typename... Args>
std::string concat(const Args &...args) {
  std::ostringstream os{};
  (
      [&]() {
        if (os.tellp() != 0)
          os << delim;

        os << args;
      }(),
      ...);
  return os.str();
}

class FormatException : public std::runtime_error {
  using std::runtime_error::runtime_error;
};

template <typename... T>
constexpr inline void format_inner(std::string_view::const_iterator begin,
                                   const std::string_view::const_iterator &end,
                                   std::ostringstream &os, T... rest);

template <>
constexpr inline void format_inner(std::string_view::const_iterator begin,
                                   const std::string_view::const_iterator &end,
                                   std::ostringstream &os) {
  for (auto it = begin; it != end; it++) {
    if (*it != '{') {
      os << *it;
      continue;
    }

    it++;
    if (it == end)
      throw FormatException("Unclosed bracket '{'");

    switch (*it) {
    case '{':
      os << '{';
      break;
    case '}':
      throw FormatException("Not enough format arguments");
    default:
      throw FormatException(
          "Either close the format bracket or escape an opening bracket");
    }
  }
}

template <typename First, typename... T>
constexpr inline void format_inner(std::string_view::const_iterator begin,
                                   const std::string_view::const_iterator &end,
                                   std::ostringstream &os, First first,
                                   T... rest) {
  for (auto it = begin; it != end; it++) {
    if (*it != '{') {
      os << *it;
      continue;
    }

    it++;
    if (it == end)
      throw FormatException("Unclosed bracket '{'");

    switch (*it) {
    case '{':
      os << '{';
      break;
    case '}':
      os << first;
      it++;
      format_inner(it, end, os, rest...);
      return;
    default:
      throw FormatException(
          "Either close the format bracket or escape an opening bracket");
    }
  }
}

template <typename... T>
inline std::string format(std::string_view s, T... args) {
  std::ostringstream os;
  format_inner<T...>(s.begin(), s.end(), os, args...);

  return os.str();
}
} // namespace sutil

#define info(x)                                                                \
  do {                                                                         \
    std::cout << "INFO: " << (x) << std::endl;                                 \
  } while (0)

#define error(x)                                                               \
  do {                                                                         \
    std::cerr << "ERROR: " << (x) << std::endl;                                \
  } while (0)

#define ensure(x)                                                              \
  ((x) ? (void)0                                                               \
       : ((std::cerr << "failed to ensure precondition at " __FILE__ ":"       \
                     << __LINE__ << ": " << __PRETTY_FUNCTION__ << std::endl),    \
          abort()))

#ifndef NDEBUG
#define dbg(x)                                                                 \
  do {                                                                         \
    std::cerr << "dbg: " << __FILE__ << ":" << __LINE__ << ": " << (x)         \
              << std::endl;                                                    \
  } while (0)

#define warn(x)                                                                \
  do {                                                                         \
    std::cerr << "WARN: " << (x) << std::endl;                                 \
  } while (0)
#else
#define dbg(x) do {} while(0)
#define warn(x) do {} while(0)
#endif
