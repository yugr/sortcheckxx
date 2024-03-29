# The MIT License (MIT)
# 
# Copyright (c) 2022 Yury Gribov
# 
# Use of this source code is governed by The MIT License (MIT)
# that can be found in the LICENSE.txt file.

CXX ?= g++
LLVM_CONFIG ?= llvm-config
DESTDIR ?= /usr/local

CPPFLAGS = $(shell $(LLVM_CONFIG) --cppflags) -isystem $(shell $(LLVM_CONFIG) --includedir) -DLLVM_ENABLE_ASSERTIONS
CXXFLAGS = $(shell $(LLVM_CONFIG) --cxxflags) -std=c++17 -g -Wall -Wextra -Werror
LDFLAGS = $(shell $(LLVM_CONFIG) --ldflags) -Wl,--warn-common

ifneq (,$(shell $(CXX) --version | grep clang))
  # LLVM flags on 18.04 are terribly broken
  CXXFLAGS += -Wno-unused-command-line-argument -Wno-unknown-warning-option
endif

LLVM_LIBDIR = $(shell $(LLVM_CONFIG) --libdir)
LIBS = -Wl,--start-group $(shell find $(LLVM_LIBDIR) -name 'libclang[A-Z]*.a') -Wl,--end-group $(shell $(LLVM_CONFIG) --libs --system-libs)

ifneq (,$(COVERAGE))
  DEBUG = 1
  CXXFLAGS += --coverage -DNDEBUG
  LDFLAGS += --coverage
endif
ifeq (,$(DEBUG))
  CXXFLAGS += -O2
  LDFLAGS += -Wl,-O2
else
  CXXFLAGS += -O0
endif
ifneq (,$(ASAN))
  CXXFLAGS += -fsanitize=address -fsanitize-address-use-after-scope -U_FORTIFY_SOURCE -fno-common -D_GLIBCXX_SANITIZE_VECTOR
  LDFLAGS += -fsanitize=address
endif
ifneq (,$(UBSAN))
  # Isan finds issues in Clang itself so disable it
  CXXFLAGS += -fsanitize=undefined -fno-sanitize-recover=undefined
  LDFLAGS += -fsanitize=undefined -fno-sanitize-recover=undefined
endif

$(shell mkdir -p bin)

all: bin/SortChecker

bin/SortChecker: bin/SortChecker.o Makefile bin/FLAGS
	$(CXX) $(LDFLAGS) -o $@ $(filter %.o, $^) $(LIBS)

bin/%.o: src/%.cpp Makefile bin/FLAGS
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $@ -c $<

bin/FLAGS: FORCE
	if test x"$(CFLAGS) $(CXXFLAGS) $(LDFLAGS)" != x"$$(cat $@)"; then \
	  echo "$(CFLAGS) $(CXXFLAGS) $(LDFLAGS)" > $@; \
	fi

check:
	@tests/runtests.sh

clean:
	rm -f bin/*
	find -name \*.gcov -o -name \*.gcda -o -name \*.gcno | xargs rm -f

.PHONY: clean all check FORCE

