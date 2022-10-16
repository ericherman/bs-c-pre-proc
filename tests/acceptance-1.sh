#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2022 Eric Herman <eric@freesa.org>

BS_CPP=$1

if [ "_${BS_CPP}_" == "__" ]; then
	BS_CPP=build/bs-cpp
fi

set -e

rm -f foo.h bar.c bar.c.expected bar.c.i

cat << EOF > foo.h
int foo(void);
EOF

cat << EOF > bar.c
#include "foo.h"
int main(void)
{
	return foo();
}
EOF

cat << EOF > bar.c.expected
int foo(void);

int main(void)
{
	return foo();
}
EOF

$BS_CPP bar.c bar.c.i

diff -uw bar.c.expected bar.c.i

rm -f foo.h bar.c bar.c.expected bar.c.i
