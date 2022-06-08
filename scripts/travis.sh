#!/bin/sh

# Copyright 2022 Yury Gribov
# 
# Use of this source code is governed by MIT license that can be
# found in the LICENSE.txt file.

set -eu

if test -n "${TRAVIS:-}" -o -n "${GITHUB_ACTIONS:-}"; then
  set -x
fi

if test -n "${GITHUB_ACTIONS:-}" && grep -q 20.04 /etc/lsb-release; then
  # Versions of clang and libclang in Github's Ubuntu 20.04 mismatch
  # which causes header search issues so fix this
  V=$(llvm-config --version | awk -F. '{print $1}')
  sudo ln -fs /usr/bin/clang-$V /usr/bin/clang
fi

cd $(dirname $0)/..

export ASAN_OPTIONS='detect_stack_use_after_return=1:check_initialization_order=1:strict_init_order=1:strict_string_checks=1'

make "$@" clean all
make "$@" check

# Upload coverage
if test -n "${COVERAGE:-}"; then
  # TODO: collect coverage for sortcheck.h and tests
  curl --retry 5 -s https://codecov.io/bash > codecov.bash
  bash codecov.bash -Z
fi
