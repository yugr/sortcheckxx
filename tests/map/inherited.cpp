#include <map>

class Key {
  public:
    bool operator <(const Key &rhs) const;
};

class A : public std::map<Key, int> {
  void foo() {
    clear();
  }
};

int main() { return 0; }
