# AREDN RFeye

AREDN RFeye is an AREDN-native RF spectrum visibility tool for ath10k-based 802.11ac nodes. The goal is to provide an AirView-like experience that fits inside an AREDN node, uses AREDN app conventions, and relies on OpenWrt `PACKAGE_ATH_SPECTRAL` where available.

## Current status

This scaffold now covers:

- Milestone 1 support probe (`rfeye-probe`) for debugfs/spectral/survey checks.
- Milestone 2 parser starter (`rfeye-spectral-parse`) for ath10k TLV samples.
- Admin CGI snapshot endpoint wired to parser output.
- First `rfeye-agent` skeleton with strict capture time/memory limits.

Architecture baseline (node + Linux split):

- `docs/ARCHITECTURE.md`

## Goals

- Fit inside an AREDN firmware image as a small OpenWrt package.
- Appear in AREDN Apps menu under `/www/cgi-bin/apps/rfeye/...`.
- Avoid continuous flash writes (keep high-rate data in RAM/tmpfs).
- Expose live FFT/waterfall/utilization views in lightweight UI.

## Probe script

Run on an AREDN/OpenWrt node:

```sh
sh /usr/lib/rfeye/rfeye-probe.sh
```

Optional short sample capture from `spectral_scan0`:

```sh
sh /usr/lib/rfeye/rfeye-probe.sh --capture-bytes 4096 --capture-dir /tmp
```

The probe reports:

- debugfs availability
- phy list
- ath10k spectral file presence
- `iw dev <iface> survey dump` field availability (`active`, `busy`, `tx`, `rx`, `noise`)

## Parser (Milestone 2)

Parser source:

- `src/rfeye_spectral_parse.c`

Build locally:

```sh
mkdir -p build
cc -O2 -Wall -Wextra -o build/rfeye-spectral-parse src/rfeye_spectral_parse.c
```

Parse a captured ath10k sample stream:

```sh
./build/rfeye-spectral-parse --input /tmp/rfeye-sample-phy0.bin --phy phy0 --limit 10 --bins 64
```

Example output (one compact JSON frame per line):

```json
{"phy":"phy0","freq1_mhz":5745,"freq2_mhz":0,"width_mhz":80,"noise":-96,"rssi":42,"max_index":115,"max_magnitude":900,"tsf":123456789,"bins":[12,13,15,18]}
```

Fixture + replay helpers:

```sh
python3 scripts/make-test-fixture.py --out fixtures/sample-ath10k.bin --bins 32
scripts/rfeye-replay.sh fixtures/sample-ath10k.bin --phy phy0
```

## rfeye-agent skeleton (status/start/stop/snapshot)

Node command:

- `/usr/sbin/rfeye-agent status`
- `/usr/sbin/rfeye-agent start [seconds bins phy]`
- `/usr/sbin/rfeye-agent stop`
- `/usr/sbin/rfeye-agent snapshot`

Safety/limits enforced by config (`/etc/config/rfeye`):

- `max_runtime_seconds` (hard cap for captures)
- `max_capture_bytes` (hard cap for capture memory)
- `snapshot_bytes` (hard cap for snapshot reads)

JSON CGI bridge endpoint:

- `/www/cgi-bin/apps/rfeye/data/agent.sh?action=status|start|stop|snapshot`

## Admin CGI snapshot endpoint

Installed path:

- `/www/cgi-bin/apps/rfeye/admin/snapshot.sh`

It captures a short sample from `spectral_scan0`, parses one frame with
`/usr/lib/rfeye/rfeye-spectral-parse`, and returns:

```json
{"ok":true,"frame":{...}}
```

Query args:

- `phy` (default `phy0`)
- `bins` (default `64`)

Example:

```sh
curl "http://<node>/cgi-bin/apps/rfeye/admin/snapshot.sh?phy=phy0&bins=64"
```
