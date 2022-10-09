/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2022 Eric Herman <eric@freesa.org> */

#ifndef BS_UTIL_H
#define BS_UTIL_H 1

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

/***************************************************/
/* global function pointers for tests to intercept */
/***************************************************/
extern int (*bs_open)(const char *path, int options, ...);
extern int (*bs_close)(int fd);

extern ssize_t (*bs_read)(int fd, void *buf, size_t count);
extern ssize_t (*bs_write)(int fd, const void *buf, size_t count);

extern pid_t (*bs_fork)(void);
extern int (*bs_pipe)(int pipefd[2]);

/******************/
/* pipe functions */
/******************/
typedef int (*bs_pipe_function)(int fd_from, int fd_to, FILE *errlog);

int bs_pipe_paths(bs_pipe_function *pfunc, const char *in_path,
		  const char *out_path, FILE *errlog);

int bs_pipes(bs_pipe_function *pfunc, int fdin, int fdout, FILE *errlog);

/******************/
/* file functions */
/******************/
int bs_fd_copy(int fd_from, int fd_to, char *buf, size_t bufsize, FILE *errlog);

/*****************/
/* error logging */
/*****************/
int bs_log_error(int perrno, const char *file, int line, FILE *errlog,
		 const char *format, ...);

#define Bs_log_errno(errlog, format, ...) \
	bs_log_error(errno, __FILE__, __LINE__, \
		     errlog, format __VA_OPT__(,) __VA_ARGS__)

#define Bs_log_error(errlog, format, ...) \
	bs_log_error(0, __FILE__, __LINE__, \
		     errlog, format __VA_OPT__(,) __VA_ARGS__)

#endif /* BS_UTIL_H */
