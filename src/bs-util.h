/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2022 Eric Herman <eric@freesa.org> */

#ifndef BS_UTIL_H
#define BS_UTIL_H 1

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
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

extern void *(*bs_malloc)(size_t size);
extern void (*bs_free)(void *p);

extern FILE *(*bs_fopen)(const char *restrict path, const char *restrict mode);
extern int (*bs_fclose)(FILE *stream);

/******************/
/* pipe functions */
/******************/
typedef int (*bs_pipe_function)(int fd_from, int fd_to, FILE *errlog);

struct pipe_func_s {
	bs_pipe_function pfunc;
	const char *name;
};

int bs_pipe_paths(struct pipe_func_s *funcs, const char *in_path,
		  const char *out_path, FILE *errlog);

int bs_pipes(struct pipe_func_s *funcs, int fdin, int fdout, FILE *errlog);

/******************/
/* file functions */
/******************/
int bs_fd_copy(int fd_from, int fd_to, char *buf, size_t bufsize, FILE *errlog);

int bs_open_ro(const char *path, int *err, FILE *log,
	       const char *file, int line);
#define Bs_open_ro(path, err, log) \
	bs_open_ro(path, err, log, __FILE__, __LINE__)

int bs_open_rw(const char *path, mode_t mode, int *err, FILE *log,
	       const char *file, int line);
#define Bs_open_rw(path, mode, err, log) \
	bs_open_rw(path, mode, err, log, __FILE__, __LINE__)

int bs_close_fd(int fd, const char *name, FILE *log, const char *file,
		int line);
#define Bs_close_fd(fd, name, log) \
	bs_close_fd(fd, name, log, __FILE__, __LINE__);

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

/* returns 0 (SUCCESS) or a 1-byte non-zero value if err is non-zero */
int exit_val(int err);

#endif /* BS_UTIL_H */
