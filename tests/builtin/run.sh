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

$ROOT/bin/SortChecker builtin.cpp
if ! grep -q sortcheck builtin.cpp; then
  echo >&2 'Unexpected modifications'
  exit 1
fi

echo SUCCESS
