/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2022 Eric Herman <eric@freesa.org> */

#include <limits.h>

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bs-util.h"

int bs_c_pre_proc(int fdin, int fdout, FILE *log);
char *bs_name_from_include(char *buf, char start_delim, char until_delim,
			   char **name_end, FILE *log);

int bs_strip_backslash_newline(int fd_from, int fd_to, FILE *log)
{
	const size_t bufsize = sizeof(size_t) + 1;
	char buf[sizeof(size_t) + 1];
	memset(buf, 0x00, bufsize);

	int have_backslash = 0;
	int error = 0;

	while (1) {
		int skip = 0;
		size_t read_size = 1;
		ssize_t bytes = bs_read(fd_from, buf, read_size);
		if (bytes < 0) {
			const char *fmt = "read(fd, buf, %zu) returned %zd";
			int save_errno =
			    Bs_log_errno(log, fmt, read_size, bytes);
			error = save_errno ? save_errno : 1;
			goto bs_strip_backslash_newline_end;
		}
		if (!bytes) {
			if (have_backslash) {
				buf[0] = '\\';
				buf[1] = '\0';
				bs_write(fd_to, buf, 1);
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
				bs_write(fd_to, buf, 1);
				have_backslash = 0;
				skip = 0;
			}
		}
		if (c == '\\') {
			have_backslash = 1;
		} else if (!skip) {
			buf[0] = c;
			buf[1] = '\0';
			bs_write(fd_to, buf, 1);
		}
	}

bs_strip_backslash_newline_end:
	/* done with "out" end of pipe */
	// Bs_close_fd(fd_to, "strip-backlash-out", log);

	/* done with "fd_from" */
	Bs_close_fd(fd_from, "strip-backslash-from", log);

	return error;
}

int bs_replace_comments(int fd_from, int fd_to, FILE *log)
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
		ssize_t bytes = bs_read(fd_from, buf, read_size);
		if (bytes < 0) {
			const char *fmt = "read(fd, buf, %zu) returned %zd";
			int save_errno =
			    Bs_log_errno(log, fmt, read_size, bytes);
			error = save_errno ? save_errno : 1;
			goto bs_replace_comments_end;
		}
		if (!bytes) {
			if (have_slash) {
				buf[0] = '/';
				buf[1] = '\0';
				bs_write(fd_to, buf, 1);
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
			bs_write(fd_to, buf, 1);
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
			bs_write(fd_to, buf, 1);
		}
		skip = 0;
	}

bs_replace_comments_end:
	/* done with "out" end of pipe */
	// Bs_close_fd(fd_to, "replace-comments-out", log);

	/* done with "fd_from" */
	Bs_close_fd(fd_from, "replace-comments-in", log);

	return error;
}

int bs_include(int fdout, char *buf, size_t bufsize, size_t offset, FILE *log);

int bs_replace_directives(int fd_from, int fd_to, FILE *log)
{
	const size_t bufsize = sizeof(size_t) + 1;
	char buf[sizeof(size_t) + 1];
	memset(buf, 0x00, bufsize);

	int error = 0;
	int may_be_pre_proc_line = 1;
	int is_preproc = 0;

	const size_t longest_line_we_tollerate = 1000 + (2 * PATH_MAX);
	const size_t directive_size = longest_line_we_tollerate;
	char *directive = bs_malloc(directive_size);
	if (!directive) {
		const char *fmt = "malloc(%zu) failed";
		int save_errno = Bs_log_errno(log, fmt, directive_size);
		error = save_errno ? save_errno : 1;
		return error;
	}
	memset(directive, 0x00, directive_size);
	size_t pos = 0;

	while (1) {
		size_t read_size = 1;
		ssize_t bytes = bs_read(fd_from, buf, read_size);
		if (bytes < 0) {
			const char *fmt = "read(fd, buf, %zu) returned %zd";
			int save_errno =
			    Bs_log_errno(log, fmt, read_size, bytes);
			error = save_errno ? save_errno : 1;
			return error;
		}
		if (!bytes) {
			goto bs_replace_directives_end;
		}
		char c = buf[0];
		if (is_preproc) {
			if (c == '\n') {
				size_t offset = 8;	// TODO

				if (strncmp("include", directive, 7) == 0) {
					// fprintf(stderr,
					//      "calling bs_include with '%s', (offset: %zu)\n",
					//      directive, offset);

					error = bs_include(fd_to, directive,
							   directive_size,
							   offset, log);
					if (error) {
						Bs_log_error(log,
							     "bs_include error: %d from '%s', offset %zu\n",
							     error, directive,
							     offset);
						goto bs_replace_directives_end;
					}
				} else {
					bs_write(fd_to, "#", 1);
					bs_write(fd_to, directive, pos);
				}
				bs_write(fd_to, "\n", 1);
				is_preproc = 0;
				may_be_pre_proc_line = 1;
				memset(directive, 0x00, directive_size);
				pos = 0;
			} else {
				if (pos >= directive_size) {
					error = 1;
					goto bs_replace_directives_end;
				}
				if (c == ' ' || c == '\t') {
					if (!pos || directive[pos] != ' ') {
						directive[pos++] = c;
					}

				} else {
					directive[pos++] = c;
				}
			}
		} else if (!may_be_pre_proc_line) {
			bs_write(fd_to, buf, 1);
			if (c == '\n') {
				may_be_pre_proc_line = 1;
			}
		} else if ((c != ' ') && (c != '\t') && (c != '#')) {
			bs_write(fd_to, buf, 1);
			may_be_pre_proc_line = 0;
		} else if (c == '#') {
			is_preproc = 1;
			memset(directive, 0x00, directive_size);
			pos = 0;
		} else {
			bs_write(fd_to, buf, 1);
		}
	}

	// TODO deal with dangling preproc line without EOL

bs_replace_directives_end:
	bs_free(directive);
	/* done with "out" end of pipe */
	// Bs_close_fd(fd_to, "replace-directives-out", log);

	/* done with "fd_from" */
	Bs_close_fd(fd_from, "replace-directives-from", log);

	return error;
}

int bs_include(int fdout, char *buf, size_t bufsize, size_t offset, FILE *log)
{
	// fprintf(stderr, "bs_include: %d, '%s'\n", fdout, buf);
	assert(offset < bufsize);
	(void)bufsize;

	char *name, *name_end;
	char delim1 = '"';
	char delim2 = '"';
	char *from = buf + offset;
	name = bs_name_from_include(from, delim1, delim2, &name_end, log);
	if (!name) {
		Bs_log_errno(log, "no name from '%s'?\n", from);
		return 1;
	}
	// we will not process anything more on this line
	// NULL-terminate the string, so we can use it for the
	// path for fopen.
	*name_end = '\0';
	// fprintf(stderr, "name: '%s'\n", name);

	int err = 0;
	int fdinclude = Bs_open_ro(name, &err, log);
	if (fdinclude < 0) {
		return err;
	}

	err = bs_c_pre_proc(fdinclude, fdout, log);
	// Bs_close_fd(fdinclude, name, log);

	return err;
}

char *bs_name_from_include(char *buf, char start_delim, char until_delim,
			   char **name_end, FILE *log)
{
	assert(name_end);

	char *name = strchr(buf, start_delim);
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

int bs_c_pre_proc(int fdin, int fdout, FILE *log)
{
	struct pipe_func_s transforms[] = {
		{ bs_strip_backslash_newline, "bs_strip_backslash_newline" },
		{ bs_replace_comments, "bs_replace_comments" },
		{ bs_replace_directives, "bs_replace_directives" },
		{ NULL, NULL }
	};
	int error = bs_pipes(transforms, fdin, fdout, log);
	return error;
}

int main(int argc, char **argv)
{
	const char *in_path = argc > 1 ? argv[1] : NULL;
	const char *out_path = argc > 2 ? argv[2] : NULL;
	if (!in_path || !out_path) {
		fprintf(stderr, "usage %s /path/to/in /path/to/out\n", argv[0]);
		return 1;
	}

	int err = 0;
	int fdin = Bs_open_ro(in_path, &err, stderr);
	if (fdin < 0) {
		return exit_val(err);
	}

	mode_t mode = 0664;
	int fdout = Bs_open_rw(out_path, mode, &err, stderr);
	if (fdout < 0) {
		Bs_close_fd(fdin, in_path, stderr);
		return exit_val(err);
	}

	err = bs_c_pre_proc(fdin, fdout, stderr);

	// Bs_close_fd(fdout, out_path, stderr);
	// Bs_close_fd(fdin, in_path, stderr);

	return exit_val(err);
}
