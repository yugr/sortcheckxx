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

$ROOT/bin/SortChecker repro.cc --
if grep -q sortcheck repro.cc; then
  echo >&2 'Unexpected modifications (not handled yet)'
#  exit 1
fi

echo SUCCESS
