[![License](http://img.shields.io/:license-MIT-blue.svg)](https://github.com/yugr/sortcheckxx/blob/master/LICENSE.txt)
[![Build Status](https://github.com/yugr/sortcheckxx/actions/workflows/ci.yml/badge.svg)](https://github.com/yugr/sortcheckxx/actions)
[![codecov](https://codecov.io/gh/yugr/sortcheckxx/branch/master/graph/badge.svg)](https://codecov.io/gh/yugr/sortcheckxx)
[![Total alerts](https://img.shields.io/lgtm/alerts/g/yugr/sortcheckxx.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/yugr/sortcheckxx/alerts/)
[![Coverity Scan](https://scan.coverity.com/projects/yugr-sortcheckxx/badge.svg)](https://scan.coverity.com/projects/yugr-sortcheckxx)

SortChecker++ is an extension of [SortChecker](https://github.com/yugr/sortcheck) tool
to C++ sorting APIs like `std::sort` or `std::binary_search`.
It verifies that comparators satisfy the [Strict Weak Ordering](https://medium.com/@shiansu/strict-weak-ordering-and-the-c-stl-f7dcfa4d4e07)
axioms.

We tested the tool on LLVM 6.0 (Ubuntu 18.04) and 10.0 (Ubuntu 20.04) for now.

**It is currently work in progress.**

# How to build

To use, first install dependencies:
```
$ sudo apt install libclang-dev llvm-dev
```
Then build the tool
```
$ make clean all
```

# How to use

SortChecker works by instrumenting, i.e. inserting additional checking code,
into the source file. You can run it manually:
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
* `SORTCHECK_VERBOSE` - verbosity
* `SORTCHECK_SYSLOG` - dump messages to syslog (in addition to stderr)
* `SORTCHECK_ABORT_ON_ERROR` - call `abort()` on detected error
* `SORTCHECK_EXIT_CODE` - call `exit(CODE)` on detected error
* `SORTCHECK_CHECKS` - set which checks are enabled via bitmask

# TODO

- apply to test packages
- integrate to old sortcheck (?)
