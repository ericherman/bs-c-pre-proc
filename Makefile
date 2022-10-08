# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2022 Eric Herman <eric@freesa.org>

# $@ : target label
# $< : the first prerequisite after the colon
# $^ : all of the prerequisite files
# $* : wildcard matched part
# Target-specific Variable syntax:
# https://www.gnu.org/software/make/manual/html_node/Target_002dspecific.html
#
# patsubst : $(patsubst pattern,replacement,text)
#	https://www.gnu.org/software/make/manual/html_node/Text-Functions.html

BROWSER=firefox

COMMON_CFLAGS += -g -Wall -Wextra -pedantic -Werror -I./src $(CFLAGS)

BUILD_CFLAGS += -DNDEBUG -O2 $(COMMON_CFLAGS)

DEBUG_CFLAGS += -DDEBUG -O0 $(COMMON_CFLAGS) \
	-fno-inline-small-functions \
	-fkeep-inline-functions \
	-fkeep-static-functions \
	--coverage

SHELL=/bin/bash

ifeq ($(VALGRIND),)
VALGRIND=0
endif

ifneq ($(VALGRIND), 0)
VALGRIND_CMD=$(shell which valgrind)
endif

# extracted from https://github.com/torvalds/linux/blob/master/scripts/Lindent
LINDENT=indent -npro -kr -i8 -ts8 -sob -l80 -ss -ncs -cp1 -il0
# see also: https://www.kernel.org/doc/Documentation/process/coding-style.rst


# .SECONDARY with no prerequisites causes all targets to be treated as
# secondary (i.e., no target is removed because it is considered intermediate).
# https://www.gnu.org/software/make/manual/html_node/Special-Targets.html
.SECONDARY:

.PHONY: default
default: check

src/bs-cpp.c: src/bs-cpp.h
src/bs-util.c: src/bs-util.h
tests/test-util.c: tests/test-util.h

build/bs-cpp: src/bs-cpp.c src/bs-cpp-main.c
	mkdir -pv build
	$(CC) $(BUILD_CFLAGS) $^ -o $@

debug/bs-cpp.o: src/bs-cpp.c
	mkdir -pv debug
	$(CC) -c $(DEBUG_CFLAGS) $< -o $@

debug/bs-util.o: src/bs-util.c
	mkdir -pv debug
	$(CC) -c $(DEBUG_CFLAGS) $< -o $@

debug/bs-cpp: debug/bs-cpp.o src/bs-cpp-main.c
	mkdir -pv debug
	$(CC) $(DEBUG_CFLAGS) $^ -o $@

build/bs-strip-backslash-nl: src/bs-strip-backslash-nl.c src/bs-util.c
	mkdir -pv debug
	$(CC) $(BUILD_CFLAGS) $^ -o $@

debug/bs-strip-backslash-nl: src/bs-strip-backslash-nl.c debug/bs-util.o
	mkdir -pv debug
	$(CC) $(DEBUG_CFLAGS) $^ -o $@

debug/tests/test-util.o: tests/test-util.c tests/test-util.h
	mkdir -pv debug/tests
	$(CC) -c $(DEBUG_CFLAGS) $< -o $@

debug/tests/test-%: debug/bs-cpp.o debug/tests/test-util.o tests/test-%.c
	$(CC) $(DEBUG_CFLAGS) $^ -o $@

.PHONY: check-%
check-%: debug/tests/test-%
	$(VALGRIND_CMD) ./$< 2>&1 | tee debug/$@.out
	bin/valgrind-check debug/$@.out
	rm debug/$@.out
	@echo "SUCCESS! ($@)"

.PHONY: check-unit
check-unit: check-simple-include check-name-from-include check-sane-fprintf
	@echo "SUCCESS! ($@)"

.PHONY: check-accpetance-0
check-accpetance-0: tests/acceptance-0.sh \
		debug/bs-strip-backslash-nl \
		build/bs-strip-backslash-nl
	$< $(VALGRIND_CMD) debug/bs-strip-backslash-nl 2>&1 | tee debug/$@.out
	bin/valgrind-check debug/$@.out
	rm debug/$@.out
	$< build/bs-strip-backslash-nl
	@echo "SUCCESS! ($@)"

.PHONY: check-accpetance-1
check-accpetance-1: tests/acceptance-1.sh debug/bs-cpp build/bs-cpp
	$< debug/bs-cpp
	$< build/bs-cpp
	@echo "SUCCESS! ($@)"

.PHONY: check-accpetance
check-accpetance: check-accpetance-0 check-accpetance-1
	@echo "SUCCESS! ($@)"

.PHONY: check
check: check-unit check-accpetance
	@echo "SUCCESS! ($@)"

coverage.info: check
	lcov    --checksum \
		--capture \
		--base-directory . \
		--directory . \
		--output-file coverage.info
	ls -l coverage.info

coverage_html/src/index.html: coverage.info
	mkdir -pv ./coverage_html
	genhtml coverage.info --output-directory coverage_html
	ls -l $@

coverage_html/src/bs-cpp.c.gcov.html: coverage_html/src/index.html
	ls -l $@

check-code-coverage: coverage_html/src/bs-cpp.c.gcov.html
	# expect two: one for lines, one for functions
	if [ $$(grep -c 'headerCovTableEntryHi">100.0' $<) -eq 2 ]; \
		then true; else false; fi
	@echo "SUCCESS! ($@)"

coverage: coverage_html/src/index.html
	$(BROWSER) $<

tidy:
	$(LINDENT) \
		-T FILE -T size_t -T ssize_t \
		-T mode_t -T pid_t \
		-T bs_pipe_function \
		src/*.c src/*.h tests/*.c tests/*.h

clean:
	rm -rvf build debug `cat .gitignore | sed -e 's/#.*//'`
	pushd src; rm -rvf `cat ../.gitignore | sed -e 's/#.*//'`; popd
	pushd tests; rm -rvf `cat ../.gitignore | sed -e 's/#.*//'`; popd

mrproper:
	git clean -dffx
	git submodule foreach --recursive git clean -dffx
