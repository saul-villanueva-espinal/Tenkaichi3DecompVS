#!/bin/sh
set -eu
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cc -std=c11 -D_POSIX_C_SOURCE=200809L -ffunction-sections -fdata-sections \
    -Iport/src -Iport/include port/tests/modes_test.c \
    port/src/plat_fighters.c port/src/plat_modes.c -Wl,--gc-sections -o "$tmp/modes_test"
"$tmp/modes_test"
