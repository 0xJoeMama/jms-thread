#include <exception>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <unistd.h>

#include "args.hpp"
#include "socket.hpp"

struct Hostname : RequiredArg<StringArg, Hostname> {
  static constexpr const char *name = "host";
  static constexpr const char shortname = 'h';
  static constexpr const char *desc = "IP address for the coordinator servef";
};

struct Port : RequiredArg<IntArg, Port> {
  static constexpr const char *name = "port";
  static constexpr const char shortname = 'p';
  static constexpr const char *desc = "TCP port for the coordinator server";
};

struct OpFile : NamedArg<StringArg, OpFile> {
  static constexpr const char *name = "operations";
  static constexpr const char shortname = 'o';
  static constexpr const char *desc = "File containing operations operations";
};

struct ConsoleConfig : Command<ConsoleConfig, Field<Hostname>, Field<Port>,
                               Field<OpFile>, Field<Help>> {
  static constexpr const char *desc = "Job Management System Console";
  static constexpr const char *signature =
      "-h <hostname> -p <port> (-o <operations-file>)";
  static constexpr const char *example =
      "-h localhost -p 6767 -o inputs/simple.txt";
};

namespace {
bool handle_command(psx::TcpSocket &sock, const std::string &op) {
  psx::WriteBuf buf;
  buf.write(op);

  sock.write_buf(std::move(buf));

  auto rbuf = sock.read_buf();
  std::cout << rbuf.read_string() << std::endl;
  return true;
}

std::ostream &print_prompt(size_t &line) {
  if (isatty(STDOUT_FILENO)) {
    return std::cout << line++ << ": jms_console > ";
  } else {
    return std::cout;
  }
}

void send_operations(const std::string &path, psx::TcpSocket &sock,
                     size_t &command_cnt) {
  std::cout << "Operation file " << path << " was provided. Sending..."
            << std::endl;
  std::ifstream fs(path);

  std::string op;
  while (std::getline(fs, op)) {
    if (op.empty())
      continue;

    print_prompt(command_cnt) << op << std::endl;
    handle_command(sock, op);
  }
}

void run(ConsoleConfig cfg) {
  psx::TcpSocket sock = psx::TcpSocket::connect(
      std::string(cfg.get<Hostname>()), cfg.get<Port>());

  size_t command_cnt = 0;

  if (!cfg.get<OpFile>().empty())
    send_operations(std::string(cfg.get<OpFile>()), sock, command_cnt);

  std::string op{};
  std::cin.exceptions(std::ios_base::badbit);

  while (print_prompt(command_cnt), std::getline(std::cin, op))
    if (!op.empty())
      if (!handle_command(sock, op))
        break;

  std::cout << "EOF given. Terminating..." << std::endl;
}
} // namespace

int main(int, const char **argv) {
  ConsoleConfig cfg{};

  try {
    Argv args(argv + 1);
    cfg.parse(args);

    if (cfg.get<Help>()) {
      cfg.print_usage(*argv, std::cout);
      return 0;
    }

    run(std::move(cfg));
    return 0;
  } catch (const error::ParsingException &e) {
    if (cfg.get<Help>()) {
      cfg.print_usage(*argv, std::cout);
      return 0;
    }

    error(sutil::format("failed to parse command line: {}", e.what()));
    cfg.print_usage(*argv, std::cerr);
    return 1;
  } catch (const std::exception &e) {
    error(e.what());
    return 1;
  }
}
