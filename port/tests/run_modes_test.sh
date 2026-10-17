#!/bin/sh
set -eu
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
cc -std=c11 -D_POSIX_C_SOURCE=200809L -ffunction-sections -fdata-sections \
    -Iport/src -Iport/include port/tests/modes_test.c \
    port/src/plat_fighters.c port/src/plat_modes.c -Wl,--gc-sections -o "$tmp/modes_test"
"$tmp/modes_test"
python3 port/tests/run_grid_test.py
python3 port/tests/run_native_roster_test.py
python3 port/tests/run_classic_roster_test.py
cc -std=c11 -D_POSIX_C_SOURCE=200809L -Iport/include port/tests/pc_battle_test.c \
    port/src/plat_pc_battle.c -lpthread -o "$tmp/pc_battle_test"
"$tmp/pc_battle_test"
python3 port/tests/run_coop_test.py
python3 port/tests/run_relay_test.py
