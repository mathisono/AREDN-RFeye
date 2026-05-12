#!/bin/sh
set -eu

mkdir -p build fixtures

cc -O2 -Wall -Wextra \
  -o build/rfeye-spectral-parse \
  src/rfeye_spectral_parse.c

python3 scripts/make-test-fixture.py \
  --out fixtures/sample-ath10k.bin \
  --bins 32

OUT="$(build/rfeye-spectral-parse \
  --input fixtures/sample-ath10k.bin \
  --phy phy0 \
  --limit 1 \
  --bins 32)"

echo "$OUT"

echo "$OUT" | grep -q '"phy":"phy0"'
echo "$OUT" | grep -q '"freq1_mhz":5745'
echo "$OUT" | grep -q '"bins":'

echo "RFeye parser smoke test passed"
