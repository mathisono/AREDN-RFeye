# RFeye r11 Test Results — KJ6DZB-WSB-ACdish5

Date: 2026-05-25 16:33–16:45 PDT

## Package under test

- Commit: `3f34455` (`rfeye: improve node GUI layout and heatmap scaling`)
- IPK: `artifacts/ipk/aredn-rfeye_0.1.0-r11_mips_24kc.ipk`
- SHA256: `38c8fff8b508dedb731665d23b57d340dbae5a0550b070cd1854acd725cf5aac`

## Access path result

- MSE-88 reachable: `ping 192.168.3.88` PASS
- MSE-88 SSH as `mat` with provided credentials: PASS
- From MSE-88 to node `10.188.138.222`: PASS
  - ping PASS
  - web HEAD PASS (`307` redirect to `/a/status`)
  - ssh `root@10.188.138.222:2222` PASS

## Install result

- `opkg install --force-reinstall /tmp/aredn-rfeye_0.1.0-r11_mips_24kc.ipk` PASS
- Installed version confirmed:
  - `aredn-rfeye - 0.1.0-r11`

## CLI sanity result

Commands and result:
- `/usr/sbin/rfeye-agent reset` PASS (`spectral_ctl_after:disable`)
- `/usr/sbin/rfeye-agent radio_info` PASS
  - trusted radio: channel `141`, frequency `5705 MHz`, width `20 MHz`, `phy0`/`wlan0`
- `/usr/sbin/rfeye-agent pipeline_test 128 phy0` **FAIL**
  - `ok:false`, `parser_frame_present:false`, `frames_emitted:0`

## Backend capture test (300s run, sampled at +60s then stopped)

Commands run:
- `/usr/sbin/rfeye-agent start 300 128 phy0`
- `sleep 60`
- `/usr/sbin/rfeye-agent capture_status`
- `/usr/sbin/rfeye-agent heatmap_bundle`
- `/usr/sbin/rfeye-agent storage_status`
- `/usr/sbin/rfeye-agent stop`

Observed at +60s:
- `frames_captured: 0`
- `no_frame_count: 37/38`
- `waveform_bin_count: 0`
- `waterfall_row_count: 0`
- `ambient_row_count: 0`
- `frame_rate: 0.00`
- `state_dir_bytes: ~618496`

This indicates backend capture/parsing did not produce frames on this live run.

## CGI result

From MSE-88:
- `action=radio_info` PASS (valid JSON)
- `action=capture_status` PASS (valid JSON)
- `action=heatmap_bundle` PASS (valid JSON, includes r11 metadata fields)
- `action=storage_status` PASS (valid JSON)
- `action=pipeline_status` PASS (valid JSON)

## GUI acceptance result

Direct browser-based visual acceptance was not completed in this shell-only run (no screenshots).

HTML endpoint check:
- `/cgi-bin/apps/rfeye/user` returns expected r11 page content including:
  - `html, body { min-height: 100%; overflow-y: auto; }`
  - compact card layout CSS
  - collapsible raw JSON section support (`details/summary` present in page source)

But because live frames were not produced, waveform/waterfall/ambient meaningfulness could not be validated in operation.

## Final safety check

Executed:
- `/usr/sbin/rfeye-agent stop`
- `/usr/sbin/rfeye-agent reset`
- `cat /sys/kernel/debug/ieee80211/phy0/ath10k/spectral_scan_ctl`
- `/usr/sbin/rfeye-agent storage_status`

Result:
- final `spectral_scan_ctl`: `disable` ✅
- capture state cleaned up ✅
- files remain under `/tmp/rfeye` ✅
- no channel hopping/channel changes were performed ✅

## Metrics summary

- Frames captured: `0`
- Waterfall rows: `0`
- Storage usage (final `state_dir_bytes`): `565248`
- Final `spectral_scan_ctl`: `disable`

## Verdict

**FAIL**

Reason: live backend capture did not produce parsed frames on node (`pipeline_test` failed; 60s capture had zero frames), so GUI operational visualization acceptance could not be completed against live data.

## Next recommended fix

Investigate node-side acquisition/parser regression or environment condition vs r10 baseline:
1. Run `/usr/sbin/rfeye-agent raw_capture_test 10 128 phy0`
2. Run `/usr/sbin/rfeye-agent raw_inspect`
3. Run `/usr/sbin/rfeye-agent parser_probe`
4. Compare parser stats/raw bytes with known-good r10 node behavior before re-running full GUI acceptance.
