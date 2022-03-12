// Copyright 2022 Yury Gribov
// 
// Use of this source code is governed by MIT license that can be
// found in the LICENSE.txt file.

#include <set>

struct Compare {
  bool operator()(int lhs, int rhs) {
    return lhs == rhs;
  }
};

int main() {
  std::set<int, Compare> v;
  v.insert(1);
  v.insert(3);
  v.insert(2);
  v.clear();
  return 0;
}
