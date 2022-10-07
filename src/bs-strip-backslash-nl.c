/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2022 Eric Herman <eric@freesa.org> */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/wait.h>

int strip_backslash_newline(int fd_from, int fd_to, FILE *errlog)
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
			int save_errno = errno;
			const char *errstr = strerror(save_errno);
			fprintf(errlog, "%s:%d: ", __FILE__, __LINE__);
			fprintf(errlog, "read(fd, buf, %zu)"
				" returned %zd, errno %d, %s\n",
				read_size, bytes, save_errno, errstr);
			error = 1;
			goto strip_backslash_newline_end;
		}
		if (!bytes) {
			if (have_backslash) {
				buf[0] = '\\';
				buf[1] = '\0';
				write(fd_to, buf, 1);
			}
			goto strip_backslash_newline_end;
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

strip_backslash_newline_end:
	/* done with "out" end of pipe */
	close(fd_to);

	/* done with "fd_from" */
	close(fd_from);

	return error;
}

int fd_read_write(int fd_from, int fd_to, char *buf, size_t bufsize,
		  FILE *errlog)
{
	ssize_t bytes = 0;
	while ((bytes = read(fd_from, buf, bufsize)) != 0) {
		if (bytes < 0) {
			int save_errno = errno;
			const char *errstr = strerror(save_errno);
			fprintf(errlog, "%s:%d: ", __FILE__, __LINE__);
			fprintf(errlog,
				"read returned %zd, errno %d, %s\n",
				bytes, save_errno, errstr);
			return 1;
		}
		write(fd_to, buf, bytes);
	}
	close(fd_from);
	return 0;
}

int strip_backslash_nl_pipe(int fdin, int fdout, FILE *errlog)
{
	int pipefd[2];
	pid_t child_pid;
	pipe(pipefd);

	int piperead = pipefd[0];
	int pipewrite = pipefd[1];

	if ((child_pid = fork()) == -1) {
		fprintf(errlog, "%s:%d: ", __FILE__, __LINE__);
		perror("fork");
		return 1;
	}

	if (child_pid == 0) {
		/* not using the "out" end of pipe */
		close(piperead);

		/* not using the fdout */
		close(fdout);

		return strip_backslash_newline(fdin, pipewrite, errlog);
	}

	/* not using fdin */
	close(fdin);

	/* not using input end of pipe */
	close(pipewrite);

	const size_t bufsize = 80;
	char buf[80];
	int err2 = fd_read_write(piperead, fdout, buf, bufsize, errlog);

	int options = 0;
	int err1 = 0;
	waitpid(child_pid, &err1, options);
	return (err1 || err2) ? 1 : 0;
}

int main(int argc, char **argv)
{
	const char *in_path = argc > 1 ? argv[1] : NULL;
	const char *out_path = argc > 2 ? argv[2] : NULL;
	if (!in_path || !out_path) {
		fprintf(stderr, "usage %s /path/to/in.c /path/to/out.c.i\n",
			argv[0]);
		return 1;
	}

	int fdin = open(in_path, O_RDONLY);
	if (fdin < 0) {
		int save_errno = errno;
		const char *errstr = strerror(save_errno);
		fprintf(stderr, "%s:%d: ", __FILE__, __LINE__);
		fprintf(stderr,
			"open(\"%s\", O_RDONLY)"
			" returned %d, errno %d: %s\n",
			in_path, fdin, save_errno, errstr);
		return 1;
	}

	mode_t mode = 0664;
	int fdout = open(out_path, O_CREAT | O_WRONLY | O_TRUNC, mode);
	if (fdin < 0) {
		int save_errno = errno;
		const char *errstr = strerror(save_errno);
		fprintf(stderr, "%s:%d: ", __FILE__, __LINE__);
		fprintf(stderr,
			"open(\"%s\", O_CREAT|O_WRONLY|O_TRUNC)"
			"returned %d, errno %d: %s\n",
			out_path, fdout, save_errno, errstr);
		return 1;
	}

	int err = strip_backslash_nl_pipe(fdin, fdout, stderr);

	/* done with fdout */
	close(fdout);

	return !!err;
}
