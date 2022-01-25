#!/bin/sh

# The MIT License (MIT)
# 
# Copyright (c) 2022 Yury Gribov
# 
# Use of this source code is governed by The MIT License (MIT)
# that can be found in the LICENSE.txt file.

set -eu
#set -x

cd $(dirname $0)

ROOT=$PWD/../..
PATH=$ROOT/scripts:$PATH
INC=$ROOT/include

CXXFLAGS="-I$ROOT/include -Wall -Wextra -Werror -g"

for std in c++98 c++11 c++14 c++17; do
  for test in *.cpp; do
    stem=$(echo $test | sed 's/\.cpp//')
    c++ $test $CXXFLAGS -std=$std
    ./a.out > test.log 2>&1 || true
    if ! diff -q $stem.ref test.log; then
      echo >&2 'Test did not produce expected output:'
      diff $stem.ref test.log >&2
      exit 1
    fi
  done
done

echo SUCCESS
