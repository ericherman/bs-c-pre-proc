/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2020-2022 Eric Herman <eric@freesa.org> */

#ifndef BS_CPP_H
#define BS_CPP_H 1

#include <stdio.h>

/* prototypes */
int bs_cpp(int argc, char **argv);
int bs_c_pre_proc(int fdin, int fdout, FILE *log);

#endif /* BS_CPP */
