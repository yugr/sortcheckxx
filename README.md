SortChecker++ is an extension of [SortChecker](https://github.com/yugr/sortcheck) tool
to C++ sorting APIs like `std::sort` or `std::binary_search`.

It's currently work in progress.

To use, first install dependencies:
```
$ sudo apt install libclang-dev llvm
```
and then build
```
$ make clean all
```

For easier integration, you can use compiler wrappers in `scripts/` folders.

TODO:
- syslogging
- apply to test packages
- integrate to old sortcheck (?)
