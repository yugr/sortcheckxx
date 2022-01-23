#include <sortcheck.h>
// Copyright 2022 Yury Gribov
// 
// Use of this source code is governed by MIT license that can be
// found in the LICENSE.txt file.

#include <algorithm>
#include <vector>

struct BadCompare {
  bool operator()(int lhs, int rhs) {
    if (lhs == rhs)
      return false;
    if (lhs > rhs)
      return ! operator()(rhs, lhs);
    if (lhs == 1 && rhs == 2)
      return true;
    if (lhs == 2 && rhs == 3)
      return true;
    if (lhs == 1 && rhs == 3)
      return false;
    return false;
  }
};

int main() {
  std::vector<int> v;
  v.push_back(3);
  v.push_back(2);
  v.push_back(1);
  sortcheck::sort_checked(v.begin(), v.end(), BadCompare(), __FILE__, __LINE__);
  return 0;
}
