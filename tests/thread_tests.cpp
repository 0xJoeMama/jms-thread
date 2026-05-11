#include "mutex.hpp"
#include "thread.hpp"

int main(void) {
  psx::Mutex<int> m(0);

  psx::Thread<int> t([&]() {
    for (int i = 0; i < 10000; i++) {
      *m.lock() += 1;
    }

    return 67;
  });

  for (int i = 0; i < 10000; i++)
    *m.lock() += 1;

  assert(t.join() == 67);
  assert(*m.lock() == 20000);
  return 0;
}
