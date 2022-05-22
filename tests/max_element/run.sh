#!/bin/sh

# The MIT License (MIT)
# 
# Copyright (c) 2022 Yury Gribov
# 
# Use of this source code is governed by The MIT License (MIT)
# that can be found in the LICENSE.txt file.

# Check std::sort instrumentation.

set -eu
#set -x

cd $(dirname $0)

ROOT=$PWD/../..
PATH=$ROOT/scripts:$PATH

CXXFLAGS='-Wall -Wextra -Werror -g'

if test -n "${COVERAGE:-}"; then
  CXXFLAGS="$CXXFLAGS --coverage"
fi

c++ $CXXFLAGS bad.cpp

export SORTCHECK_ABORT=0

if ./a.out > test.log 2>&1; then
  echo >&2 'Test did not fail as expected'
  exit 1
fi
if ! diff -q bad.ref test.log; then
  echo >&2 'Test did not produce expected output:'
  diff bad.ref test.log >&2
  exit 1
fi

echo SUCCESS
