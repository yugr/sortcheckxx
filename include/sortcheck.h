// Copyright 2022 Yury Gribov
// 
// Use of this source code is governed by MIT license that can be
// found in the LICENSE.txt file.

#ifndef SORTCHECK_H
#define SORTCHECK_H

#include <algorithm>
#include <functional>
#include <iostream>

#include <stdlib.h>

namespace sortcheck {

struct Options {
  bool abort_on_error;
  int verbose;
};

const Options &get_options() {
  static Options opts;
  static bool opts_initialized;
  if (!opts_initialized) {
    // TODO: parse env variables
    // TODO: optionally print to syslog
    opts.abort_on_error = true;
    opts.verbose = 0;
    opts_initialized = true;
  }
  return opts;
}

template<typename _ForwardIterator, typename _Tp, typename _Compare>
inline bool binary_search_checked(_ForwardIterator __first,
                                  _ForwardIterator __last,
                                  const _Tp& __val, _Compare __comp,
                                  const char *file, int line) {
  const Options &opts = get_options();

  // Ordered
  if (__first != __last) {
    unsigned i = 0, num_changes = 0;
    _ForwardIterator prev = __first++;
    for (_ForwardIterator it = __first; it != __last; prev = it++, ++i) {
      num_changes += (*prev < __val) != (*it < __val);
      if (num_changes > 1) {
        std::cerr << file << ':' << line << ": non-monotonous range "
                  << "in position " << i << '\n';
        if (opts.abort_on_error)
          abort();
      }
    }
  }

  // TODO: enable additional checks if typeof(*__first) == _Tp

  return std::binary_search(__first, __last, __val, __comp);
}

template<typename _RandomAccessIterator, typename _Compare>
inline void sort_checked(_RandomAccessIterator __first,
                         _RandomAccessIterator __last,
                         _Compare __comp,
                         const char *file, int line) {
  const Options &opts = get_options();

  bool cmp[32u][32u];
  const size_t n = std::min(size_t(__last - __first), sizeof(cmp) / sizeof(cmp[0]));
  for (size_t i = 0; i < n; ++i) {
    for (size_t j = 0; j < n; ++j) {
      cmp[i][j] = __comp(*(__first + i), *(__first + j));
    }
  }

  // Anti-reflexivity
  for (size_t i = 0; i < size_t(__last - __first); ++i) {
    if (cmp[i][i]) {
      std::cerr << "sortcheck: " << file << ':' << line << ": "
                << "non-reflexive comparator at position " << i << '\n';
      if (opts.abort_on_error)
        abort();
    }
  }

  // Anti-symmetry
  for (size_t i = 0; i < n; ++i) {
    for (size_t j= 0; j < i; ++j) {
      if (cmp[i][j] != !cmp[j][i]) {
        std::cerr << "sortcheck: " << file << ':' << line << ": "
                  << "non-symmetric comparator at positions "
                  << i << " and " << j << '\n';
        if (opts.abort_on_error)
          abort();
      }
    }
  }

  // Transitivity
  for (size_t i = 0; i < n; ++i) {
    for (size_t j = 0; j < i; ++j) {
      for (size_t k = 0; k < n; ++k) {
        if (cmp[i][j] && cmp[j][k] && !cmp[i][k]) {
          std::cerr << "sortcheck: " << file << ':' << line << ": "
                    << "non-transitive comparator at positions "
                    << i << ", " << j << " and " << k << '\n';
          if (opts.abort_on_error)
            abort();
        }
      }
    }
  }

  std::sort(__first, __last, __comp);
}

// TODO: can I have generic impl ? Check STL code.
#if __cplusplus >= 201100L || ! defined __STRICT_ANSI__
template<typename _RandomAccessIterator>
inline void sort_checked(_RandomAccessIterator __first,
                         _RandomAccessIterator __last,
                         const char *file, int line) {
  struct Compare {
#if __cplusplus >= 201100L
    bool operator()(const decltype(*__first) &lhs, const decltype(*__first) &rhs) const {
#else
    bool operator()(const typeof(*__first) &lhs, const typeof(*__first) &rhs) const {
#endif
      return lhs < rhs;
    }
  };
  sort_checked(__first, __last, Compare(), file, line);
}
#endif

} // anon namespace

#endif
