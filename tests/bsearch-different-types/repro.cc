#include <string>
#include <algorithm>

bool checkForImplicitGCCall(const char *name) {
  static const std::string names[] = {
    "A",
    "B",
    "C",
  };
  return binary_search(&names[0], &names[sizeof(names) / sizeof(std::string)], name);
}

