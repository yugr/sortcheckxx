#include <vector>
#include <algorithm>

struct A {
  int x;
  bool operator<(const A& that) const {
    return x < that.x;
  }

};

void foo() {
  std::vector<A> v;
  std::lower_bound(v.begin(), v.end(), A());
}
