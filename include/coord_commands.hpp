#pragma once

#include <chrono>
#include <signal.h>
#include <vector>

#include "command.hpp"
#include "config.hpp"
#include "file_store.hpp"
#include "job.hpp"
#include "mutex.hpp"
#include "thread_pool.hpp"

namespace jms {
class CoordView {
  psx::Mutex<bool> &shutting_down;
  psx::Mutex<std::vector<Job>> &jobs;
  ThreadPool &workers;
  const FileStore &store;

public:
  CoordView(psx::Mutex<bool> &shutting_down, psx::Mutex<std::vector<Job>> &jobs,
            ThreadPool &workers, const FileStore &store)
      : shutting_down(shutting_down), jobs(jobs), workers(workers),
        store(store) {}

  void shutdown() {
    *shutting_down.lock() = true;
    // notify main thread that the shutting down flag has been changed
    // this will interrupt the accept call
    assert(sigqueue(getpid(), SIGALRM, {}) >= 0);
  }

  auto &shutdown_flag() { return shutting_down; }
  psx::Lock<std::vector<Job>> get_jobs() { return jobs.lock(); }

  ThreadPool &get_worker_pool() { return workers; }
  psx::Mutex<std::vector<Job>> &get_job_list() { return jobs; }
  auto &get_store() const { return store; }
};

class Submit : public jms::Command<CoordView> {
  std::vector<std::string> cmd;

public:
  explicit Submit(PeekableStream &peek)
      : jms::Command<CoordView>(config::mnem::SUBMIT, peek) {
    std::string s;
    if (!(peek.stream() >> s))
      throw error::CommandParseError("no program path was provided to submit");

    cmd.emplace_back(std::move(s));
    while (peek.stream() >> s)
      cmd.emplace_back(std::move(s));
  }

  std::string execute(CoordView &ex) override;
};

struct Shutdown : public jms::Command<CoordView> {
  explicit Shutdown(PeekableStream &peek)
      : jms::Command<CoordView>(config::mnem::SHUTDOWN, peek) {}
  std::string execute(CoordView &ex) override;
};

struct ShowWorkers : public jms::Command<CoordView> {
  explicit ShowWorkers(PeekableStream &peek)
      : jms::Command<CoordView>(config::mnem::SHOW_WORKERS, peek) {}
  std::string execute(CoordView &ex) override;
};

struct ShowActive : public jms::Command<CoordView> {
  explicit ShowActive(PeekableStream &peek)
      : jms::Command<CoordView>(config::mnem::SHOW_ACTIVE, peek) {}
  std::string execute(CoordView &ex) override;
};

struct ShowFinished : public jms::Command<CoordView> {
  explicit ShowFinished(PeekableStream &peek)
      : jms::Command<CoordView>(config::mnem::SHOW_FINISHED, peek) {}
  std::string execute(CoordView &ex) override;
};

struct Status : public jms::JobIdCommand<CoordView> {
  explicit Status(PeekableStream &peek)
      : jms::JobIdCommand<CoordView>(config::mnem::STATUS, peek) {}
  std::string execute(CoordView &ex) override;
};

struct StatusAll : public jms::Command<CoordView> {
  Timepoint since;

  explicit StatusAll(PeekableStream &peek)
      : jms::Command<CoordView>(config::mnem::STATUS_ALL, peek) {
    size_t n;
    if (peek.stream() >> n) {
      since = chr::system_clock::now() - chr::seconds(n);
    } else {
      since = Timepoint::min();
    }
  }

  std::string execute(CoordView &ex) override;
};
} // namespace jms
