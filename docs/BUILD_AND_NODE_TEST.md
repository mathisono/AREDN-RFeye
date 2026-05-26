# Build and Node Test Instructions

This guide describes how to build the RFeye `.ipk`, install it on a bench AREDN node, and run the current diagnostic test flow.

RFeye is still in active hardware bring-up. The package/UI/API work, but the current first-node blocker is parser/framing: `spectral_scan0` returns nonzero raw data while `rfeye-spectral-parse` currently emits zero valid FFT frames on the first tested node.

## Safety rules

Use a bench node first.

- Do not test on a production mesh node.
- Do not change channel settings.
- Do not enable channel hopping.
- Keep capture data under `/tmp/rfeye`.
- Stop if Wi-Fi or node reachability becomes unstable.

## 1. Local validation before building

From the repository root:

```sh
sh scripts/check-parser-source-sync.sh
sh -n package/aredn-rfeye/files/usr/sbin/rfeye-agent
sh -n package/aredn-rfeye/files/usr/sbin/rfeye-survey
sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/data/agent.sh
sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/user
cc -Wall -Wextra -o /tmp/rfeye-spectral-parse src/rfeye_spectral_parse.c
/tmp/rfeye-spectral-parse --help | grep -E -- '--probe|--resync'
scripts/test-parser-smoke.sh
scripts/test-hardware-fixture-probe.sh
```

Expected:

```text
Parser source sync check passed
RFeye parser smoke test passed
```

This validates the synthetic parser fixture only. It does not prove real ath10k spectral output is decoded.

## 2. Build the `.ipk`

Example paths:

```sh
RFeye=$HOME/src/AREDN-RFeye
AREDN=$HOME/src/aredn
```

Copy the package into the AREDN/OpenWrt build tree or SDK:

```sh
rsync -av --delete "$RFeye/package/aredn-rfeye/" \
  "$AREDN/package/aredn-rfeye/"
```

Build only RFeye:

```sh
cd "$AREDN"
make package/aredn-rfeye/clean V=s
make package/aredn-rfeye/compile V=s
find bin -name 'aredn-rfeye*.ipk' -print
```

Known SDK note: one test SDK required `LD_LIBRARY_PATH=/home/bill/src/local-gawk/lib` because the SDK host `gawk` needed `libsigsegv.so.2`.

## 3. Save the build artifact

From the RFeye repo:

```sh
cd "$RFeye"
mkdir -p artifacts/ipk
cp "$(find "$AREDN/bin" -name 'aredn-rfeye*.ipk' | head -n1)" artifacts/ipk/
sha256sum artifacts/ipk/aredn-rfeye_*.ipk
```

Update:

```text
artifacts/ipk/BUILD_NOTES.md
```

Record package version, architecture, SHA256, build tree path, validation results, and node-test status.

## 4. Install on a bench node

Direct install example:

```sh
scp artifacts/ipk/aredn-rfeye_*.ipk root@localnode.local.mesh:/tmp/
ssh root@localnode.local.mesh
opkg install --force-reinstall /tmp/aredn-rfeye_*.ipk
```

Jump-host example used during testing:

```text
Jump host: MSE-88 / 192.168.3.88
Target node: 10.188.138.222
Target SSH: port 2222
Target web: http://10.188.138.222:8080
```

Use the site-appropriate SSH/scp jump method. Do not hard-code credentials in committed docs or scripts.

## 5. Basic node checks

After install:

```sh
/usr/sbin/rfeye-agent status
/usr/sbin/rfeye-agent radio_info
/usr/sbin/rfeye-survey survey
/usr/sbin/rfeye-survey utilization
```

Expected:

- Valid JSON from each command.
- Clear diagnostic JSON if unsupported.
- Trusted radio info should show the actual node channel/frequency/width.

For the first test node, expected trusted radio state was:

```text
phy0 / wlan0 / channel 141 / 5705 MHz / 20 MHz / IBSS AREDN-20-v3
```

## 6. Current parser/framing diagnostic flow

This is the most important test while the parser issue remains open:

```sh
/usr/sbin/rfeye-agent reset
/usr/sbin/rfeye-agent raw_capture_test 10 128 phy0
/usr/sbin/rfeye-agent raw_inspect
/usr/sbin/rfeye-agent parser_probe
/usr/lib/rfeye/rfeye-spectral-parse --probe --input /tmp/rfeye/raw-test.tlv
/usr/lib/rfeye/rfeye-spectral-parse --debug --stats --input /tmp/rfeye/raw-test.tlv --phy phy0 --limit 5 --bins 64
hexdump -C /tmp/rfeye/raw-test.tlv | head -40
```

Interpretation:

- If `bytes_read` is zero, spectral capture is not producing data.
- If `bytes_read` is nonzero but `frames_emitted` is zero, preserve the raw file and investigate TLV/framing/layout.
- If frames are emitted, proceed to heatmap/capture-loop testing.

Current first-node result:

```text
raw_capture_test: bytes_read=262144, frames_emitted=0
raw data: mostly nonzero
current diagnosis: raw framing/layout mismatch remains
```

## 7. Capture-loop and heatmap tests

Run only after the basic diagnostic flow:

```sh
/usr/sbin/rfeye-agent reset
/usr/sbin/rfeye-agent start 10 128 phy0
sleep 12
/usr/sbin/rfeye-agent capture_status
/usr/sbin/rfeye-agent acquisition_debug
/usr/sbin/rfeye-agent heatmap_bundle
/usr/sbin/rfeye-agent waveform
/usr/sbin/rfeye-agent waterfall
/usr/sbin/rfeye-agent ambient
/usr/sbin/rfeye-agent stop
cat /sys/kernel/debug/ieee80211/phy0/ath10k/spectral_scan_ctl 2>/dev/null || true
```

Expected when parser decoding works:

- `frames_captured` increases.
- `waveform.bins` is populated.
- `waterfall.rows` is populated.
- `ambient.rows` eventually populates.
- `spectral_scan_ctl` ends at `disable` after stop/reset.

If no frames are decoded, `heatmap_bundle` should still return valid JSON with diagnostic state.

## 8. CGI/API tests

From a workstation with access to the node web service:

```sh
BASE='http://localnode.local.mesh/cgi-bin/apps/rfeye/data/agent.sh'

curl "$BASE?action=status"
curl "$BASE?action=radio_info"
curl "$BASE?action=ui_state"
curl "$BASE?action=heatmap_bundle"
curl "$BASE?action=waveform"
curl "$BASE?action=waterfall"
curl "$BASE?action=ambient"
curl "$BASE?action=survey"
curl "$BASE?action=utilization"
curl "$BASE?action=survey_raw"
curl "$BASE?action=raw_inspect"
curl "$BASE?action=parser_probe"
curl "$BASE?action=acquisition_debug"
curl "$BASE?action=start&seconds=10&bins=128&phy=phy0"
curl "$BASE?action=capture_status"
curl "$BASE?action=stop"
```

Expected:

- HTTP 200 where applicable.
- Valid JSON body.
- No shell tracebacks.
- Clear errors if hardware/parser support is incomplete.

## 9. Node GUI test

Open:

```text
http://localnode.local.mesh/cgi-bin/apps/rfeye/user
```

Verify:

- Page loads.
- Trusted channel/frequency/width are correct.
- Start/stop/reset controls are visible.
- Capture state changes.
- No-frame count does not kill the UI.
- Waveform/waterfall/ambient panels stay alive even if empty.

The first node currently loads the GUI correctly, but waveform/waterfall/ambient remain empty until the parser framing issue is resolved.

## 10. Preserve raw fixtures for parser work

When `bytes_read > 0` and `frames_emitted == 0`, preserve these files:

```text
/tmp/rfeye/raw-test.tlv
/tmp/rfeye/debug-last-cycle.tlv
/tmp/rfeye/debug-last-parser.txt
/tmp/rfeye/debug-last-stats.json
```

Copy them into a local fixture directory, gzipping large binary files before commit:

```text
fixtures/hardware/<NODE_NAME>_<TEST_VERSION>/
```

These fixtures are needed to refine `rfeye-spectral-parse` offline.

## 11. What to record in the test report

Use `docs/NODE_TEST_REPORT_TEMPLATE.md` and include:

- Node model and chipset
- AREDN/OpenWrt version
- Kernel version
- Radio PHY/interface/channel/frequency/width
- Whether debugfs is mounted
- Whether ath10k spectral files exist
- Package version and SHA256
- `raw_capture_test` bytes and frame count
- `raw_inspect` summary
- `parser_probe` result
- Whether `heatmap_bundle` rows populate
- Final `spectral_scan_ctl` state
- Wi-Fi/mesh stability notes
- PASS / PARTIAL / FAIL

## 12. Current expected result

Until parser framing is fixed, the expected first-node result is:

```text
Package install: PASS
Radio info: PASS
CGI/API: PASS
GUI load: PASS
Raw capture bytes: PASS
Parsed FFT frames: FAIL/PARTIAL
Heatmap rows: empty
Overall: PARTIAL, parser/framing work required
```
