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

#include <unistd.h>
#include <fcntl.h>

namespace sortcheck {

// TODO: can I have generic impl without code dup?
#if __cplusplus >= 201100L || ! defined __STRICT_ANSI__
# define SORTCHECK_SUPPORT_COMPARELESS_API 1
# if __cplusplus >= 201100L
#   define SORTCHECK_DECLTYPE(val) decltype((val))
# else
#   define SORTCHECK_DECLTYPE(val) typeof((val))
# endif
#endif

#define SORTCHECK_UNUSED(x) x = x

#if __cplusplus >= 201100L
# define SORTCHECK_NOEXCEPT(expr) noexcept(expr)
#else
# define SORTCHECK_NOEXCEPT(expr)
#endif

enum {
  LESS = -1,
  EQ = 0,
  GREATER = 1
};

// TODO: replace with std:less ?
template<typename A>
struct SingleTypedCompare {
  bool operator()(A lhs, A rhs) const SORTCHECK_NOEXCEPT(noexcept(lhs < rhs)) {
    return lhs < rhs;
  }
};

// TODO: replace with std:less ?
template<typename A, typename B>
struct DoubleTypedCompare {
  bool operator()(A lhs, B rhs) const SORTCHECK_NOEXCEPT(noexcept(lhs < rhs)) {
    return lhs < rhs;
  }
  bool operator()(B lhs, A rhs) const SORTCHECK_NOEXCEPT(noexcept(lhs < rhs)) {
    return lhs < rhs;
  }
};

struct Options {
  bool abort;
  int verbose;
  bool syslog;
  int exit_code;
  int out;
  unsigned long checks;
};

// SORTCHECK_CHECKS bits
#define SORTCHECK_CHECK_REFLEXIVITY (1 << 0)
#define SORTCHECK_CHECK_SYMMETRY (1 << 1)
#define SORTCHECK_CHECK_TRANSITIVITY (1 << 2)
#define SORTCHECK_CHECK_SORTED (1 << 3)
#define SORTCHECK_CHECK_ORDERED (1 << 4)

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

    if (const char *checks = getenv("SORTCHECK_CHECKS")) {
      const bool is_binary = checks && checks[0] == '0' && (checks[1] == 'b' || checks[1] == 'B');
      opts.checks = strtoul(checks, NULL, is_binary ? 2 : 0);
    } else {
      opts.checks = ~0ul;
    }

    if (const char *out = getenv("SORTCHECK_OUTPUT")) {
      opts.out = open(out, O_WRONLY | O_CREAT | O_APPEND, 0777);
      if (opts.out < 0) {
        std::cerr << "sortcheck: failed to open " << out << '\n';
        abort();
      }
    } else {
      opts.out = STDOUT_FILENO;
    }

    opts_initialized = true;
  }
  return opts;
}

inline void report_error(const std::string &msg, const Options &opts) {
  if (opts.syslog)
    syslog(LOG_ERR, "%s", msg.c_str());

  char c = '\n';
  if (write(opts.out, msg.c_str(), msg.size()) >= 0
      && write(opts.out, &c, 1) >= 0) {
    fsync(opts.out);
  } else {
    std::cerr << "sortcheck: failed to write to " << opts.out << '\n';
    abort();
  }

  if (opts.abort) {
    close(opts.out);
    abort();
  }

  if (opts.exit_code)
    exit(opts.exit_code);
}

template<typename _RandomAccessIterator, typename _Compare>
inline void check_range(_RandomAccessIterator __first,
                        _RandomAccessIterator __last,
                        _Compare __comp,
                        const char *file, int line) {
  const Options &opts = get_options();

  signed char cmp[32u][32u];
  const size_t n = std::min(size_t(__last - __first), sizeof(cmp) / sizeof(cmp[0]));
  for (size_t i = 0; i < n; ++i) {
    for (size_t j = 0; j < n; ++j) {
      cmp[i][j] = __comp(*(__first + i), *(__first + j)) ? GREATER : LESS;
    }
  }

  for (size_t i = 0; i < n; ++i) {
    for (size_t j = 0; j <= i; ++j) {
      if (cmp[i][j] == LESS && cmp[j][i] == LESS)
        cmp[i][j] = cmp[j][i] = EQ;
    }
  }

  if (opts.checks & SORTCHECK_CHECK_REFLEXIVITY) {
    for (size_t i = 0; i < size_t(__last - __first); ++i) {
      if (cmp[i][i] != EQ) {
        std::ostringstream os;
        os << "sortcheck: " << file << ':' << line << ": "
           << "irreflexive comparator at position " << i;
        report_error(os.str(), opts);
      }
    }
  }

  if (opts.checks & SORTCHECK_CHECK_SYMMETRY) {
    for (size_t i = 0; i < n; ++i) {
      for (size_t j = 0; j < i; ++j) {
        if (cmp[i][j] != -cmp[j][i]) {
          std::ostringstream os;
          os << "sortcheck: " << file << ':' << line << ": "
             << "non-asymmetric comparator at positions "
             << i << " and " << j;
          report_error(os.str(), opts);
        }
      }
    }
  }

  if (opts.checks & SORTCHECK_CHECK_TRANSITIVITY) {
    for (size_t i = 0; i < n; ++i) {
      for (size_t j = 0; j < i; ++j) {
        for (size_t k = 0; k < n; ++k) {
          if (cmp[i][j] == cmp[j][k] && cmp[i][k] != cmp[i][j]) {
            std::ostringstream os;
            os << "sortcheck: " << file << ':' << line << ": "
               << "non-transitive " << (cmp[i][j] ? "" : "equivalent ")
               << "comparator at positions "
               << i << ", " << j << " and " << k;
            report_error(os.str(), opts);
          }
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
  if (!(opts.checks & SORTCHECK_CHECK_SORTED))
    return;

  unsigned pos = 0;
  for (_ForwardIterator cur = __first, prev = cur++;
       cur != __last;
       ++prev, ++cur, ++pos) {
    if (!__comp(*prev, *cur)) {
      std::ostringstream os;
      os << "sortcheck: " << file << ':' << line << ": "
         << "unsorted range at position " << pos;
      report_error(os.str(), opts);
    }
  }
}

template<typename _ForwardIterator, typename _Tp, typename _Compare>
inline void check_ordered(_ForwardIterator __first,
                          _ForwardIterator __last,
                          _Compare __comp,
                          const _Tp &__val,
                          const char *file, int line) {
  const Options &opts = get_options();
  if (!(opts.checks & SORTCHECK_CHECK_ORDERED) || __first == __last)
    return;

  int prev = LESS;
  unsigned pos = 0;
  for (_ForwardIterator it = __first; it != __last; ++it, ++pos) {
    const int dir = __comp(*it, __val) ? LESS :
      __comp(__val, *it) ? GREATER :
      EQ;
    if (dir < prev) {
      std::ostringstream os;
      os << "sortcheck: " << file << ':' << line << ": unsorted range "
         << "at position " << pos;
      report_error(os.str(), opts);
    }
    prev = dir;
  }
}

// A simpler version of check_ordered when presense of __comp(__val, *iter)
// is not guaranteed (e.g. in std::lower_bound).
template<typename _ForwardIterator, typename _Tp, typename _Compare>
inline void check_ordered_simple(_ForwardIterator __first,
                          _ForwardIterator __last,
                          _Compare __comp,
                          const _Tp &__val,
                          const char *file, int line) {
  const Options &opts = get_options();
  if (!(opts.checks & SORTCHECK_CHECK_ORDERED) || __first == __last)
    return;

  int prev = LESS;
  unsigned pos = 0;
  for (_ForwardIterator it = __first; it != __last; ++it, ++pos) {
    const int dir = __comp(*it, __val) ? LESS : GREATER;
    if (dir < prev) {
      std::ostringstream os;
      os << "sortcheck: " << file << ':' << line << ": unsorted range "
         << "at position " << pos;
      report_error(os.str(), opts);
    }
    prev = dir;
  }
}

template<typename _ForwardIterator, typename _Tp, typename _Compare>
inline bool binary_search_checked(_ForwardIterator __first,
                                  _ForwardIterator __last,
                                  const _Tp &__val, _Compare __comp,
                                  const char *file, int line) {
  check_ordered(__first, __last, __comp, __val, file, line);
  return std::binary_search(__first, __last, __val, __comp);
}

template<typename _ForwardIterator, typename _Tp>
inline bool binary_search_checked(_ForwardIterator __first,
                                  _ForwardIterator __last,
                                  const _Tp &__val,
                                  const char *file, int line) {
#ifdef SORTCHECK_SUPPORT_COMPARELESS_API
  DoubleTypedCompare<SORTCHECK_DECLTYPE(*__first), _Tp> compare;
  return binary_search_checked(__first, __last, __val, compare, file, line);
#else
  SORTCHECK_UNUSED(file);
  SORTCHECK_UNUSED(line);
  return std::binary_search(__first, __last, __val);
#endif
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
  SingleTypedCompare<_Tp> compare;
  return binary_search_checked_full(__first, __last, __val, compare, file, line);
}

template<typename _ForwardIterator, typename _Tp, typename _Compare>
inline _ForwardIterator lower_bound_checked(_ForwardIterator __first,
                                            _ForwardIterator __last,
                                            const _Tp &__val, _Compare __comp,
                                            const char *file, int line) {
  check_ordered_simple(__first, __last, __comp, __val, file, line);
  return std::lower_bound(__first, __last, __val, __comp);
}

template<typename _ForwardIterator, typename _Tp>
inline _ForwardIterator lower_bound_checked(_ForwardIterator __first,
                                            _ForwardIterator __last,
                                            const _Tp &__val,
                                            const char *file, int line) {
#ifdef SORTCHECK_SUPPORT_COMPARELESS_API
  DoubleTypedCompare<SORTCHECK_DECLTYPE(*__first), _Tp> compare;
  return lower_bound_checked(__first, __last, __val, compare, file, line);
#else
  SORTCHECK_UNUSED(file);
  SORTCHECK_UNUSED(line);
  return std::lower_bound(__first, __last, __val);
#endif
}

template<typename _ForwardIterator, typename _Tp, typename _Compare>
inline _ForwardIterator lower_bound_checked_full(_ForwardIterator __first,
                                                 _ForwardIterator __last,
                                                 const _Tp &__val, _Compare __comp,
                                                 const char *file, int line) {
  check_range(__first, __last, __comp, file, line);
  check_sorted(__first, __last, __comp, file, line);
  return lower_bound_checked(__first, __last, __val, __comp, file ,line);
}

template<typename _ForwardIterator, typename _Tp>
inline _ForwardIterator lower_bound_checked_full(_ForwardIterator __first,
                                                 _ForwardIterator __last,
                                                 const _Tp &__val,
                                                 const char *file, int line) {
  SingleTypedCompare<_Tp> compare;
  return lower_bound_checked_full(__first, __last, __val, compare, file, line);
}

template<typename _RandomAccessIterator, typename _Compare>
inline void sort_checked(_RandomAccessIterator __first,
                         _RandomAccessIterator __last,
                         _Compare __comp,
                         const char *file, int line) {
  check_range(__first, __last, __comp, file, line);
  std::sort(__first, __last, __comp);
}

template<typename _RandomAccessIterator>
inline void sort_checked(_RandomAccessIterator __first,
                         _RandomAccessIterator __last,
                         const char *file, int line) {
#ifdef SORTCHECK_SUPPORT_COMPARELESS_API
  SingleTypedCompare<SORTCHECK_DECLTYPE(*__first)> compare;
  sort_checked(__first, __last, compare, file, line);
#else
  SORTCHECK_UNUSED(file);
  SORTCHECK_UNUSED(line);
  std::sort(__first, __last);
#endif  // SORTCHECK_SUPPORT_COMPARELESS_API
}

} // anon namespace

#endif
