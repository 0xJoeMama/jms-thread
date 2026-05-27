#include <memory>
#include <variant>

#include "args.hpp"
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

struct WorkerCount : RequiredArg<IntArg, WorkerCount> {
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

namespace {
int run(const CoordConfig &cfg) {
  auto listener = psx::TcpListener::listen(cfg.get<Port>());
  std::cout << "INFO: now accepting connections on port " << cfg.get<Port>()
            << std::endl;

  auto pool = jms::ThreadPool(cfg.get<WorkerCount>());

  for (;;) {
    auto sock = std::make_shared<psx::TcpSocket>(listener.accept());
    psx::Thread<std::monostate>::start_detached([sock = sock]() {
      while (1) {
        try {
          auto buf = sock->read_buf();
          auto cmd = buf.read_string();

          psx::WriteBuf res_buf;
          res_buf.write("Hello there");

          sock->write_buf(std::move(res_buf));
        } catch (const psx::SocketException &e) {
          std::cerr << "ERROR: socket error has been thrown: " << e.what()
                    << std::endl;
          break;
        } catch (const std::exception &e) {
          std::cerr << "ERROR: spurious error has been thrown: " << e.what()
                    << std::endl;
          break;
        }
      }

      return std::monostate{};
    });
  }

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
  } catch (const error::ParsingException &e) {
    if (cfg.get<Help>()) {
      cfg.print_usage(prog_name, std::cout);
      return 0;
    }

    std::cerr << "ERROR: failed to parse command line arguments: " << e.what()
              << std::endl;

    cfg.print_usage(prog_name, std::cerr);
    return 1;
  }

  return 0;
}
