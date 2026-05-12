# Build and Node Test Instructions

This document describes the first practical test path for AREDN RFeye on a Linux build host and a bench AREDN node.

## 1. Local parser smoke test

From the repository root:

```sh
scripts/test-parser-smoke.sh
```

Expected result:

```text
RFeye parser smoke test passed
```

This only validates the synthetic TLV fixture and parser. It does not prove ath10k spectral support on a node.

## 2. Copy package into an AREDN/OpenWrt build tree

Example paths:

```sh
RFeye=$HOME/src/AREDN-RFeye
AREDN=$HOME/src/aredn

rsync -av --delete "$RFeye/package/aredn-rfeye/" \
  "$AREDN/package/aredn-rfeye/"
```

## 3. Compile package

```sh
cd "$AREDN"
make package/aredn-rfeye/clean V=s
make package/aredn-rfeye/compile V=s
```

Find the package:

```sh
find bin -name 'aredn-rfeye*.ipk' -print
```

If the AREDN category is not available in the build menu, temporarily set the package category to Network while debugging package mechanics.

## 4. Install on a bench node

Use a bench node first. Do not use a production mesh node for first tests.

Copy package:

```sh
scp bin/packages/*/*/aredn-rfeye_*.ipk root@localnode.local.mesh:/tmp/
```

Install:

```sh
ssh root@localnode.local.mesh
opkg install /tmp/aredn-rfeye_*.ipk
```

## 5. Manual node checks before enabling service

```sh
/usr/sbin/rfeye-agent status
/usr/sbin/rfeye-survey survey
/usr/sbin/rfeye-survey utilization
```

Expected:

- JSON output from each command.
- Clear error JSON if unsupported.
- No shell tracebacks or HTML error pages.

## 6. Snapshot and capture tests

```sh
/usr/sbin/rfeye-agent snapshot
/usr/sbin/rfeye-agent start 5 128 phy0
sleep 6
ls -lh /tmp/rfeye/latest.tlv
/usr/lib/rfeye/rfeye-spectral-parse \
  --input /tmp/rfeye/latest.tlv \
  --phy phy0 \
  --limit 5 \
  --bins 64
/usr/sbin/rfeye-agent stop
```

Acceptance:

- `latest.tlv` is created in `/tmp/rfeye`.
- Parser emits JSON frames, or a clear no-frame result is observed.
- Wi-Fi remains stable.
- Scan stops cleanly.

## 7. Enable service only after manual tests

```sh
uci set rfeye.main.enabled='1'
uci commit rfeye
/etc/init.d/rfeye enable
/etc/init.d/rfeye start
```

Stop/disable:

```sh
/etc/init.d/rfeye stop
/etc/init.d/rfeye disable
uci set rfeye.main.enabled='0'
uci commit rfeye
```

## 8. CGI tests

From a Linux workstation or browser:

```sh
curl 'http://localnode.local.mesh/cgi-bin/apps/rfeye/data/agent.sh?action=status'
curl 'http://localnode.local.mesh/cgi-bin/apps/rfeye/data/agent.sh?action=survey'
curl 'http://localnode.local.mesh/cgi-bin/apps/rfeye/data/agent.sh?action=utilization'
curl 'http://localnode.local.mesh/cgi-bin/apps/rfeye/data/agent.sh?action=snapshot'
```

Expected:

- `Content-Type: application/json`
- Valid JSON body
- No crash if spectral files are missing

## 9. What to record

Use `docs/NODE_TEST_REPORT_TEMPLATE.md` for each test node.

Important notes:

- Node model and chipset
- AREDN version
- whether debugfs is mounted
- whether ath10k spectral files exist
- whether survey counters include active/busy/tx/rx/noise
- whether `spectral_scan0` produces non-empty TLV data
- whether Wi-Fi remains stable during start/snapshot/stop
