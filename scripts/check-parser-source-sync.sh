#!/bin/sh
set -eu

diff -u src/rfeye_spectral_parse.c package/aredn-rfeye/src/rfeye_spectral_parse.c

echo "Parser source sync check passed"
