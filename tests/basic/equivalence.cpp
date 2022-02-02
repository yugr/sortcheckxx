// Copyright 2022 Yury Gribov
// 
// Use of this source code is governed by MIT license that can be
// found in the LICENSE.txt file.

#include <algorithm>
#include <vector>

struct BadCompare {
  // Example from https://en.wikipedia.org/wiki/Weak_ordering#Strict_weak_orderings
  bool operator()(int lhs, int rhs) {
    if (lhs == 2 && rhs == 3)
      return true;
    return false;
  }
};

int main() {
  std::vector<int> v;
  v.push_back(1);
  v.push_back(2);
  v.push_back(3);
  std::sort(v.begin(), v.end(), BadCompare());
  return 0;
}
