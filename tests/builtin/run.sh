#!/bin/sh

# The MIT License (MIT)
# 
# Copyright (c) 2022 Yury Gribov
# 
# Use of this source code is governed by The MIT License (MIT)
# that can be found in the LICENSE.txt file.

# Check that primitive types are not instrumented.

set -eu
#set -x

cd $(dirname $0)

ROOT=$PWD/../..

for file in builtin.cpp string.cpp map.cpp; do
  $ROOT/bin/SortChecker $file --
  if grep -q sortcheck $file; then
    echo >&2 'Unexpected modifications'
    exit 1
  fi
done

echo SUCCESS
