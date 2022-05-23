#!/bin/sh

# The MIT License (MIT)
# 
# Copyright (c) 2022 Yury Gribov
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

for t in *.cpp; do
  stem=$(echo $t | sed 's/\.cpp//')
  c++ $t $CXXFLAGS
  ./a.out > test.log 2>&1 || true
  if ! diff -q $stem.ref test.log; then
    echo >&2 'Test did not produce expected output:'
    diff $stem.ref test.log >&2
    exit 1
  fi
done

echo SUCCESS
