#include <sortcheck.h>
// Copyright 2022 Yury Gribov
// 
// Use of this source code is governed by MIT license that can be
// found in the LICENSE.txt file.

#include <algorithm>
#include <vector>

struct BadCompare {
  bool operator()(int, int) {
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
