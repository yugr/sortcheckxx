// Copyright 2022 Yury Gribov
// 
// Use of this source code is governed by MIT license that can be
// found in the LICENSE.txt file.

#include <map>

struct Compare {
  bool operator()(int lhs, int rhs) {
    return lhs == rhs;
  }
};

int main() {
  std::map<int, int, Compare> v;
  v[1] = 1;
  v[3] = 3;
  v[2] = 2;
  v.clear();
  return 0;
}
