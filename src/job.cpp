#include "job.hpp"
#include "fd.hpp"
#include "file_store.hpp"
#include "util.hpp"
#include <algorithm>

using namespace jms;

namespace {
inline std::string make_output_dir(JobId job_id, pid_t pid) {
  std::ostringstream os{};

  std::time_t time = std::time(nullptr);
  char time_string[std::size("yyyymmdd_hhmmss")];
  std::strftime(time_string, sizeof(time_string), "%Y%m%d_%H%M%S",
                std::localtime(&time));
  os << "outputs_" << job_id << "_" << pid << "_" << time_string;

  return os.str();
}

[[noreturn]] void run_command(JobId id, std::vector<std::string> cmd,
                              const FileStore &store) noexcept {
  pid_t pid = getpid();
  // first set-up file descriptors
  FileStore root = store.substore(make_output_dir(id, pid));

  if (chdir(root.get_root().c_str()) < 0) {
    error(sutil::format("failed to chdir into {}", root.get_root()));
    exit(EXIT_FAILURE);
  }

  std::string fmt = sutil::concat<'_'>("stdout", id);
  {
    psx::OwnedFd std_out(open(fmt.c_str(), O_WRONLY | O_CREAT, 0644));
    dbg(sutil::format("STDOUT is {}", std_out.get_fd()));
    if (dup2(std_out.get_fd(), STDOUT_FILENO) < 0) {
      error(sutil::format(
          "failed to set file descriptor for stdout for process {}: {}", pid,
          strerror(errno)));
      exit(EXIT_FAILURE);
    }
  }

  fmt = sutil::concat<'_'>("stderr", id);
  {
    psx::OwnedFd std_err(open(fmt.c_str(), O_WRONLY | O_CREAT, 0644));
    if (dup2(std_err.get_fd(), STDERR_FILENO) < 0) {
      error(sutil::format(
          "failed to set file descriptor for stderr for process {}: {}", pid,
          strerror(errno)));
      exit(EXIT_FAILURE);
    }
  }

  // setup arguments and call into execve
  std::vector<char *> argv;
  std::transform(
      cmd.begin(), cmd.end(), std::back_inserter(argv),
      [](const auto &arg) { return const_cast<char *>(arg.c_str()); });
  argv.push_back(nullptr);

  ::execvp(argv[0], argv.data());
  std::cerr << "failed to execute program " << argv[0] << " with arguments "
            << sutil::concat_iter(cmd) << std::endl;

  exit(EXIT_FAILURE);
}
} // namespace

Job::Job(JobId id, std::vector<std::string> cmd) noexcept
    : id(id), submission_time(chr::system_clock::now()), cmd(std::move(cmd)),
      status(JobStatus::Queued) {}

pid_t Job::launch(const FileStore &store) {
  dbg(sutil::format("Running command {}", sutil::concat_iter(std::move(cmd))));

  pid_t pid;

  switch ((pid = fork())) {
  case 0:
    run_command(id, std::move(cmd), store);
  case -1:
    throw psx::ErrnoException();
  default: {
    status = JobStatus::Active;
    start = chr::system_clock::now();

    return pid;
  } break;
  };
}

void Job::finished() { status = JobStatus::Finished; }
