// Copyright 2022 Yury Gribov
// 
// Use of this source code is governed by MIT license that can be
// found in the LICENSE.txt file.

#include <set>

struct Compare {
  bool operator()(int lhs, int rhs) const {
    return lhs == rhs;
  }
};

int main() {
  std::set<int, Compare> *v[2] = {new std::set<int, Compare>(), 0};
  v[0]->insert(1);
  v[0]->insert(3);
  v[0]->insert(2);
  v[0]->clear();
  return 0;
}
