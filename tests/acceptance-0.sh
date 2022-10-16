#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2022 Eric Herman <eric@freesa.org>

BS_EXE="$@"

if [ "_${BS_EXE}_" == "__" ]; then
	BS_EXE=build/bs-cpp
fi

set -e

BS_EXE_IN=test-acceptance-0-backslash-newlines.c
BS_EXE_OUT=${BS_EXE_IN}.i
BS_EXE_EXPECT=${BS_EXE_OUT}.execpt

rm -f $BS_EXE_IN $BS_EXE_OUT $BS_EXE_EXPECT

cat << EOF > $BS_EXE_IN
/*
*/#/* */defi\
ne FORTY_\
TWO 4\
2
int // foo
main(void)
{
	return FORTY_TWO;
}
EOF

cat << EOF > $BS_EXE_EXPECT
 # define FORTY_TWO 42
int  
main(void)
{
	return FORTY_TWO;
}
EOF

$BS_EXE $BS_EXE_IN $BS_EXE_OUT

diff -u $BS_EXE_EXPECT $BS_EXE_OUT

rm -f $BS_EXE_IN $BS_EXE_OUT $BS_EXE_EXPECT
