#!/bin/sh

# The MIT License (MIT)
# 
# Copyright (c) 2022 Yury Gribov
# 
# Use of this source code is governed by The MIT License (MIT)
# that can be found in the LICENSE.txt file.

# Check that -v works.

set -eu
#set -x

cd $(dirname $0)

ROOT=$PWD/../..
PATH=$ROOT/scripts:$PATH

CXXFLAGS='-Wall -Wextra -Werror -g'
if test "${COVERAGE:-}"; then
  CXXFLAGS="$CXXFLAGS --coverage"
fi

g++ $CXXFLAGS example.cpp

export SORTCHECK_OUTPUT=file.txt
rm -f $SORTCHECK_OUTPUT

if ./a.out > test.log 2>&1; then
  echo >&2 'Test did not fail as expected'
  exit 1
fi
if ! diff -q example.ref $SORTCHECK_OUTPUT; then
  echo >&2 'Test did not produce expected output:'
  diff example.ref $SORTCHECK_OUTPUT >&2
  exit 1
fi

rm -f $SORTCHECK_OUTPUT

echo SUCCESS
