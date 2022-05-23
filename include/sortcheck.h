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
#include <vector>

#include <stdlib.h>

#include <unistd.h>
#include <fcntl.h>

// Alas, syslog.h defines very popular symbols like LOG_ERROR
// so we can't include it
extern "C" void syslog(int __pri, const char *__fmt, ...);

namespace sortcheck {

#if __cplusplus >= 201100L
# define SORTCHECK_NOEXCEPT(expr) noexcept(expr)
#else
# define SORTCHECK_NOEXCEPT(expr)
#endif

enum {
  SORTCHECK_LESS = -1,
  SORTCHECK_EQUAL = 0,
  SORTCHECK_GREATER = 1
};

struct Compare {
  template<typename A, typename B>
  bool operator()(const A &lhs, const B &rhs) const SORTCHECK_NOEXCEPT(noexcept(lhs < rhs)) {
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
    opts.syslog = slog ? atoi(slog) : 0;

    const char *abrt = getenv("SORTCHECK_ABORT");
    opts.abort = abrt ? atoi(abrt) : 1;

    const char *exit_code = getenv("SORTCHECK_EXIT_CODE");
    opts.exit_code = exit_code ? atoi(exit_code) : 1;

    if (const char *checks = getenv("SORTCHECK_CHECKS")) {
      const bool is_binary = checks && checks[0] == '0' && (checks[1] == 'b' || checks[1] == 'B');
      opts.checks = strtoul(checks, NULL, is_binary ? 2 : 0);
      if (!opts.checks) {
        std::cerr << "sortcheck: all checks disabled in SORTCHECK_CHECKS\n";
      }
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
#define LOG_ERR 3  // From syslog.h
  if (opts.syslog)
    syslog(LOG_ERR, "%s", msg.c_str());
#undef LOG_ERR

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
      cmp[i][j] = __comp(*(__first + i), *(__first + j)) ? SORTCHECK_GREATER : SORTCHECK_LESS;
    }
  }

  for (size_t i = 0; i < n; ++i) {
    for (size_t j = 0; j <= i; ++j) {
      if (cmp[i][j] == SORTCHECK_LESS && cmp[j][i] == SORTCHECK_LESS)
        cmp[i][j] = cmp[j][i] = SORTCHECK_EQUAL;
    }
  }

  if (opts.checks & SORTCHECK_CHECK_REFLEXIVITY) {
    for (size_t i = 0; i < size_t(__last - __first); ++i) {
      if (cmp[i][i] != SORTCHECK_EQUAL) {
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
  if (!(opts.checks & SORTCHECK_CHECK_SORTED) || __first == __last)
    return;

  unsigned pos = 0;
  for (_ForwardIterator cur = __first, prev = cur++;
       cur != __last;
       ++prev, ++cur, ++pos) {
    if (__comp(*cur, *prev)) {
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

  int prev = SORTCHECK_LESS;
  unsigned pos = 0;
  for (_ForwardIterator it = __first; it != __last; ++it, ++pos) {
    const int dir = __comp(*it, __val) ? SORTCHECK_LESS :
      __comp(__val, *it) ? SORTCHECK_GREATER :
      SORTCHECK_EQUAL;
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

  int prev = SORTCHECK_LESS;
  unsigned pos = 0;
  for (_ForwardIterator it = __first; it != __last; ++it, ++pos) {
    const int dir = __comp(*it, __val) ? SORTCHECK_LESS : SORTCHECK_GREATER;
    if (dir < prev) {
      std::ostringstream os;
      os << "sortcheck: " << file << ':' << line << ": unsorted range "
         << "at position " << pos;
      report_error(os.str(), opts);
    }
    prev = dir;
  }
}

// binary_search overloads

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
  return binary_search_checked(__first, __last, __val, Compare(), file, line);
}

template<typename _ForwardIterator, typename _Tp, typename _Compare>
inline bool binary_search_checked_full(_ForwardIterator __first,
                                       _ForwardIterator __last,
                                       const _Tp &__val, _Compare __comp,
                                       bool do_check_range,
                                       const char *file, int line) {
  if (do_check_range)
    check_range(__first, __last, __comp, file, line);
  check_sorted(__first, __last, __comp, file, line);
  return binary_search_checked(__first, __last, __val, __comp, file , line);
}

template<typename _ForwardIterator, typename _Tp>
inline bool binary_search_checked_full(_ForwardIterator __first,
                                       _ForwardIterator __last,
                                       const _Tp &__val,
                                       bool do_check_range,
                                       const char *file, int line) {
  return binary_search_checked_full(__first, __last, __val, Compare(), do_check_range, file, line);
}

// lower_bound overloads

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
  return lower_bound_checked(__first, __last, __val, Compare(), file, line);
}

template<typename _ForwardIterator, typename _Tp, typename _Compare>
inline _ForwardIterator lower_bound_checked_full(_ForwardIterator __first,
                                                 _ForwardIterator __last,
                                                 const _Tp &__val, _Compare __comp,
                                                 bool do_check_range,
                                                 const char *file, int line) {
  if (do_check_range)
    check_range(__first, __last, __comp, file, line);
  check_sorted(__first, __last, __comp, file, line);
  return lower_bound_checked(__first, __last, __val, __comp, file , line);
}

template<typename _ForwardIterator, typename _Tp>
inline _ForwardIterator lower_bound_checked_full(_ForwardIterator __first,
                                                 _ForwardIterator __last,
                                                 const _Tp &__val,
                                                 bool do_check_range,
                                                 const char *file, int line) {
  return lower_bound_checked_full(__first, __last, __val, Compare(), do_check_range, file, line);
}

// upper_bound overloads

template<typename _ForwardIterator, typename _Tp, typename _Compare>
inline _ForwardIterator upper_bound_checked(_ForwardIterator __first,
                                            _ForwardIterator __last,
                                            const _Tp &__val, _Compare __comp,
                                            const char *file, int line) {
  check_ordered_simple(__first, __last, __comp, __val, file, line);
  return std::upper_bound(__first, __last, __val, __comp);
}

template<typename _ForwardIterator, typename _Tp>
inline _ForwardIterator upper_bound_checked(_ForwardIterator __first,
                                            _ForwardIterator __last,
                                            const _Tp &__val,
                                            const char *file, int line) {
  return upper_bound_checked(__first, __last, __val, Compare(), file, line);
}

template<typename _ForwardIterator, typename _Tp, typename _Compare>
inline _ForwardIterator upper_bound_checked_full(_ForwardIterator __first,
                                                 _ForwardIterator __last,
                                                 const _Tp &__val, _Compare __comp,
                                                 bool do_check_range,
                                                 const char *file, int line) {
  if (do_check_range)
    check_range(__first, __last, __comp, file, line);
  check_sorted(__first, __last, __comp, file, line);
  return upper_bound_checked(__first, __last, __val, __comp, file , line);
}

template<typename _ForwardIterator, typename _Tp>
inline _ForwardIterator upper_bound_checked_full(_ForwardIterator __first,
                                                 _ForwardIterator __last,
                                                 const _Tp &__val,
                                                 bool do_check_range,
                                                 const char *file, int line) {
  return upper_bound_checked_full(__first, __last, __val, Compare(), do_check_range, file, line);
}

// equal_range overloads

template<typename _ForwardIterator, typename _Tp, typename _Compare>
inline std::pair<_ForwardIterator, _ForwardIterator> equal_range_checked(_ForwardIterator __first,
                                                                         _ForwardIterator __last,
                                                                         const _Tp &__val, _Compare __comp,
                                                                         const char *file, int line) {
  check_ordered_simple(__first, __last, __comp, __val, file, line);
  return std::equal_range(__first, __last, __val, __comp);
}

template<typename _ForwardIterator, typename _Tp>
inline std::pair<_ForwardIterator, _ForwardIterator> equal_range_checked(_ForwardIterator __first,
                                                                         _ForwardIterator __last,
                                                                         const _Tp &__val,
                                                                         const char *file, int line) {
  return equal_range_checked(__first, __last, __val, Compare(), file, line);
}

template<typename _ForwardIterator, typename _Tp, typename _Compare>
inline std::pair<_ForwardIterator, _ForwardIterator> equal_range_checked_full(_ForwardIterator __first,
                                                                              _ForwardIterator __last,
                                                                              const _Tp &__val, _Compare __comp,
                                                                              bool do_check_range,
                                                                              const char *file, int line) {
  if (do_check_range)
    check_range(__first, __last, __comp, file, line);
  check_sorted(__first, __last, __comp, file, line);
  return equal_range_checked(__first, __last, __val, __comp, file , line);
}

template<typename _ForwardIterator, typename _Tp>
inline std::pair<_ForwardIterator, _ForwardIterator> equal_range_checked_full(_ForwardIterator __first,
                                                                              _ForwardIterator __last,
                                                                              const _Tp &__val,
                                                                              bool do_check_range,
                                                                              const char *file, int line) {
  return equal_range_checked_full(__first, __last, __val, Compare(), do_check_range, file, line);
}

// sort overloads

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
  sort_checked(__first, __last, Compare(), file, line);
}

// stable_sort overloads

template<typename _RandomAccessIterator, typename _Compare>
inline void stable_sort_checked(_RandomAccessIterator __first,
                         _RandomAccessIterator __last,
                         _Compare __comp,
                         const char *file, int line) {
  check_range(__first, __last, __comp, file, line);
  std::stable_sort(__first, __last, __comp);
}

template<typename _RandomAccessIterator>
inline void stable_sort_checked(_RandomAccessIterator __first,
                         _RandomAccessIterator __last,
                         const char *file, int line) {
  stable_sort_checked(__first, __last, Compare(), file, line);
}

// max_element overloads

template<typename _RandomAccessIterator, typename _Compare>
inline _RandomAccessIterator max_element_checked(_RandomAccessIterator __first,
                                                 _RandomAccessIterator __last,
                                                 _Compare __comp,
                                                 const char *file, int line) {
  check_range(__first, __last, __comp, file, line);
  return std::max_element(__first, __last, __comp);
}

template<typename _RandomAccessIterator>
inline _RandomAccessIterator max_element_checked(_RandomAccessIterator __first,
                                                 _RandomAccessIterator __last,
                                                 const char *file, int line) {
  return max_element_checked(__first, __last, Compare(), file, line);
}

// min_element overloads

template<typename _RandomAccessIterator, typename _Compare>
inline _RandomAccessIterator min_element_checked(_RandomAccessIterator __first,
                                                 _RandomAccessIterator __last,
                                                 _Compare __comp,
                                                 const char *file, int line) {
  check_range(__first, __last, __comp, file, line);
  return std::min_element(__first, __last, __comp);
}

template<typename _RandomAccessIterator>
inline _RandomAccessIterator min_element_checked(_RandomAccessIterator __first,
                                                 _RandomAccessIterator __last,
                                                 const char *file, int line) {
  return min_element_checked(__first, __last, Compare(), file, line);
}

// std::map/set checks

template<typename Map>
Map &check_map(Map &m, const char *file, int line) {
  std::vector<typename Map::key_type> keys;
  for (typename Map::iterator i = m.begin(), end = m.end(); i != end; ++i)
    keys.push_back(i->first);
  check_range(keys.begin(), keys.end(), m.key_comp(), file, line);
  return m;
}

template<typename Map>
Map *check_associative_map(Map *m, const char *file, int line) {
  return &check_map(*m, file, line);
}

template<typename Set>
Set &check_set(Set &m, const char *file, int line) {
  std::vector<typename Set::key_type> keys(m.begin(), m.end());
  check_range(keys.begin(), keys.end(), m.key_comp(), file, line);
  return m;
}

template<typename Set>
Set *check_set(Set *m, const char *file, int line) {
  return &check_set(*m, file, line);
}

} // namespace sortcheck

#endif
