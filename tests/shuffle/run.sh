#!/bin/sh

# The MIT License (MIT)
# 
# Copyright (c) 2023 Yury Gribov
# 
# Use of this source code is governed by The MIT License (MIT)
# that can be found in the LICENSE.txt file.

# Check bsearch instrumentation.

set -eu
#set -x

cd $(dirname $0)

ROOT=$PWD/../..
PATH=$ROOT/scripts:$PATH

CXXFLAGS='-Wall -Wextra -Werror -g'

export SORTCHECK_ABORT=0

c++ repro.cpp $CXXFLAGS

./a.out > test.log 2>&1 || true
if grep 'irreflexive comparator' test.log; then
  echo >&2 'Reference reported unexpected error'
  cat test.log >&2
  exit 1
fi

SORTCHECK_SHUFFLE=0 ./a.out > test.log 2>&1 || true
if ! diff -q repro.ref test.log; then
  echo >&2 'Modified test did not produce expected output:'
  diff repro.ref test.log >&2
  exit 1
fi

echo SUCCESS
