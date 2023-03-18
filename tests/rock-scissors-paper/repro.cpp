// Copyright 2023 Yury Gribov
// 
// Use of this source code is governed by MIT license that can be
// found in the LICENSE.txt file.

#include <algorithm>
#include <vector>

enum class RSP {
  Rock,
  Scissors,
  Paper
};

struct Compare {
  bool operator()(RSP a, RSP b) {
    if (a == b)
      return false;
    if (a == RSP::Scissors && b == RSP::Rock)
      return false;
    if (a == RSP::Rock && b == RSP::Paper)
      return false;
    if (a == RSP::Paper && b == RSP::Scissors)
      return false;
    return ! operator()(b, a);
  }
};

int main() {
  std::vector<RSP> v;
  v.push_back(RSP::Rock);
  v.push_back(RSP::Scissors);
  v.push_back(RSP::Paper);
  std::stable_sort(v.begin(), v.end(), Compare());
  return 0;
}
