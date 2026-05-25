#!/bin/sh
set -eu

FIXTURE_DIR="fixtures/hardware/KJ6DZB_WSB_ACDISH5_R6"
FIXTURE="$FIXTURE_DIR/raw-test.tlv"
PARSER="${PARSER:-/tmp/rfeye-spectral-parse}"

if [ ! -f "$FIXTURE" ] && [ -f "$FIXTURE.gz" ]; then
  FIXTURE="/tmp/rfeye-raw-test.tlv"
  gzip -dc "$FIXTURE_DIR/raw-test.tlv.gz" >"$FIXTURE"
fi

if [ ! -f "$FIXTURE" ]; then
  echo "fixture not found: $FIXTURE_DIR/raw-test.tlv(.gz)" >&2
  exit 1
fi

python3 scripts/analyze-spectral-raw.py "$FIXTURE" >/tmp/rfeye-fixture-analyze.json
python3 - <<'PY'
import json
j=json.load(open('/tmp/rfeye-fixture-analyze.json'))
print(j['best_guess']['variant'])
print(j['best_guess']['candidate_count'])
print(j['best_guess']['plausible_record_count'])
PY

"$PARSER" --probe --input "$FIXTURE" >/tmp/rfeye-fixture-probe.json
python3 - <<'PY'
import json
j=json.load(open('/tmp/rfeye-fixture-probe.json'))
print(j['best_guess'])
print(j['variants']['ff93_fixed_stride']['candidate_count'])
print(j['variants']['ff93_fixed_stride']['plausible_record_count'])
PY

"$PARSER" --resync --stats --input "$FIXTURE" --phy phy0 --limit 4 --bins 64 >/tmp/rfeye-fixture-resync.json 2>/tmp/rfeye-fixture-resync-stats.json || true
python3 - <<'PY'
import json, sys
j=json.load(open('/tmp/rfeye-fixture-resync-stats.json'))
print(j)
if j.get('frames_emitted', 0) <= 0:
    sys.exit('expected frames_emitted > 0')
PY
