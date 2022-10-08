/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2022 Eric Herman <eric@freesa.org> */

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>

/* prototypes */
typedef int (*bs_pipe_function)(int fd_from, int fd_to, FILE *errlog);

int bs_log_error(int perrno, const char *file, int line, FILE *errlog,
		 const char *format, ...);
#define Bs_log_errno(errlog, format, ...) \
	bs_log_error(errno, __FILE__, __LINE__, \
		     errlog, format __VA_OPT__(,) __VA_ARGS__)

int bs_pipe_paths(bs_pipe_function * pfunc, const char *in_path,
		  const char *out_path, FILE *errlog);
int bs_pipe(bs_pipe_function * pfunc, int fdin, int fdout, FILE *errlog);
int bs_fd_copy(int fd_from, int fd_to, char *buf, size_t bufsize, FILE *errlog);

/* definitions */
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
		if (!(have_slash
		      || have_double_slash
		      || have_slash_star_star || have_slash_star || skip)
		    ) {
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

/* utility functions */
int bs_fd_copy(int fd_from, int fd_to, char *buf, size_t bufsize, FILE *errlog)
{
	ssize_t bytes = 0;
	while ((bytes = read(fd_from, buf, bufsize)) != 0) {
		if (bytes < 0) {
			const char *fmt = "read(%d, buf, %zu) returned %zd";
			int save_errno =
			    Bs_log_errno(errlog, fmt, fd_from, bufsize, bytes);
			return save_errno ? save_errno : 1;
		}
		write(fd_to, buf, bytes);
	}
	close(fd_from);
	return 0;
}

int bs_pipe(bs_pipe_function * pfunc, int fdin, int fdout, FILE *errlog)
{
	pid_t child_pid = 0;
	int piperead;
	int pipewrite;
	int incoming = fdin;

	for (size_t i = 0; pfunc[i]; ++i) {
		int pipefd[2];
		pipe(pipefd);

		piperead = pipefd[0];
		pipewrite = pipefd[1];

		if ((child_pid = fork()) == -1) {
			const char *fmt = "fork() number %zu returned -1";
			int save_errno = Bs_log_errno(errlog, fmt, i);
			return save_errno ? save_errno : 1;
		}

		if (child_pid == 0) {
			/* not using the "out" end of pipe */
			close(piperead);

			/* not using "fdout" */
			close(fdout);

			return pfunc[i] (incoming, pipewrite, errlog);
		}

		/* not using incoming */
		close(incoming);

		/* not using input end of pipe */
		close(pipewrite);

		incoming = piperead;
	}

	const size_t bufsize = 80;
	char buf[80];
	int err2 = bs_fd_copy(incoming, fdout, buf, bufsize, errlog);

	int options = 0;
	int err1 = 0;
	waitpid(child_pid, &err1, options);
	return (err1 > err2) ? err1 : err2;
}

int bs_pipe_paths(bs_pipe_function * pfunc, const char *in_path,
		  const char *out_path, FILE *errlog)
{
	int fdin = open(in_path, O_RDONLY);
	if (fdin < 0) {
		const char *fmt = "open(\"%s\", O_RDONLY) returned %d";
		int save_errno = Bs_log_errno(errlog, fmt, in_path, fdin);
		return save_errno ? save_errno : 1;
	}

	mode_t mode = 0664;
	int fdout = open(out_path, O_CREAT | O_WRONLY | O_TRUNC, mode);
	if (fdin < 0) {
		const char *fmt =
		    "open(\"%s\", O_CREAT|O_WRONLY|O_TRUNC) returned %d";
		int save_errno = Bs_log_errno(errlog, fmt, out_path, fdout);
		return save_errno ? save_errno : 1;
	}

	int err = bs_pipe(pfunc, fdin, fdout, errlog);

	/* done with fdout */
	close(fdout);

	return err;
}

int bs_log_error(int perrno, const char *file, int line, FILE *errlog,
		 const char *format, ...)
{
	fflush(stdout);

	fprintf(errlog, "%s:%d: ", file, line);

	va_list args;
	va_start(args, format);
	vfprintf(errlog, format, args);
	va_end(args);

	if (perrno) {
		const char *errstr = strerror(perrno);
		fprintf(errlog, " (errno: %d, %s)", perrno, errstr);
	}

	fprintf(errlog, "\n");

	return perrno;
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
