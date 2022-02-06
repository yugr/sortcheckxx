#include <string>
#include <algorithm>

bool checkForImplicitGCCall(const char *name) {
  static const std::string names[] = {
    "A",
#ifdef INVALID
    "C",
    "B",
#else
    "B",
    "C",
#endif
  };
  return std::binary_search(&names[0], &names[sizeof(names) / sizeof(std::string)], name);
}

int main() {
  return checkForImplicitGCCall("B") ? 0 : 1;
}
