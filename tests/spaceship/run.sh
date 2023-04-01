#!/bin/sh

# The MIT License (MIT)
# 
# Copyright (c) 2023 Yury Gribov
# 
# Use of this source code is governed by The MIT License (MIT)
# that can be found in the LICENSE.txt file.

# Check spaceship instrumentation.

set -eu
#set -x

cd $(dirname $0)

ROOT=$PWD/../..
PATH=$ROOT/scripts:$PATH

CXXFLAGS='-Wall -Wextra -Werror -g -std=c++20'

export SORTCHECK_ABORT=0

if ! g++ $CXXFLAGS -S -x c++ /dev/null 2>/dev/null; then
  # C++20 is not supported
  exit 0
fi

c++ repro.cpp $CXXFLAGS
./a.out > test.log 2>&1 || true
if ! diff -q repro.ref test.log; then
  echo >&2 'Test did not produce expected output:'
  diff repro.ref test.log >&2
  exit 1
fi

echo SUCCESS
