/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2020-2022 Eric Herman <eric@freesa.org> */

#ifndef BS_CPP_H
#define BS_CPP_H 1

#include <stdio.h>

/* prototypes */
int bs_cpp(int argc, char **argv);
int bs_preprocess(FILE *in, FILE *out, char *buf, size_t bufsize, FILE *log);

/* internals for testing
int bs_include(FILE *out, char *buf, size_t bufsize, size_t ws, FILE *log);

extern FILE *bs_stdin;
extern FILE *bs_stdout;
extern FILE *bs_stderr;
extern FILE *(*bs_fopen)(const char *restrict path, const char *restrict mode);
extern int (*bs_fclose)(FILE *stream);
extern void *(*bs_malloc)(size_t size);
extern void (*bs_free)(void *ptr);
*/

#endif /* BS_CPP */
