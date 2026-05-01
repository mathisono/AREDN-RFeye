#!/bin/sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PARSER="$ROOT_DIR/build/rfeye-spectral-parse"

if [ ! -x "$PARSER" ]; then
  echo "Parser binary missing: $PARSER" >&2
  echo "Build with: cc -O2 -Wall -Wextra -o $PARSER $ROOT_DIR/src/rfeye_spectral_parse.c" >&2
  exit 1
fi

if [ $# -lt 1 ]; then
  echo "Usage: $0 <sample.bin> [parser args...]" >&2
  exit 2
fi

SAMPLE="$1"
shift

exec "$PARSER" --input "$SAMPLE" "$@"

