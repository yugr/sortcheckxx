[![License](http://img.shields.io/:license-MIT-blue.svg)](https://github.com/yugr/sortcheckxx/blob/master/LICENSE.txt)
[![Build Status](https://github.com/yugr/sortcheckxx/actions/workflows/ci.yml/badge.svg)](https://github.com/yugr/sortcheckxx/actions)
[![codecov](https://codecov.io/gh/yugr/sortcheckxx/branch/master/graph/badge.svg)](https://codecov.io/gh/yugr/sortcheckxx)
[![Total alerts](https://img.shields.io/lgtm/alerts/g/yugr/sortcheckxx.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/yugr/sortcheckxx/alerts/)
[![Coverity Scan](https://scan.coverity.com/projects/yugr-sortcheckxx/badge.svg)](https://scan.coverity.com/projects/yugr-sortcheckxx)

SortChecker++ verifies that comparators in C++ APIs like `std::sort` or `std::binary_search`
satisfy the [Strict Weak Ordering](https://medium.com/@shiansu/strict-weak-ordering-and-the-c-stl-f7dcfa4d4e07)
axioms.
Violation of these axioms is undefined behavior and may lead to all sorts of runtime
errors including [aborts](https://stackoverflow.com/questions/2441045/bewildering-segfault-involving-stl-sort-algorithm)
(also [here](https://stackoverflow.com/questions/46670734/erratic-behavior-of-gccs-stdsort-with-lambdas),
see [this answer](https://stackoverflow.com/a/24048654/2170527),
[Qualys analysis](https://www.openwall.com/lists/oss-security/2024/01/30/7) and
[my slides](https://github.com/yugr/CppRussia/blob/master/2023/EN.pdf) for explanations).

SortChecker++ is an extension of [SortChecker](https://github.com/yugr/sortcheck) tool which does similar job to C sorting APIs.

The tool has been tested on LLVM 6.0 (Ubuntu 18.04), 10.0 (Ubuntu 20.04) and 14.0 (Ubuntu 22.04).

List of found issues:
* [Libosmium: Missing prepare\_for\_lookup in test\_members\_database.cpp](https://github.com/osmcode/libosmium/issues/351) (fixed)
* [ZeroC Ice: Unsorted array in lookupKwd](https://github.com/zeroc-ice/ice/issues/1362) (fixed)
* [GiNaC: Potential issue in comparator in test\_antipode](https://www.ginac.de/pipermail/ginac-list/2022-June/002390.html) (fixed)
* ArrayFire: Irreflexive comparator (already [fixed](https://github.com/arrayfire/arrayfire/commit/77181f1d9c860144554cd61e4de69b9dd82ccad9) in latest)
* Giac: Non-asymmetric comparator in polynome\_less (already [fixed](https://github.com/geogebra/giac/commit/efbde32d614aed9833903f93084f76bbf61cf418) in latest)

Note that some (but not all!) errors found by Sortchecker++ can also be found via [`-D_LIBCPP_DEBUG_STRICT_WEAK_ORDERING_CHECK`](https://reviews.llvm.org/D150264).

# How to build

To use, first install dependencies:
```
$ sudo apt install libclang-dev llvm-dev
```
and then build the tool
```
$ make clean all
```

# How to use

SortChecker works by instrumenting the input file, i.e. inserting additional checking code into it.
You can run it manually via
```
$ SortChecker file.cpp -- $CXXFLAGS
```
then compile via
```
$ g++ file.cpp $CXXFLAGS -Ipath/to/sortcheck.h
```

Finally run the resulting executable and it will report any errors e.g.
```
$ ./a.out
sortcheck: file.cpp:23: non-asymmetric comparator at positions 1 and 0
```
(you'll probably want to combine this with some kind of regression
or random/fuzz testing to achieve good coverage,
also see the `SORTCHECK_SHUFFLE` option below).

You could also use compiler wrappers in `scripts/` folder to combine instrumentation and compilation:
```
$ PATH=path/to/scripts:$PATH make clean all
```

Instrumented program may be controlled with environment variables:
* `SORTCHECK_VERBOSE=N` - verbosity (`N` is an integer)
* `SORTCHECK_SYSLOG=1` - dump messages to syslog (in addition to stderr)
* `SORTCHECK_ABORT_ON_ERROR=1` - call `abort()` on detected error
* `SORTCHECK_EXIT_CODE=N` - call `exit(CODE)` on detected error (`N` is an integer)
* `SORTCHECK_OUTPUT=path/to/logfile` - write detected errors to file instead of stdout
* `SORTCHECK_CHECKS=mask` - set which checks are enabled via bitmask
  (e.g. `mask=0xfffe` would disable the generally uninteresting irreflexivity checks)
* `SORTCHECK_SHUFFLE=val` - reshuffle containers before checking with given seed;
  a value of `rand` will use random seed
  (helps to find bugs which are not located at start of array)

# Interpreting the error messages

tbd
