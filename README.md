SortChecker++ is an extension of [SortChecker](https://github.com/yugr/sortcheck) tool
to C++ sorting APIs like `std::sort` or `std::binary_search`.
It verifies that comparators satisfy the [Strict Weak Ordering](https://medium.com/@shiansu/strict-weak-ordering-and-the-c-stl-f7dcfa4d4e07)
axioms.

**It is currently work in progress.**

To use, first install dependencies:
```
$ sudo apt install libclang-dev llvm
```
We only support LLVM 10 for now (default on Ubuntu 20.04).
Then build
```
$ make clean all
```

For easier integration, you can use compiler wrappers in `scripts/` folders:
```
$ PATH=path/to/scripts:$PATH make clean all
```
You could also run tool manually:
```
$ SortChecker file.cpp -- $CXXFLAGS
```

TODO:
- syslogging
- apply to test packages
- integrate to old sortcheck (?)
