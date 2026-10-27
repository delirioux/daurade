#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Daurade's host tests: the generated files up to date (the splash, the FM6 patches), the groove engine strict about
# warnings and single precision and its own tests (tests/da_groove_test.c), the screen and its layout lint
# (tests/da_ui_test.sh).
#   sh tests/da_tests.sh        (exit 0: all passed)
set -eu
mkdir -p build/host
python3 tools/da_logo.py --check
python3 tools/da_fm6.py --check
cc -std=c11 -O2 -Wall -Wextra -Wshadow -Wdouble-promotion -Werror -Ifirmware/src -c firmware/src/da_groove.c \
   -o build/host/da_groove.o
cc -std=c11 -O2 -Wall -Ifirmware/src -o build/host/da_groove_test tests/da_groove_test.c firmware/src/da_groove.c -lm
build/host/da_groove_test
sh tests/da_ui_test.sh build/da_ui
echo "da_tests: all passed"
