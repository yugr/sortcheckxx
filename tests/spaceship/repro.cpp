// Copyright 2023 Yury Gribov
// 
// Use of this source code is governed by MIT license that can be
// found in the LICENSE.txt file.

#include <compare>
#include <algorithm>
#include <vector>

struct A {
  int x;
  A(int x): x(x) {}
  std::weak_ordering operator <=>(A rhs) const {
    return x == rhs.x ? std::weak_ordering::less : std::weak_ordering::equivalent;
  }
};

int main() {
  std::vector<A> v;
  v.push_back(1);
  v.push_back(3);
  v.push_back(2);
  std::sort(v.begin(), v.end());
  return 0;
}
