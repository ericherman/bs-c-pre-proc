/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2022 Eric Herman <eric@freesa.org> */

#include "bs-cpp.h"
#include "bs-util.h"
#include "test-util.h"

#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

FILE *foo_h = NULL;
int fdfoo_h = -1;

FILE *baz_h = NULL;
int fdbaz_h = -1;

unsigned global_errors = 0;

const char *foo_h_txt = "\
int foo(void);\n\
int bar(void);\n\
";

const char *baz_h_txt = "\
#include \"foo.h\"\n\
int baz(void);\n\
";

void slurp(char *buf, size_t bufsize, FILE *f)
{
	memset(buf, 0x00, bufsize);
	size_t max = bufsize - 1;

	fseek(f, 0L, SEEK_END);
	size_t len = ftell(f);
	rewind(f);
	fread(buf, 1, len < max ? len : max, f);
}

int our_open(const char *path, int options, ...)
{
	va_list argp;
	va_start(argp, options);

	int have_mode = 0;
	mode_t mode = 0;
	if (options & (O_CREAT | O_WRONLY | O_TRUNC)) {
		have_mode = 1;
		mode = va_arg(argp, mode_t);
	}
	va_end(argp);

	if (strcmp(path, "foo.h") == 0) {
		if (Check(!foo_h, "expected foo_h to be NULL, second call?")) {
			++global_errors;
			return -1;
		}
		if (Check(mode == O_RDONLY,
			  "expected mode of O_RDONLY but was %d\n", mode)) {
			++global_errors;
			return -1;
		}
		foo_h = tmpfile();
		fprintf(foo_h, "%s", foo_h_txt);
		fflush(foo_h);
		rewind(foo_h);
		fdfoo_h = fileno(foo_h);
		if (Check(fdfoo_h >= 0, "fdfoo_h: %d\n", fdfoo_h)) {
			++global_errors;
		}
		return fdfoo_h;
	}
	if (strcmp(path, "baz.h") == 0) {
		if (Check(!baz_h, "expected baz_h to be NULL, second call?")) {
			++global_errors;
			return -1;
		}
		if (Check(mode == O_RDONLY,
			  "expected mode of O_RDONLY but was %d\n", mode)) {
			++global_errors;
			return -1;
		}
		baz_h = tmpfile();
		fprintf(baz_h, "%s", baz_h_txt);
		fflush(baz_h);
		rewind(baz_h);
		fdbaz_h = fileno(baz_h);
		if (Check(fdbaz_h >= 0, "fdbaz_h: %d\n", fdbaz_h)) {
			++global_errors;
		}
		return fdbaz_h;
	}

	if (have_mode) {
		return open(path, options, mode);
	} else {
		return open(path, options);
	}
}

int our_close(int fd)
{
	int was_foo_h = (fd == fdfoo_h);
	int was_baz_h = (fd == fdbaz_h);

	if (was_foo_h) {
		int rv = fclose(foo_h);
		foo_h = NULL;
		fdfoo_h = -1;
		return rv;
	}
	if (was_baz_h) {
		int rv = fclose(baz_h);
		baz_h = NULL;
		fdbaz_h = -1;
		return rv;
	}

	return close(fd);
}

int bs_include(int fdout, char *buf, size_t bufsize, size_t offset, FILE *log);

unsigned test_simple_include(void)
{
	unsigned failures = 0;
	foo_h = NULL;
	global_errors = 0;

	bs_open = our_open;
	bs_close = our_close;

	FILE *out = tmpfile();
	fprintf(out, "%s", "");
	fflush(out);
	rewind(out);
	int fdout = fileno(out);
	int saveerrno = errno;
	failures += Check(fdout >= 0,
			  "expected fd >= 0, but was %d (errno %d: %s)",
			  fdout, saveerrno, strerror(saveerrno));

	const size_t bufsize = 80 * 24;
	char logbuf[80 * 24];
	memset(logbuf, 0x00, bufsize);
	FILE *log = fmemopen(logbuf, bufsize, "w");

	char linebuf[80];
	strncpy(linebuf, "  #include \"foo.h\"\n", 80);
	size_t offset = 2;

	int err = bs_include(fdout, linebuf, 80, offset, log);

	fflush(out);
	char outbuf[80 * 24];
	slurp(outbuf, bufsize, out);
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

	bs_open = open;
	bs_close = close;

	return failures;
}

unsigned test_simple_include_include(void)
{
	unsigned failures = 0;
	foo_h = NULL;
	global_errors = 0;

	bs_open = our_open;
	bs_close = our_close;

	FILE *out = tmpfile();
	fprintf(out, "%s", "");
	fflush(out);
	rewind(out);
	int fdout = fileno(out);
	int saveerrno = errno;
	failures += Check(fdout >= 0,
			  "expected fd >= 0, but was %d (errno %d: %s)",
			  fdout, saveerrno, strerror(saveerrno));

	const size_t bufsize = 80 * 24;
	char logbuf[80 * 24];
	memset(logbuf, 0x00, bufsize);
	FILE *log = fmemopen(logbuf, bufsize, "w");

	char linebuf[80];
	strncpy(linebuf, "   #include \"baz.h\"\n", 80);
	size_t offset = 3;

	int err = bs_include(fdout, linebuf, 80, offset, log);

	fflush(out);
	char outbuf[80 * 24];
	slurp(outbuf, bufsize, out);
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

	bs_open = open;
	bs_close = close;

	return failures;
}

int main(void)
{
	unsigned failures = 0;

	failures += run_test(test_simple_include);
	failures += run_test(test_simple_include_include);

	return failures_to_status("test_exit_reason", failures);
}
