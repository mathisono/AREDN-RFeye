# RFeye r12 Test Results — KJ6DZB-WSB-ACdish5

Date: 2026-05-25 PDT

## Package under test

- Commit: pending local commit (`rfeye: sync packaged parser source with resync support`)
- IPK: `artifacts/ipk/aredn-rfeye_0.1.0-r12_mips_24kc.ipk`
- SHA256: `947c7231310150f8c71c8d4e1d1a04880dd851ba545894b42aa3798c784321a3`

## Root cause

- r11 shipped a stale package-local parser source.
- `package/aredn-rfeye/src/rfeye_spectral_parse.c` now matches `src/rfeye_spectral_parse.c`.
- Installed parser help now includes `--probe` and `--resync`.

## Install result

- `opkg install --force-reinstall /tmp/aredn-rfeye_0.1.0-r12_mips_24kc.ipk` PASS
- Installed version confirmed: `aredn-rfeye - 0.1.0-r12`

## Parser verification

- `/usr/lib/rfeye/rfeye-spectral-parse --help | grep -E -- '--probe|--resync'` PASS
- `sha256sum /usr/lib/rfeye/rfeye-spectral-parse` PASS

## Acceptance result

### CLI/backend checks

- `/usr/sbin/rfeye-agent reset` PASS (`spectral_ctl_after:disable`)
- `/usr/sbin/rfeye-agent radio_info` PASS
  - channel `141`, `5705 MHz`, `20 MHz`, `phy0`/`wlan0`
- `/usr/sbin/rfeye-agent raw_capture_test 12 128 phy0` PASS
  - `frames_emitted: 1`
- `/usr/sbin/rfeye-agent parser_probe` PASS
  - valid JSON; probe/resync stats returned
- `/usr/sbin/rfeye-agent pipeline_test 128 phy0` PASS
  - `parser_frame_present:true`, `frames_emitted:1`
- `/usr/sbin/rfeye-agent heatmap_bundle` PASS
  - waveform/waterfall/ambient data present

### 5-minute readiness run

- `/usr/sbin/rfeye-agent start 300 128 phy0` PASS
- `sleep 60`
- `/usr/sbin/rfeye-agent capture_status` PASS
- `/usr/sbin/rfeye-agent heatmap_bundle` PASS
  - non-empty waveform and waterfall data
- `/usr/sbin/rfeye-agent storage_status` PASS
- `/usr/sbin/rfeye-agent stop` PASS
- final `spectral_scan_ctl`: `disable`

## GUI spot check

- `http://10.188.138.222:8080/cgi-bin/apps/rfeye/user`
- HTML source check PASS
  - scrollable page
  - compact controls/details layout
  - waveform, waterfall, ambient, and scaling sections present
  - trusted radio info still shows channel 141 / 5705 MHz / 20 MHz

## Verdict

**PASS**
