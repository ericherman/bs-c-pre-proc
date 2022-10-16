/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2022 Eric Herman <eric@freesa.org> */

#include <fcntl.h>
#include <stdlib.h>
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

void *(*bs_malloc)(size_t size) = malloc;
void (*bs_free)(void *p) = free;

FILE *(*bs_fopen)(const char *restrict path, const char *restrict mode) = fopen;
int (*bs_fclose)(FILE *stream) = fclose;

void (*bs_exit)(int status) = exit;

int bs_fd_copy(int fd_from, int fd_to, char *buf, size_t bufsize, FILE *errlog)
{
	int err = 0;
	ssize_t bytes = 0;
	while ((bytes = bs_read(fd_from, buf, bufsize)) != 0) {
		if (bytes < 0) {
			const char *fmt = "read(%d, buf, %zu) returned %zd";
			int save_errno =
			    Bs_log_errno(errlog, fmt, fd_from, bufsize, bytes);
			err = save_errno ? save_errno : 1;
			goto bs_fd_copy_end;
		}
		bs_write(fd_to, buf, bytes);
	}

bs_fd_copy_end:
	return err;
}

int bs_pipes(struct pipe_func_s *funcs, int fdin, int fdout, FILE *errlog)
{
	pid_t child_pid = 0;
	int piperead;
	int pipewrite;
	int incoming = fdin;
	char name[80];
	memset(name, 0x00, 80);

	for (size_t i = 0; funcs[i].pfunc; ++i) {

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
			pid_t my_pid = getpid();

			/* not using "fdout" */
			snprintf(name, 80,
				 "child func[%zu] %s (pid: %zd) fdout",
				 i, funcs[i].name, (ssize_t)my_pid);
			Bs_close_fd(fdout, name, errlog);

			snprintf(name, 80,
				 "child func[%zu] %s (pid: %zd) piperead", i,
				 funcs[i].name, (ssize_t)my_pid);
			/* not using the "out" end of pipe */
			Bs_close_fd(piperead, name, errlog);

			/* make my funk the p-funk, I want my funk uncut */
			bs_pipe_function myfunc = funcs[i].pfunc;
			int childerr = myfunc(incoming, pipewrite, errlog);
			bs_exit(exit_val(childerr));
		}

		/* not using incoming */
		snprintf(name, 80, "pfunc[%zu] (child_pid: %zd) incoming",
			 i, (ssize_t)child_pid);
		Bs_close_fd(incoming, name, errlog);

		/* not using input end of pipe */
		snprintf(name, 80, "pfunc[%zu] (child_pid: %zd) piperwite",
			 i, (ssize_t)child_pid);
		Bs_close_fd(pipewrite, name, errlog);

		incoming = piperead;
	}

	const size_t bufsize = 80;
	char buf[80];
	int err2 = bs_fd_copy(incoming, fdout, buf, bufsize, errlog);
	snprintf(name, 80, "parent finish incoming");
	Bs_close_fd(incoming, name, errlog);

	// snprintf(name, 80, "parent finish fdout");
	// Bs_close_fd(fdout, name, errlog);

	int options = 0;
	int err1 = 0;
	waitpid(child_pid, &err1, options);

	return (err1 > err2) ? err1 : err2;
}

int bs_open_ro(const char *path, int *err, FILE *log,
	       const char *file, int line)
{
	int fdin = bs_open(path, O_RDONLY);
	// fprintf(stderr, "bs_open_ro %s %d: %s == fd: %d\n", file, line, path,
	//      fdin);
	if (fdin < 0) {
		int save_errno = errno;
		bs_log_error(save_errno, file, line, log,
			     "open(\"%s\", O_RDONLY) returned %d", path, fdin);
		*err = save_errno ? save_errno : 1;
	}
	return fdin;
}

int bs_open_rw(const char *path, mode_t mode, int *err, FILE *log,
	       const char *file, int line)
{
	if (!mode) {
		mode = 0600;
	}
	int fdout = bs_open(path, O_CREAT | O_WRONLY | O_TRUNC, mode);
	// fprintf(stderr, "bs_open_rw %s %d: %s == fd: %d\n", file, line, path,
	//      fdout);
	if (fdout < 0) {
		int save_errno = errno;
		const char *fmt =
		    "open(\"%s\", O_CREAT|O_WRONLY|O_TRUNC) returned %d";
		bs_log_error(save_errno, file, line, log, fmt, path, fdout);
		*err = save_errno ? save_errno : 1;
	}
	return fdout;
}

int bs_close_fd(int fd, const char *name, FILE *log, const char *file, int line)
{
	// fprintf(stderr, "bs_close_fd(%d) (name: %s)\n", fd, name);
	int err = bs_close(fd);
	if (err) {
		int save_errno = errno;
		bs_log_error(save_errno, file, line, log,
			     "close(%d) (name: %s) returned %d", fd, name, err);
	}
	return err;
}

int bs_pipe_paths(struct pipe_func_s *funcs, const char *in_path,
		  const char *out_path, FILE *errlog)
{
	int err = 0;
	int fdin = Bs_open_ro(in_path, &err, errlog);
	if (fdin < 0) {
		return err;
	}

	mode_t mode = 0664;
	int fdout = Bs_open_rw(out_path, mode, &err, errlog);
	if (fdin < 0) {
		Bs_close_fd(fdin, in_path, errlog);
		return err;
	}

	err = bs_pipes(funcs, fdin, fdout, errlog);

	Bs_close_fd(fdout, out_path, errlog);
	Bs_close_fd(fdin, in_path, errlog);

	return err;
}

int bs_log_error(int perrno, const char *file, int line, FILE *errlog,
		 const char *format, ...)
{
	fflush(stdout);

	fprintf(errlog, "\n%s:%d: ", file, line);

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

int exit_val(int err)
{
	/* if err is a 1-byte value, we can return it raw,
	 * to avoid a value like 0x0100 looking like SUCCESS
	 * return EXIT_FAILURE with values larger than 1 byte */
	return ((err & 0xFF) == err) ? err : EXIT_FAILURE;
}
