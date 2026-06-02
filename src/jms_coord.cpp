#include <memory>
#include <variant>

#include "args.hpp"
#include "command.hpp"
#include "coord_commands.hpp"
#include "mutex.hpp"
#include "socket.hpp"
#include "thread.hpp"
#include "thread_pool.hpp"

struct Port : RequiredArg<IntArg, Port> {
  static constexpr const char *name = "port";
  static constexpr const char shortname = 'p';
  static constexpr const char *desc = "Select the port to run the server on";
};

struct Runpath : RequiredArg<StringArg, Runpath> {
  static constexpr const char *name = "run-path";
  static constexpr const char shortname = 'l';
  static constexpr const char *desc = "Directory to use for coordinator files";
};

struct WorkerCount : RequiredArg<PositiveIntArg, WorkerCount> {
  static constexpr const char *name = "workers";
  static constexpr const char shortname = 'n';
  static constexpr const char *desc = "Maximum number of active workers";
};

struct CoordConfig : Command<CoordConfig, Field<Port>, Field<Runpath>,
                             Field<WorkerCount>, Field<Help>> {
  static constexpr const char *desc =
      "Coordinator process for the Job Management System";
  static constexpr const char *signature =
      "-p [port number] -l [run path] -n [n-workers]";

  static constexpr const char *example = "-p 6767 -l run -n 12";
};

using namespace jms;

namespace {
void nothing_handler(int) {}

int run(const CoordConfig &cfg) {
  auto listener = psx::TcpListener::listen(cfg.get<Port>());
  std::cout << sutil::format("INFO: now accepting connections on port {}",
                             cfg.get<Port>())
            << std::endl;

  sigset_t blocked;
  sigemptyset(&blocked);
  sigaddset(&blocked, SIGALRM);
  sigset_t old;

  sigprocmask(SIG_BLOCK, &blocked, &old);
  ThreadPool pool(cfg.get<WorkerCount>());
  sigprocmask(SIG_UNBLOCK, &blocked, &old);

  struct sigaction sa{};
  sa.sa_handler = nothing_handler;
  sa.sa_flags = SA_INTERRUPT;
  sigaction(SIGALRM, &sa, nullptr);

  CoordView view(pool, cfg.get<Runpath>());

  while (!*view.shutdown_flag().lock()) {
    try {
      auto sock = std::make_shared<psx::TcpSocket>(listener.accept());

      psx::Thread<std::monostate>::start_detached([sock = sock, &view] {
        while (!*view.shutdown_flag().lock()) {
          try {
            auto buf = sock->read_buf();
            auto cmd_str = buf.read_string();

            auto cmd =
                parse_command<CoordView, Submit, Status, StatusAll, ShowActive,
                              ShowFinished, ShowWorkers, Shutdown>(
                    std::move(cmd_str));

            auto response = cmd->execute(view);
            sock->write_string(response);
          } catch (const jms::error::CommandParseError &e) {
            psx::WriteBuf res_buf;
            res_buf.write(e.what());
            sock->write_buf(std::move(res_buf));
          } catch (const psx::SocketException &e) {
            error(sutil::format("socket error has been thrown: {}", e.what()));
            break;
          } catch (const std::exception &e) {
            error(
                sutil::format("spurious error has been thrown: {}", e.what()));
            break;
          }
        }

        return std::monostate{};
      });
    } catch (const psx::SocketCreationException &e) {
      if (e.get_errnum() != EINTR) {
        error(sutil::format("could not establish socket connection: {}",
                            e.what()));
        break;
      }
    }
  }

  pool.shutdown();
  pool.join();

  return 0;
}
} // namespace

int main(int argc, const char **argv) {
  CoordConfig cfg;
  const char *prog_name = *(argv++);

  (void)prog_name, (void)argc;

  try {
    Argv arg(argv);
    cfg.parse(arg);

    if (cfg.get<Help>()) {
      cfg.print_usage(prog_name, std::cout);
      return 0;
    }

    return run(cfg);
  } catch (const ::error::ParsingException &e) {
    if (cfg.get<Help>()) {
      cfg.print_usage(prog_name, std::cout);
      return 0;
    }

    error(
        sutil::format("failed to parse command line arguments: {}", e.what()));
    cfg.print_usage(prog_name, std::cerr);
    return 1;
  }

  return 0;
}
