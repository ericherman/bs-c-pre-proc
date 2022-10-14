/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2022 Eric Herman <eric@freesa.org> */

#include <limits.h>
#include <stdarg.h>

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bs-cpp.h"
#include "bs-util.h"

/* globals for testing */
FILE *bs_stdin = NULL;
#define BS_STDIN (bs_stdin ? bs_stdin : stdin)
FILE *bs_stdout = NULL;
#define BS_STDOUT (bs_stdout ? bs_stdout : stdout)
FILE *bs_stderr = NULL;
#define BS_STDERR (bs_stderr ? bs_stderr : stderr)

/* global function pointers for tests to intercept */
int (*bs_vfprintf)(FILE *restrict stream, const char *restrict format,
		   va_list ap) = vfprintf;

/* local prototypes, not static for testing */
int bs_include(int fdout, char *buf, size_t bufsize, size_t offset, FILE *log);
char *bs_name_from_include(char *buf, char start_delim, char until_delim,
			   char **name_end, FILE *log);

int bs_preprocess(int fdin, int fdout, char *buf, size_t bufsize, FILE *log)
{
	assert(buf);
	assert(bufsize > 0);
	assert(log);

	int continue_long_line = 0;

	FILE *in = fdopen(fdin, "r");
	if (!in) {
		const char *fmt = "fdopen(%d, \"r\") returned NULL";
		int save_errno = Bs_log_errno(log, fmt, fdin);
		return save_errno ? save_errno : 1;
	}
	// fgets() reads in at most one less than size characters from
	// stream and stores them into the buffer pointed to by s.
	// Reading stops after an EOF or a newline.
	// If a newline is read, it is stored into the buffer.
	// A terminating null byte is stored after the last character in
	// the buffer.
	memset(buf, 0x00, bufsize);
	while (fgets(buf, bufsize, in)) {
		size_t ws = 0;
		while (!continue_long_line && buf[ws] &&
		       (buf[ws] == ' ' || buf[ws] == '\t')) {
			++ws;
		}
		size_t offset = ws;
		char *from = buf + offset;
		int include_line = (!continue_long_line) &&
		    (!strncmp(from, "#include", strlen("#include")));

		if (include_line) {
			int error =
			    bs_include(fdout, buf, bufsize, offset, log);
			if (error) {
				return error;
			}
		} else {
			ssize_t written = bs_write(fdout, buf, strlen(buf));
			if (written < 0) {
				const char *fmt = "write(%d, buf, %zu)"
				    " returned %zd";
				int save_errno =
				    Bs_log_errno(log, fmt, fdout, bufsize,
						 written);
				return save_errno ? save_errno : 1;
			} else if ((((size_t)written) == (bufsize - 1))
				   && (buf[bufsize - 1] != '\n')) {
				continue_long_line = 1;
			} else {
				continue_long_line = 0;
			}
		}
		memset(buf, 0x00, bufsize);
	}
	bs_fclose(in);
	return 0;
}

int bs_include(int fdout, char *buf, size_t bufsize, size_t offset, FILE *log)
{
	char *name, *name_end;
	char delim1 = '"';
	char delim2 = '"';
	char *from = buf + offset;
	name = bs_name_from_include(from, delim1, delim2, &name_end, log);
	if (!name) {
		return 1;
	}
	// we will not process anything more on this line
	// NULL-terminate the string, so we can use it for the
	// path for fopen.
	*name_end = '\0';

	int fdinclude = bs_open(name, O_RDONLY);
	if (fdinclude < 0) {
		const char *fmt = "open(\"%s\", O_RDONLY) returned %d";
		int save_errno = Bs_log_errno(log, fmt, name, fdinclude);
		return save_errno ? save_errno : 1;
	}

	int error = bs_preprocess(fdinclude, fdout, buf, bufsize, log);
	bs_close(fdinclude);

	return error;
}

char *bs_name_from_include(char *buf, char start_delim, char until_delim,
			   char **name_end, FILE *log)
{
	assert(name_end);

	char *name = strchr(buf + strlen("#include") + 1, start_delim);
	if (name) {
		*name_end = strchr(name + 1, until_delim);
		if (!*name_end) {
			fprintf(log, "parse error"
				" #include has unbalanced quotes."
				" Opening quote but no close with: "
				" %s\n", buf);
			return NULL;
		}
		// skip the quote
		name += 1;
	}
	return name;
}

int bs_cpp(int argc, char **argv)
{
	char *buf = NULL;
	int error = 0;

	if (argc < 2) {
		fprintf(BS_STDERR, "usage: %s path/to/in.c path/to/out.c\n",
			argv[0]);
		return EXIT_FAILURE;
	}
	const char *in_path = argv[1];
	const char *out_path = argv[2];

	int fdin = bs_open(in_path, O_RDONLY);
	if (fdin < 0) {
		const char *fmt = "open(\"%s\", O_RDONLY) returned %d";
		int save_errno = Bs_log_errno(BS_STDERR, fmt, in_path, fdin);
		return save_errno ? save_errno : 1;
	}

	mode_t mode = 0664;
	int fdout = bs_open(out_path, O_CREAT | O_WRONLY | O_TRUNC, mode);
	if (fdin < 0) {
		const char *fmt =
		    "open(\"%s\", O_CREAT|O_WRONLY|O_TRUNC) returned %d";
		int save_errno = Bs_log_errno(BS_STDERR, fmt, out_path, fdout);
		error = save_errno ? save_errno : 1;
		goto bs_cpp_end;
	}

	const size_t longest_line_we_expect = 1000 + (2 * PATH_MAX);
	const size_t bufsize = longest_line_we_expect;
	buf = bs_malloc(bufsize);
	if (!buf) {
		fprintf(BS_STDERR, "could not malloc(%zu)\n", bufsize);
		error = 1;
		goto bs_cpp_end;
	}

	FILE *log = BS_STDERR;
	error = bs_preprocess(fdin, fdout, buf, bufsize, log);

bs_cpp_end:
	bs_free(buf);
	if (fdout >= 0) {
		bs_close(fdout);
	}
	if (fdin >= 0) {
		bs_close(fdin);
	}

	return error ? EXIT_FAILURE : EXIT_SUCCESS;
}
