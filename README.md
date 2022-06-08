[![License](http://img.shields.io/:license-MIT-blue.svg)](https://github.com/yugr/sortcheckxx/blob/master/LICENSE.txt)
[![Build Status](https://github.com/yugr/sortcheckxx/actions/workflows/ci.yml/badge.svg)](https://github.com/yugr/sortcheckxx/actions)
[![codecov](https://codecov.io/gh/yugr/sortcheckxx/branch/master/graph/badge.svg)](https://codecov.io/gh/yugr/sortcheckxx)
[![Total alerts](https://img.shields.io/lgtm/alerts/g/yugr/sortcheckxx.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/yugr/sortcheckxx/alerts/)
[![Coverity Scan](https://scan.coverity.com/projects/yugr-sortcheckxx/badge.svg)](https://scan.coverity.com/projects/yugr-sortcheckxx)

SortChecker++ verifies that comparators in C++ APIs like `std::sort` or `std::binary_search`
satisfy the [Strict Weak Ordering](https://medium.com/@shiansu/strict-weak-ordering-and-the-c-stl-f7dcfa4d4e07)
axioms.
Violation of these axioms is undefined behavior and may lead to all sorts of runtime
errors including [aborts](https://stackoverflow.com/questions/2441045/bewildering-segfault-involving-stl-sort-algorithm) (also [here](https://stackoverflow.com/questions/46670734/erratic-behavior-of-gccs-stdsort-with-lambdas), see [this answer](https://stackoverflow.com/a/24048654/2170527) for explanations).

SortChecker++ is an extension of [SortChecker](https://github.com/yugr/sortcheck) tool which does similar job to C sorting APIs.

The tool has been tested on LLVM 6.0 (Ubuntu 18.04) and 10.0 (Ubuntu 20.04).

List of found issues:
* [Libosmium: Missing prepare\_for\_lookup in test\_members\_database.cpp](https://github.com/osmcode/libosmium/issues/351) (fixed)
* [ZeroC Ice: Unsorted array in lookupKwd](https://github.com/zeroc-ice/ice/issues/1362)
* [GiNaC: Potential issue in comparator in test\_antipode](https://www.ginac.de/pipermail/ginac-list/2022-June/002390.html) (fixed)

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
and then compile via
```
$ g++ file.cpp $CXXFLAGS -Ipath/to/sortcheck.h
```

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

# Interpreting the error messages

tbd
