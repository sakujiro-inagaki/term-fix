#!/bin/sh
# Compile the C shim for term-fix.
#
# Invoked via fixproj.toml's `preliminary_commands`. Produces
# `c_src/libterm_shim.a` which fixproj.toml links statically.
set -eu

CC="${CC:-cc}"

# shellcheck disable=SC2086
$CC -Wall -Wextra -Werror -O2 -fPIC \
    -c -o c_src/term_shim.o c_src/term_shim.c

ar rcs c_src/libterm_shim.a c_src/term_shim.o
