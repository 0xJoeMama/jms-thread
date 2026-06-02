#pragma once

#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <string_view>

#include "fd.hpp"

namespace jms {
class Dir {
  std::string root;

public:
  Dir(std::string_view root, bool delete_if_exists = false);
  Dir(const Dir &) = delete;

  Dir &operator=(const Dir &) = delete;
  const std::string &get_root_path() const { return root; }
};

class FileStore {
  const Dir root_dir;

  FileStore(bool delete_if_exists, std::string_view root_path);

public:
  explicit FileStore(std::string_view root_path);

  const std::string &get_root() const { return root_dir.get_root_path(); }
  FileStore substore(std::string_view name) const;

  psx::OwnedFd touch(std::string_view path, int options = O_RDWR);
};
} // namespace jms
