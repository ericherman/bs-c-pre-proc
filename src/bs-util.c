/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2022 Eric Herman <eric@freesa.org> */

#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "bs-util.h"

/* global function pointers for tests to intercept */
int (*bs_open)(const char *path, int options, ...) = open;
int (*bs_close)(int fd) = close;

ssize_t (*bs_read)(int fd, void *buf, size_t count) = read;
ssize_t (*bs_write)(int fd, const void *buf, size_t count) = write;

pid_t (*bs_fork)(void) = fork;
int (*bs_pipe)(int pipefd[2]) = pipe;

int bs_fd_copy(int fd_from, int fd_to, char *buf, size_t bufsize, FILE *errlog)
{
	ssize_t bytes = 0;
	while ((bytes = bs_read(fd_from, buf, bufsize)) != 0) {
		if (bytes < 0) {
			const char *fmt = "read(%d, buf, %zu) returned %zd";
			int save_errno =
			    Bs_log_errno(errlog, fmt, fd_from, bufsize, bytes);
			return save_errno ? save_errno : 1;
		}
		bs_write(fd_to, buf, bytes);
	}
	return 0;
}

int bs_pipes(bs_pipe_function *pfunc, int fdin, int fdout, FILE *errlog)
{
	pid_t child_pid = 0;
	int piperead;
	int pipewrite;
	int incoming = fdin;

	for (size_t i = 0; pfunc[i]; ++i) {
		int pipefd[2];
		bs_pipe(pipefd);

		piperead = pipefd[0];
		pipewrite = pipefd[1];

		if ((child_pid = bs_fork()) == -1) {
			const char *fmt = "fork() number %zu returned -1";
			int save_errno = Bs_log_errno(errlog, fmt, i);
			return save_errno ? save_errno : 1;
		}

		if (child_pid == 0) {
			/* not using "fdout" */
			bs_close(fdout);

			/* not using the "out" end of pipe */
			bs_close(piperead);

			/* make my funk the p-funk, I want my funk uncut */
			bs_pipe_function myfunc = pfunc[i];
			return myfunc(incoming, pipewrite, errlog);
		}

		/* not using incoming */
		bs_close(incoming);

		/* not using input end of pipe */
		bs_close(pipewrite);

		incoming = piperead;
	}

	const size_t bufsize = 80;
	char buf[80];
	int err2 = bs_fd_copy(incoming, fdout, buf, bufsize, errlog);
	bs_close(incoming);
	bs_close(fdout);

	int options = 0;
	int err1 = 0;
	waitpid(child_pid, &err1, options);
	return (err1 > err2) ? err1 : err2;
}

int bs_pipe_paths(bs_pipe_function *pfunc, const char *in_path,
		  const char *out_path, FILE *errlog)
{
	int fdin = bs_open(in_path, O_RDONLY);
	if (fdin < 0) {
		const char *fmt = "open(\"%s\", O_RDONLY) returned %d";
		int save_errno = Bs_log_errno(errlog, fmt, in_path, fdin);
		return save_errno ? save_errno : 1;
	}

	mode_t mode = 0664;
	int fdout = bs_open(out_path, O_CREAT | O_WRONLY | O_TRUNC, mode);
	if (fdin < 0) {
		const char *fmt =
		    "open(\"%s\", O_CREAT|O_WRONLY|O_TRUNC) returned %d";
		int save_errno = Bs_log_errno(errlog, fmt, out_path, fdout);
		return save_errno ? save_errno : 1;
	}

	int err = bs_pipes(pfunc, fdin, fdout, errlog);

	/* done with fdout */
	bs_close(fdout);

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
