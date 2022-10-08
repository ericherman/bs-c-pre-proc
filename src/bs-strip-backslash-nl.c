/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2022 Eric Herman <eric@freesa.org> */

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bs-util.h"

int bs_strip_backslash_newline(int fd_from, int fd_to, FILE *errlog)
{
	const size_t bufsize = sizeof(size_t) + 1;
	char buf[sizeof(size_t) + 1];
	memset(buf, 0x00, bufsize);

	int have_backslash = 0;
	int error = 0;

	while (1) {
		int skip = 0;
		size_t read_size = 1;
		ssize_t bytes = read(fd_from, buf, read_size);
		if (bytes < 0) {
			const char *fmt = "read(fd, buf, %zu) returned %zd";
			int save_errno =
			    Bs_log_errno(errlog, fmt, read_size, bytes);
			error = save_errno ? save_errno : 1;
			goto bs_strip_backslash_newline_end;
		}
		if (!bytes) {
			if (have_backslash) {
				buf[0] = '\\';
				buf[1] = '\0';
				write(fd_to, buf, 1);
			}
			goto bs_strip_backslash_newline_end;
		}

		char c = buf[0];
		if (have_backslash) {
			if (c == '\n') {
				have_backslash = 0;
				skip = 1;
			} else {
				buf[0] = '\\';
				buf[1] = '\0';
				write(fd_to, buf, 1);
				have_backslash = 0;
				skip = 0;
			}
		}
		if (c == '\\') {
			have_backslash = 1;
		} else if (!skip) {
			buf[0] = c;
			buf[1] = '\0';
			write(fd_to, buf, 1);
		}
	}

bs_strip_backslash_newline_end:
	/* done with "out" end of pipe */
	close(fd_to);

	/* done with "fd_from" */
	close(fd_from);

	return error;
}

int bs_replace_comments(int fd_from, int fd_to, FILE *errlog)
{
	const size_t bufsize = sizeof(size_t) + 1;
	char buf[sizeof(size_t) + 1];
	memset(buf, 0x00, bufsize);

	int have_slash = 0;
	int have_double_slash = 0;
	int have_slash_star = 0;
	int have_slash_star_star = 0;
	int skip = 0;
	int error = 0;

	while (1) {
		size_t read_size = 1;
		ssize_t bytes = read(fd_from, buf, read_size);
		if (bytes < 0) {
			const char *fmt = "read(fd, buf, %zu) returned %zd";
			int save_errno =
			    Bs_log_errno(errlog, fmt, read_size, bytes);
			error = save_errno ? save_errno : 1;
			goto bs_replace_comments_end;
		}
		if (!bytes) {
			if (have_slash) {
				buf[0] = '/';
				buf[1] = '\0';
				write(fd_to, buf, 1);
			}
			goto bs_replace_comments_end;
		}

		char c = buf[0];
		if (have_slash) {
			char d;
			if (c == '/') {
				have_double_slash = 1;
				d = ' ';
			} else if (c == '*') {
				have_slash_star = 1;
				d = ' ';
			} else {
				d = '/';
			}
			buf[0] = d;
			buf[1] = '\0';
			write(fd_to, buf, 1);
			have_slash = 0;
		}
		if (have_double_slash) {
			if (c == '\n') {
				have_double_slash = 0;
			}
		} else if (have_slash_star_star) {
			if (c == '/') {
				have_slash_star_star = 0;
				skip = 1;
			}
		} else if (have_slash_star) {
			if (c == '*') {
				have_slash_star = 0;
				have_slash_star_star = 1;
			}
		} else if (c == '/') {
			have_slash = 1;
		}
		if (!(skip || have_slash || have_double_slash
		      || have_slash_star_star || have_slash_star)) {
			buf[0] = c;
			buf[1] = '\0';
			write(fd_to, buf, 1);
		}
		skip = 0;
	}

bs_replace_comments_end:
	/* done with "out" end of pipe */
	close(fd_to);

	/* done with "fd_from" */
	close(fd_from);

	return error;
}

int main(int argc, char **argv)
{
	const char *in = argc > 1 ? argv[1] : NULL;
	const char *out = argc > 2 ? argv[2] : NULL;
	if (!in || !out) {
		fprintf(stderr, "usage %s /path/to/in /path/to/out\n", argv[0]);
		return 1;
	}

	bs_pipe_function transforms[3] = {
		bs_strip_backslash_newline,
		bs_replace_comments,
		NULL
	};

	int err = bs_pipe_paths(transforms, in, out, stderr);

	/* if err is a 1-byte value, we can return it raw,
	 * to avoid a value like 0x0100 looking like SUCCESS
	 * return EXIT_FAILURE with values larger than 1 byte */
	return ((err & 0xFF) == err) ? err : EXIT_FAILURE;
}
