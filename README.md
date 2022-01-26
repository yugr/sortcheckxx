[![License](http://img.shields.io/:license-MIT-blue.svg)](https://github.com/yugr/sortcheckxx/blob/master/LICENSE.txt)
[![Build Status](https://github.com/yugr/sortcheckxx/actions/workflows/ci.yml/badge.svg)](https://github.com/yugr/sortcheckxx/actions)

SortChecker++ is an extension of [SortChecker](https://github.com/yugr/sortcheck) tool
to C++ sorting APIs like `std::sort` or `std::binary_search`.
It verifies that comparators satisfy the [Strict Weak Ordering](https://medium.com/@shiansu/strict-weak-ordering-and-the-c-stl-f7dcfa4d4e07)
axioms.

**It is currently work in progress.**

# How to build

To use, first install dependencies:
```
$ sudo apt install libclang-dev llvm
```
We only support LLVM 10 for now (default on Ubuntu 20.04).
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

# TODO

- syslogging
- apply to test packages
- integrate to old sortcheck (?)
