// Copyright 2024 Yury Gribov
// 
// Use of this source code is governed by MIT license that can be
// found in the LICENSE.txt file.

#include <algorithm>
#include <vector>

struct Compare {
  bool operator()(int a, int b) {
    if (a == 100)
      return true;
    return a != b;
  }
};

int main() {
  std::vector<int> v;
  for (size_t i = 0; i < 32; ++i)
    v.push_back(0);
  v.push_back(100);
  std::sort(v.begin(), v.end(), Compare());
  return 0;
}
