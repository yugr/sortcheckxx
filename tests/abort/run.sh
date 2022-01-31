#!/bin/sh

# The MIT License (MIT)
# 
# Copyright (c) 2022 Yury Gribov
# 
# Use of this source code is governed by The MIT License (MIT)
# that can be found in the LICENSE.txt file.

# Check that SORTCHECK_ABORT works.

set -eu
#set -x

cd $(dirname $0)

ROOT=$PWD/../..
PATH=$ROOT/scripts:$PATH
INC=$ROOT/include

CXXFLAGS='-Wall -Wextra -Werror -g'

ulimit -c 1024

c++ $CXXFLAGS abort.cpp
if ./a.out > test.log 2>&1; then
  echo >&2 'Test did not fail as expected'
  exit 1
fi
if ! diff -q abort.ref test.log; then
  echo >&2 'Test did not produce expected output:'
  diff abort.ref test.log >&2
  exit 1
fi

echo SUCCESS
