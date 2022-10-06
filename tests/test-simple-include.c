/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2022 Eric Herman <eric@freesa.org> */

#include "bs-cpp.h"
#include "test-util.h"

#include <string.h>

FILE *foo_h = NULL;
FILE *baz_h = NULL;
unsigned global_errors = 0;

const char *foo_h_txt = "\
int foo(void);\n\
int bar(void);\n\
";

const char *baz_h_txt = "\
#include \"foo.h\"\n\
int baz(void);\n\
";

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
	if (strcmp(path, "baz.h") == 0) {
		if (Check(!baz_h, "expected baz_h to be NULL, second call?")) {
			++global_errors;
			return NULL;
		}
		if (Check(strcmp(mode, "r") == 0,
			  "expected mode of \"r\" but was %s\n", mode)) {
			++global_errors;
			return NULL;
		}
		char *buf = ignore_const_s(baz_h_txt);
		baz_h = fmemopen(buf, strlen(baz_h_txt), mode);
		return baz_h;
	}
	return fopen(path, mode);
}

extern int (*bs_fclose)(FILE *stream);
int our_fclose(FILE *stream)
{
	int was_foo_h = (stream == foo_h);
	int was_baz_h = (stream == baz_h);

	int rv = fclose(stream);

	if (was_foo_h) {
		foo_h = NULL;
	}
	if (was_baz_h) {
		baz_h = NULL;
	}

	return rv;
}

int bs_include(FILE *out, char *buf, size_t bufsize, size_t offset, FILE *log);

unsigned test_simple_include(void)
{
	unsigned failures = 0;
	foo_h = NULL;
	global_errors = 0;

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

	fflush(out);
	fclose(out);
	out = NULL;

	fflush(log);
	fclose(log);
	log = NULL;

	failures += Check(err == 0, "expected error == 0, but was %d\n", err);
	failures += Check((strcmp(logbuf, "") == 0), "errorlog: %s\n", logbuf);
	failures += Check(global_errors == 0,
			  "global_errors == %u\n", global_errors);

	failures += Check((strcmp(outbuf, foo_h_txt) == 0),
			  "expected: '%s'\n"
			  " but was: '%s'\n", foo_h_txt, outbuf);

	bs_fopen = fopen;
	bs_fclose = fclose;

	return failures;
}

unsigned test_simple_include_include(void)
{
	unsigned failures = 0;
	foo_h = NULL;
	global_errors = 0;

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
	strncpy(linebuf, "   #include \"baz.h\"\n", 80);
	size_t offset = 3;

	int err = bs_include(out, linebuf, 80, offset, log);

	fflush(out);
	fclose(out);
	out = NULL;

	fflush(log);
	fclose(log);
	log = NULL;

	failures += Check(err == 0, "expected error == 0, but was %d\n", err);
	failures += Check((strcmp(logbuf, "") == 0), "errorlog: %s\n", logbuf);
	failures += Check(global_errors == 0,
			  "global_errors == %u\n", global_errors);

	failures += Check((strstr(outbuf, foo_h_txt)),
			  "expected: '%s'\n"
			  " but was: '%s'\n", foo_h_txt, outbuf);

	const char *baz_txt = "int baz(void);\n";
	failures += Check((strstr(outbuf, baz_txt)),
			  "expected: '%s'\n"
			  " but was: '%s'\n", baz_txt, outbuf);

	bs_fopen = fopen;
	bs_fclose = fclose;

	return failures;
}

int main(void)
{
	unsigned failures = 0;

	failures += run_test(test_simple_include);
	failures += run_test(test_simple_include_include);

	return failures_to_status("test_exit_reason", failures);
}
