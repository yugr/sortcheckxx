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

$ROOT/bin/SortChecker -v example.cpp > test.log 2>&1
if ! grep -q 'Found relevant function' test.log; then
  echo >&2 'Missing verbose print'
  exit 1
fi

echo SUCCESS
