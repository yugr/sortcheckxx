// Copyright 2022 Yury Gribov
// 
// Use of this source code is governed by MIT license that can be
// found in the LICENSE.txt file.

#ifndef SORTCHECK_H
#define SORTCHECK_H

#include <algorithm>
#include <functional>
#include <iostream>
#include <sstream>

#include <stdlib.h>
#include <syslog.h>

namespace sortcheck {

// TODO: can I have generic impl ? Check STL code.
#if __cplusplus >= 201100L || ! defined __STRICT_ANSI__
# define SORTCHECK_SUPPORT_COMPARELESS_API 1
# if __cplusplus >= 201100L
#   define SORTCHECK_DECLTYPE(val) decltype((val))
# else
#   define SORTCHECK_DECLTYPE(val) typeof((val))
# endif
#endif

// TODO: replace with std:less ?
template<typename T>
struct Compare {
  bool operator()(const T &lhs, const T &rhs) const {
    return lhs < rhs;
  }
};

struct Options {
  bool abort;
  int verbose;
  bool syslog;
  int exit_code;
};

inline const Options &get_options() {
  static Options opts;
  static bool opts_initialized;
  if (!opts_initialized) {
    const char *verbose = getenv("SORTCHECK_VERBOSE");
    opts.verbose = verbose ? atoi(verbose) : 0;

    const char *slog = getenv("SORTCHECK_SYSLOG");
    if ((opts.syslog = slog ? atoi(slog) : 0))
      openlog(0, 0, LOG_USER);

    const char *abrt = getenv("SORTCHECK_ABORT");
    opts.abort = abrt ? atoi(abrt) : 1;

    const char *exit_code = getenv("SORTCHECK_EXIT_CODE");
    opts.exit_code = exit_code ? atoi(exit_code) : 1;

    opts_initialized = true;
  }
  return opts;
}

void report_error(const std::string &msg) {
  const Options &opts = get_options();

  if (opts.syslog)
    syslog(LOG_ERR, "%s", msg.c_str());
  std::cerr << msg << '\n';

  if (opts.abort)
    abort();

  if (opts.exit_code)
    exit(opts.exit_code);
}

template<typename _RandomAccessIterator, typename _Compare>
inline void check_range(_RandomAccessIterator __first,
                        _RandomAccessIterator __last,
                        _Compare __comp,
                        const char *file, int line) {
  bool cmp[32u][32u];
  const size_t n = std::min(size_t(__last - __first), sizeof(cmp) / sizeof(cmp[0]));
  for (size_t i = 0; i < n; ++i) {
    for (size_t j = 0; j < n; ++j) {
      cmp[i][j] = __comp(*(__first + i), *(__first + j));
    }
  }

  // Irreflexivity
  for (size_t i = 0; i < size_t(__last - __first); ++i) {
    if (cmp[i][i]) {
      std::ostringstream os;
      os << "sortcheck: " << file << ':' << line << ": "
         << "irreflexive comparator at position " << i;
      report_error(os.str());
    }
  }

  // Anti-symmetry
  for (size_t i = 0; i < n; ++i) {
    for (size_t j = 0; j < i; ++j) {
      if (cmp[i][j] != !cmp[j][i]) {
        std::ostringstream os;
        os << "sortcheck: " << file << ':' << line << ": "
           << "non-symmetric comparator at positions "
           << i << " and " << j;
        report_error(os.str());
      }
    }
  }

  // Transitivity
  for (size_t i = 0; i < n; ++i) {
    for (size_t j = 0; j < i; ++j) {
      for (size_t k = 0; k < n; ++k) {
        if (cmp[i][j] && cmp[j][k] && !cmp[i][k]) {
          std::ostringstream os;
          os << "sortcheck: " << file << ':' << line << ": "
             << "non-transitive comparator at positions "
             << i << ", " << j << " and " << k;
          report_error(os.str());
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
  unsigned pos = 0;
  for (_ForwardIterator cur = __first, prev = cur++;
       cur != __last;
       ++prev, ++cur, ++pos) {
    if (!__comp(*prev, *cur)) {
      std::ostringstream os;
      os << "sortcheck: " << file << ':' << line << ": "
         << "unsorted range at position " << pos;
      report_error(os.str());
    }
  }
}

template<typename _ForwardIterator, typename _Tp, typename _Compare>
inline bool binary_search_checked(_ForwardIterator __first,
                                  _ForwardIterator __last,
                                  const _Tp &__val, _Compare __comp,
                                  const char *file, int line) {
  // Ordered
  if (__first != __last) {
    bool is_prev_less = true;
    unsigned pos = 0;
    for (_ForwardIterator it = __first; it != __last; ++it, ++pos) {
      bool is_less = __comp(*it, __val);
      if (is_less && !is_prev_less) {
        std::ostringstream os;
        os << file << ':' << line << ": unsorted range "
           << "in position " << pos;
        report_error(os.str());
      }
      is_prev_less = is_less;
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
