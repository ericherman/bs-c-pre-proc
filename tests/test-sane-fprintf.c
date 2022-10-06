/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2022 Eric Herman <eric@freesa.org> */

#include "bs-cpp.h"
#include "test-util.h"

#include <string.h>
#include <stdlib.h>

size_t sane_fprintf(FILE *stream, size_t max, FILE *err, const char *fmt, ...);

unsigned test_sane_fprintf(void)
{
	unsigned failures = 0;

	const size_t buflen = 80 * 24;
	char errbuf[80 * 24];
	memset(errbuf, 0x00, buflen);
	FILE *err = fmemopen(errbuf, buflen, "w");

	char outbuf[80 * 24];
	memset(outbuf, 0x00, buflen);
	FILE *out = fmemopen(outbuf, buflen, "w");

	size_t written = sane_fprintf(out, buflen, err, "%s", "foo");

	fflush(out);
	fclose(out);
	out = NULL;

	fflush(err);
	fclose(err);
	err = NULL;

	failures += Check((strcmp(errbuf, "") == 0), "error log: %s\n", errbuf);
	failures += Check(written == strlen("foo"),
			  "expected 3, was: %zu\n", written);
	failures += Check((strcmp(outbuf, "foo") == 0), "out: %s\n", outbuf);

	return failures;
}

extern int (*bs_vfprintf)(FILE *restrict stream, const char *restrict format,
			  va_list ap);

int fail_vfprintf(FILE *restrict stream, const char *restrict format,
		  va_list ap)
{
	(void)stream;
	(void)format;
	(void)ap;
	return -1;
}

extern void (*bs_exit)(int status);
int exit_value = 0;
int exit_called = 0;
void faux_exit(int status)
{
	++exit_called;
	exit_value = status;
}

unsigned test_sane_fprintf_fail(void)
{
	unsigned failures = 0;
	exit_value = 0;
	exit_called = 0;

	bs_vfprintf = fail_vfprintf;
	bs_exit = faux_exit;

	const size_t buflen = 80 * 24;
	char errbuf[80 * 24];
	memset(errbuf, 0x00, buflen);
	FILE *err = fmemopen(errbuf, buflen, "w");

	char outbuf[80 * 24];
	memset(outbuf, 0x00, buflen);
	FILE *out = fmemopen(outbuf, buflen, "w");

	sane_fprintf(out, buflen, err, "%s", "foobar");

	fflush(out);
	fclose(out);
	out = NULL;

	fflush(err);
	fclose(err);
	err = NULL;

	failures += Check(exit_called == 1, "exit_called: %d", exit_called);
	failures += Check(exit_value != 0, "exit_value == 0");
	failures += Check((strcmp(errbuf, "") != 0), "err: '%s'", errbuf);

	bs_exit = exit;
	bs_vfprintf = vfprintf;
	return failures;
}

int main(void)
{
	unsigned failures = 0;

	failures += run_test(test_sane_fprintf);
	failures += run_test(test_sane_fprintf_fail);

	return failures_to_status("test_exit_reason", failures);
}
