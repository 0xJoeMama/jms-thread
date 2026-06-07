#include <iostream>
#include <fcntl.h>

int main(void) {
  int fd = open("/dev/null", O_RDONLY);

  std::cout << "Largest fd is " << fd << std::endl;

  return 0;
}
