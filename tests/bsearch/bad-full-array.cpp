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
  // Vector is not correctly sorted
  // but partiality check alone does not detect it.
  int v[] = {1, 3, 2};
  std::binary_search(&v[0], &v[3], 0, Compare());
  return 0;
}
