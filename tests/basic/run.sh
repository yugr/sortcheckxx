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
  for test in reflex symmetry trans; do
    c++ $test.cpp $CXXFLAGS -std=$std
    if ./a.out > test.log 2>&1; then
      echo >&2 'Test did not fail as expected'
      exit 1
    fi
    if ! diff -q $test.ref test.log; then
      echo >&2 'Test did not produce expected output:'
      diff $test.ref test.log >&2
      exit 1
    fi
  done
done

echo SUCCESS
