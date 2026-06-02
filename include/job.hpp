#pragma once

#include <chrono>
#include <cstddef>
#include <string>
#include <unistd.h>
#include <vector>

#include "file_store.hpp"

namespace jms {
enum class JobStatus {
  Queued,
  Active,
  Finished,
};

using JobId = size_t;

namespace chr = std::chrono;
using Timepoint = chr::time_point<std::chrono::system_clock>;

class Job {
protected:
  const JobId id;
  const chr::time_point<chr::system_clock> submission_time;
  std::vector<std::string> cmd;
  chr::time_point<chr::system_clock> start;
  JobStatus status;

public:
  Job(JobId id, std::vector<std::string> cmd) noexcept;
  Job(const Job &) noexcept = delete;
  Job(Job &&) noexcept = default;
  virtual ~Job() = default;

  Job &operator=(const Job &) = delete;
  Job &operator=(Job &&) = delete;

  JobStatus get_status() const { return status; }
  auto get_id() const { return id; }
  auto get_submission_time() const { return submission_time; }
  auto get_start_time() const { return start; }
  auto get_cmd() const { return cmd; }

  pid_t launch(const jms::FileStore &store);
  void finished();

  bool operator<(const Timepoint &t) const { return submission_time < t; }
};
} // namespace jms
