#!/bin/sh

# The MIT License (MIT)
# 
# Copyright (c) 2022 Yury Gribov
# 
# Use of this source code is governed by The MIT License (MIT)
# that can be found in the LICENSE.txt file.

set -eu
#set -x

for d in $(dirname $0)/*; do
  ! test -d $d || $d/run.sh
done
