#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <sys/wait.h>

#include "coord_commands.hpp"
#include "job.hpp"
#include "thread_pool.hpp"
#include "util.hpp"

using namespace jms;

namespace {
std::string pretty_job_status(const Job &job) {
  std::string status;
  switch (job.get_status()) {
  case JobStatus::Queued: {
    status = "Queued (waiting in job queue)";
  } break;
  case JobStatus::Active: {
    auto start_time = job.get_start_time();
    status = sutil::format(
        "Active (running for {} sec)",
        chr::duration_cast<chr::seconds>(chr::system_clock::now() - start_time)
            .count());
  } break;
  case JobStatus::Finished: {
    status = "Finished";
  } break;
  default:
    unreachable();
  }

  return sutil::format("JobID {} Status: {}", job.get_id(), status);
}
} // namespace

std::string Submit::execute(CoordView &view) {
  JobId new_id = SIZE_MAX;
  {
    auto lock = view.get_jobs();
    new_id = lock->size();
    lock->emplace_back(new_id, std::move(cmd));
  }

  view.get_worker_pool().enqueue([job_id = new_id, &jobs = view.get_job_list(),
                                  &store = view.get_store(),
                                  &shutting_down = view.shutdown_flag()] {
    if (*shutting_down.lock())
      return ThreadResult::Break;

    pid_t job_pid = -1;
    {
      auto lock = jobs.lock();
      auto &job = lock->at(job_id);
      job_pid = job.launch(store);
    }

    int exit;
    int res;
    while ((res = waitpid(job_pid, &exit, 0)) < 0 && errno == EINTR)
      ;

    if (res < 0)
      error(sutil::format("failed to wait for child with pid {}: {}", job_pid,
                          strerror(errno)));

    {
      auto lock = jobs.lock();
      auto &job = lock->at(job_id);
      job.finished();
    }

    return ThreadResult::Continue;
  });

  return sutil::format("JobId: {}", new_id);
}

std::string Status::execute(CoordView &view) {
  auto lock = view.get_jobs();
  try {
    auto &job = lock->at(job_id);
    return pretty_job_status(job);
  } catch (const std::out_of_range &e) {
    return sutil::format("JobID {} not found", job_id);
  }
}

std::string StatusAll::execute(CoordView &view) {
  auto jobs = view.get_jobs();
  auto low = std::lower_bound(jobs->cbegin(), jobs->cend(), since);

  std::ostringstream os{};
  for (auto it = low; it != jobs->cend(); it++)
    os << pretty_job_status(*it) << std::endl;

  return os.str();
}

std::string ShowActive::execute(CoordView &view) {
  auto lock = view.get_jobs();

  std::ostringstream os{};

  for (const auto &job : *lock) {
    if (job.get_status() != JobStatus::Active)
      continue;

    os << sutil::format("JobID {}", job.get_id()) << std::endl;
  }

  return os.str();
}

std::string ShowFinished::execute(CoordView &view) {
  auto lock = view.get_jobs();

  std::ostringstream os{};

  for (const auto &job : *lock) {
    if (job.get_status() != JobStatus::Finished)
      continue;

    os << sutil::format("JobID {}", job.get_id()) << std::endl;
  }

  return os.str();
}

std::string ShowWorkers::execute(CoordView &) { todo("not implemented"); }

// imposed lock order is:
// job metadata lock -> shutting_down lock -> thread pool internal lock
std::string Shutdown::execute(CoordView &view) {
  auto job_lock = view.get_jobs();
  view.shutdown();

  size_t total_jobs = job_lock->size();
  size_t active_jobs =
      std::count_if(job_lock->cbegin(), job_lock->cend(), [](const auto &it) {
        return it.get_status() == JobStatus::Active;
      });

  size_t queued_jobs = view.get_worker_pool().queue_size();

  return sutil::format("Served {} jobs, {} were running, {} were queued",
                       total_jobs, active_jobs, queued_jobs);
}
