#!/bin/sh

# The MIT License (MIT)
# 
# Copyright (c) 2022 Yury Gribov
# 
# Use of this source code is governed by The MIT License (MIT)
# that can be found in the LICENSE.txt file.

# Check that wrapper handles various weird cases.

set -eu
#set -x

cd $(dirname $0)

ROOT=$PWD/../..
PATH=$ROOT/scripts:$PATH

# Separate compilation
cc example.cc -c
cc example.o
./a.out | grep -q 'Hello world'

# CMake-style object names
cc example.cc -c -o example.cc.o
cc example.cc.o
./a.out | grep -q 'Hello world'
