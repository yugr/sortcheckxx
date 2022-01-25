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

// TODO: can I have generic impl ? Check STL code.
#if __cplusplus >= 201100L || ! defined __STRICT_ANSI__
#define SORTCHECK_SUPPORT_COMPARELESS_API 1

#if __cplusplus >= 201100L
#define SORTCHECK_DECLTYPE(val) decltype((val))
#else
#define SORTCHECK_DECLTYPE(val) typeof((val))
#endif
#endif

// TODO: replace with std:less ?
template<typename T>
struct Compare {
  bool operator()(const T &lhs, const T &rhs) const {
    return lhs < rhs;
  }
};

struct Options {
  bool abort_on_error;
  int verbose;
};

inline const Options &get_options() {
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

template<typename _RandomAccessIterator, typename _Compare>
inline void check_range(_RandomAccessIterator __first,
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
    for (size_t j = 0; j < i; ++j) {
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
}

template<typename _ForwardIterator, typename _Compare>
inline void check_sorted(_ForwardIterator __first,
                         _ForwardIterator __last,
                         _Compare __comp,
                         const char *file, int line) {
  const Options &opts = get_options();

  unsigned pos = 0;
  for (_ForwardIterator cur = __first, prev = cur++;
       cur != __last;
       ++prev, ++cur, ++pos) {
    if (!__comp(*prev, *cur)) {
      std::cerr << "sortcheck: " << file << ':' << line << ": "
                << "unsorted range at position " << pos << '\n';
      if (opts.abort_on_error)
        abort();
    }
  }
}

template<typename _ForwardIterator, typename _Tp, typename _Compare>
inline bool binary_search_checked(_ForwardIterator __first,
                                  _ForwardIterator __last,
                                  const _Tp &__val, _Compare __comp,
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

  return std::binary_search(__first, __last, __val, __comp);
}

template<typename _ForwardIterator, typename _Tp>
inline bool binary_search_checked(_ForwardIterator __first,
                                  _ForwardIterator __last,
                                  const _Tp &__val,
                                  const char *file, int line) {
  Compare<_Tp> compare;
  return binary_search_checked(__first, __last, __val, compare, file, line);
}

template<typename _ForwardIterator, typename _Tp, typename _Compare>
inline bool binary_search_checked_full(_ForwardIterator __first,
                                       _ForwardIterator __last,
                                       const _Tp &__val, _Compare __comp,
                                       const char *file, int line) {
  check_range(__first, __last, __comp, file, line);
  check_sorted(__first, __last, __comp, file, line);
  return binary_search_checked(__first, __last, __val, __comp, file ,line);
}

template<typename _ForwardIterator, typename _Tp>
inline bool binary_search_checked_full(_ForwardIterator __first,
                                       _ForwardIterator __last,
                                       const _Tp &__val,
                                       const char *file, int line) {
  Compare<_Tp> compare;
  return binary_search_checked_full(__first, __last, __val, compare, file, line);
}

template<typename _RandomAccessIterator, typename _Compare>
inline void sort_checked(_RandomAccessIterator __first,
                         _RandomAccessIterator __last,
                         _Compare __comp,
                         const char *file, int line) {
  check_range(__first, __last, __comp, file, line);
  std::sort(__first, __last, __comp);
}

// TODO: can I have generic impl ? Check STL code.
#ifdef SORTCHECK_SUPPORT_COMPARELESS_API
template<typename _RandomAccessIterator>
inline void sort_checked(_RandomAccessIterator __first,
                         _RandomAccessIterator __last,
                         const char *file, int line) {
  Compare<SORTCHECK_DECLTYPE(*__first)> compare;
  sort_checked(__first, __last, compare, file, line);
}
#endif  // SORTCHECK_SUPPORT_COMPARELESS_API

} // anon namespace

#endif
