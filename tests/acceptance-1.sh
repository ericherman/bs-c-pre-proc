#!/bin/bash

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

diff -u bar.c.i bar.c.expected

rm -f foo.h bar.c bar.c.expected bar.c.i
