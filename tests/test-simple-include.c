/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2022 Eric Herman <eric@freesa.org> */

#include "bs-cpp.h"
#include "test-util.h"

#include <string.h>

FILE *foo_h = NULL;
unsigned global_errors = 0;

const char *foo_h_txt = "" "int foo(void);\n" "int bar(void);\n";

extern FILE *(*bs_fopen)(const char *restrict path, const char *restrict mode);
FILE *our_fopen(const char *restrict path, const char *restrict mode)
{
	if (strcmp(path, "foo.h") == 0) {
		if (Check(!foo_h, "expected foo_h to be NULL, second call?")) {
			++global_errors;
			return NULL;
		}
		if (Check(strcmp(mode, "r") == 0,
			  "expected mode of \"r\" but was %s\n", mode)) {
			++global_errors;
			return NULL;
		}
		char *buf = ignore_const_s(foo_h_txt);
		foo_h = fmemopen(buf, strlen(foo_h_txt), mode);
		return foo_h;
	}
	return fopen(path, mode);
}

extern int (*bs_fclose)(FILE *stream);
int our_fclose(FILE *stream)
{
	int was_foo_h = (stream == foo_h);
	int rv = fclose(stream);
	if (was_foo_h) {
		foo_h = NULL;
	}
	return rv;
}

int bs_include(FILE *out, char *buf, size_t bufsize, size_t offset, FILE *log);
unsigned test_simple_include(void)
{
	unsigned failures = 0;

	bs_fopen = our_fopen;
	bs_fclose = our_fclose;

	const size_t buflen = 80 * 24;
	char outbuf[80 * 24];
	memset(outbuf, 0x00, buflen);
	FILE *out = fmemopen(outbuf, buflen, "w");

	char logbuf[80 * 24];
	memset(logbuf, 0x00, buflen);
	FILE *log = fmemopen(logbuf, buflen, "w");

	char linebuf[80];
	strncpy(linebuf, "  #include \"foo.h\"\n", 80);
	size_t offset = 2;

	int err = bs_include(out, linebuf, 80, offset, log);
	failures += Check(err == 0, "expected error == 0, but was %d\n", err);

	failures += Check(global_errors == 0,
			  "global_errors == %u\n", global_errors);

	fflush(out);
	fclose(out);
	out = NULL;
	failures += Check((strcmp(outbuf, foo_h_txt) == 0),
			  "expected: '%s'\n"
			  " but was: '%s'\n", foo_h_txt, outbuf);

	fflush(log);
	fclose(log);
	log = NULL;
	failures += Check((strcmp(logbuf, "") == 0), "errorlong: %s\n", logbuf);

	bs_fopen = fopen;
	bs_fclose = fclose;

	return failures;
}

int main(void)
{
	unsigned failures = 0;

	failures += run_test(test_simple_include);

	return failures_to_status("test_exit_reason", failures);
}
