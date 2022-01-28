// Copyright 2022 Yury Gribov
// 
// Use of this source code is governed by MIT license that can be
// found in the LICENSE.txt file.

#include <algorithm>
#include <vector>

// Use non-default comparison to trigger instrumentation
struct Compare {
  bool operator()(int lhs, int rhs) {
    return lhs < rhs;
  }
};

int main() {
  std::vector<int> v;
  // Vector is not correctly sorted
  // but partiality check alone does not detect it.
  v.push_back(1);
  v.push_back(3);
  v.push_back(2);
  std::lower_bound(v.begin(), v.end(), 0, Compare());
  return 0;
}
