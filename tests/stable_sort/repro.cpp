// Copyright 2022 Yury Gribov
// 
// Use of this source code is governed by MIT license that can be
// found in the LICENSE.txt file.

#include <algorithm>
#include <vector>

struct Compare {
  bool operator()(int lhs, int rhs) {
    return lhs == rhs;
  }
};

int main() {
  std::vector<int> v;
  v.push_back(1);
  v.push_back(3);
  v.push_back(2);
  std::stable_sort(v.begin(), v.end(), Compare());
  return 0;
}
