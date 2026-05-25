# AREDN RFeye

AREDN RFeye is an AREDN-native RF spectrum visibility tool for ath10k-based 802.11ac nodes. The goal is to provide an AirView-like test view inside an AREDN node while keeping heavier analysis, replay, and classifiers on a Linux workstation.

## Current status

The node package now includes:

- `rfeye-agent` for safe status/start/stop/snapshot capture control and trusted radio-state reporting.
- `rfeye-survey` for survey counters and utilization estimates.
- `rfeye-spectral-parse` for ath10k TLV-to-JSON FFT frames.
- A lightweight AREDN app GUI at `/cgi-bin/apps/rfeye/user`.
- A CGI JSON bridge at `/cgi-bin/apps/rfeye/data/agent.sh`.
- Parser smoke tests and GitHub Actions smoke workflow.

## Safety rules

- Current-channel/background scan only.
- No automatic channel hopping.
- No continuous flash writes.
- Captures stay in `/tmp`.
- Capture runtime and byte count are capped.
- Unsupported hardware must return clear JSON errors.

## Current field-test limitations

First bench-node testing on `KJ6DZB-WSB-ACdish5` showed the core ath10k spectral path works, but some node/driver counters need defensive handling:

- Survey counters may be zero or unchanged on some ath10k/IBSS nodes; `rfeye-survey` returns clean diagnostic JSON in that case.
- CGI snapshot timing is still under test; failures should return diagnostic JSON rather than a bare `no frame`.
- A 10-second capture succeeded on the first tested node and produced `/tmp/rfeye/latest.tlv`; shorter captures may need polling via `capture_status` before assuming the file exists.

## Radio frequency display

RFeye uses `iw dev <iface> info` radio state as the trusted source for the current channel, frequency, width, and IBSS/SSID. The ath10k FFT frame frequency metadata is kept for debugging, but it is sanity-checked and may be ignored if implausible. Invalid FFT metadata such as `768 MHz` should be displayed only as ignored/debug frame metadata, never as the node operating channel.

## Local parser test

```sh
scripts/test-parser-smoke.sh
```

Expected:

```text
RFeye parser smoke test passed
```

## Build an OpenWrt/AREDN `.ipk`

Copy the package into an AREDN/OpenWrt build tree:

```sh
RFeye=$HOME/src/AREDN-RFeye
AREDN=$HOME/src/aredn

rsync -av --delete "$RFeye/package/aredn-rfeye/" \
  "$AREDN/package/aredn-rfeye/"
```

Build:

```sh
cd "$AREDN"
make package/aredn-rfeye/clean V=s
make package/aredn-rfeye/compile V=s
find bin -name 'aredn-rfeye*.ipk' -print
```

## Install on a bench node

```sh
scp bin/packages/*/*/aredn-rfeye_*.ipk root@localnode.local.mesh:/tmp/
ssh root@localnode.local.mesh
opkg install /tmp/aredn-rfeye_*.ipk
```

Manual checks before enabling service:

```sh
/usr/sbin/rfeye-agent status
/usr/sbin/rfeye-survey survey
/usr/sbin/rfeye-survey utilization
/usr/sbin/rfeye-agent snapshot
/usr/sbin/rfeye-agent capture_status
/usr/sbin/rfeye-survey raw
```

Optional service enable:

```sh
uci set rfeye.main.enabled='1'
uci commit rfeye
/etc/init.d/rfeye enable
/etc/init.d/rfeye start
```

## Test the node GUI

Open:

```text
http://localnode.local.mesh/cgi-bin/apps/rfeye/user
```

The GUI provides:

- Start/stop data pull.
- Test duration selector.
- PHY and bin controls.
- Current FFT canvas.
- Utilization and external busy estimates.
- Raw JSON output for debugging.

## CGI/API tests

```sh
curl 'http://localnode.local.mesh/cgi-bin/apps/rfeye/data/agent.sh?action=status'
curl 'http://localnode.local.mesh/cgi-bin/apps/rfeye/data/agent.sh?action=radio_info'
curl 'http://localnode.local.mesh/cgi-bin/apps/rfeye/data/agent.sh?action=survey'
curl 'http://localnode.local.mesh/cgi-bin/apps/rfeye/data/agent.sh?action=utilization'
curl 'http://localnode.local.mesh/cgi-bin/apps/rfeye/data/agent.sh?action=snapshot'
curl 'http://localnode.local.mesh/cgi-bin/apps/rfeye/data/agent.sh?action=capture_status'
curl 'http://localnode.local.mesh/cgi-bin/apps/rfeye/data/agent.sh?action=survey_raw'
curl 'http://localnode.local.mesh/cgi-bin/apps/rfeye/data/agent.sh?action=start&seconds=5&bins=128&phy=phy0'
curl 'http://localnode.local.mesh/cgi-bin/apps/rfeye/data/agent.sh?action=stop'
```

## Commit built package artifact

For early node testing only, OpenClaw may commit a built `.ipk` under:

```text
artifacts/ipk/
```

Use a filename that includes package version, architecture, and build date where possible. Do not commit full OpenWrt build trees or temporary build output.

More detailed instructions are in:

- `docs/BUILD_AND_NODE_TEST.md`
- `docs/OPENCLAW_IPK_TASK.md`
- `docs/NODE_TEST_REPORT_TEMPLATE.md`
- `docs/TEST_RESULTS_KJ6DZB_WSB_ACDISH5.md`
