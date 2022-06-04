#include <algorithm>

namespace foo {

struct Compare {
  bool operator()(int, int) {
    return false;
  }
};

void foo(int *p) {
  std::sort(p, p + 100, Compare());
}
