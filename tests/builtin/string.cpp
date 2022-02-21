// Copyright 2022 Yury Gribov
// 
// Use of this source code is governed by MIT license that can be
// found in the LICENSE.txt file.

#include <algorithm>
#include <vector>
#include <string>

int main() {
  std::vector<std::string> v;
  v.push_back("a");
  v.push_back("b");
  v.push_back("c");
  std::sort(v.begin(), v.end());
  return 0;
}
