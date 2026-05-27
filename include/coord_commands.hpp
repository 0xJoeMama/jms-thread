#pragma once

#include <chrono>
#include <vector>

#include "command.hpp"
#include "config.hpp"

namespace jms {
class CoordExecutor {};

class Submit : public jms::Command<CoordExecutor> {
  std::string path;
  std::vector<std::string> args;

public:
  explicit Submit(PeekableStream &peek)
      : jms::Command<CoordExecutor>(config::mnem::SUBMIT, peek) {
    if (!(peek.stream() >> path))
      throw error::CommandParseError("no program path was provided to submit");

    std::string s;
    while (peek.stream() >> s)
      args.emplace_back(std::move(s));
  }

  std::string execute(CoordExecutor &ex) override;
};

struct Shutdown : public jms::Command<CoordExecutor> {
  explicit Shutdown(PeekableStream &peek)
      : jms::Command<CoordExecutor>(config::mnem::SHUTDOWN, peek) {}
  std::string execute(CoordExecutor &ex) override;
};

struct ShowWorkers : public jms::Command<CoordExecutor> {
  explicit ShowWorkers(PeekableStream &peek)
      : jms::Command<CoordExecutor>(config::mnem::SHOW_WORKERS, peek) {}
  std::string execute(CoordExecutor &ex) override;
};

struct ShowActive : public jms::Command<CoordExecutor> {
  explicit ShowActive(PeekableStream &peek)
      : jms::Command<CoordExecutor>(config::mnem::SHOW_ACTIVE, peek) {}
  std::string execute(CoordExecutor &ex) override;
};

struct ShowFinished : public jms::Command<CoordExecutor> {
  explicit ShowFinished(PeekableStream &peek)
      : jms::Command<CoordExecutor>(config::mnem::SHOW_FINISHED, peek) {}
  std::string execute(CoordExecutor &ex) override;
};

struct Status : public jms::JobIdCommand<CoordExecutor> {
  explicit Status(PeekableStream &peek)
      : jms::JobIdCommand<CoordExecutor>(config::mnem::STATUS, peek) {}
  std::string execute(CoordExecutor &ex) override;
};

namespace chr = std::chrono;
using Timepoint = std::chrono::time_point<std::chrono::system_clock>;

struct StatusAll : public jms::Command<CoordExecutor> {
  Timepoint since;

  explicit StatusAll(PeekableStream &peek)
      : jms::Command<CoordExecutor>(config::mnem::STATUS_ALL, peek) {
    size_t n;
    if (peek.stream() >> n) {
      since = chr::system_clock::now() - chr::seconds(n);
    } else {
      since = Timepoint::min();
    }
  }

  std::string execute(CoordExecutor &ex) override;
};
} // namespace jms
