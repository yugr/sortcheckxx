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
PATH=$ROOT/scripts:$PATH

if $ROOT/bin/SortChecker error.cc -- 2>/dev/null; then
  echo >&2 "Error not reported correctly"
  exit 1
fi

$ROOT/bin/SortChecker --ignore-parse-errors error.cc --

echo SUCCESS
