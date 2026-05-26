# QCA988X Wideband Spectral Working Brief

## Purpose

This brief collects the current RFeye research thread on whether the Ubiquiti PowerBeam 5AC 500 / PBE-5AC-500 can expose a wider spectral view than the normal ath10k current-channel FFT stream.

This is a research/design brief only. It is not approval to add channel hopping or production RF behavior changes to RFeye.

## Hardware target

Known target hardware:

```text
Device: Ubiquiti PowerBeam 5AC 500 / PBE-5AC-500
Host SoC / CPU: Qualcomm Atheros QCA9558, MIPS 74Kc-class, 720 MHz
Radio: Qualcomm Atheros QCA988X 5 GHz 802.11ac
RAM / Flash: 128 MB DDR2 / 16 MB flash
Driver family: ath10k / QCA988X
AREDN tested node: KJ6DZB-WSB-ACdish5
AREDN/OpenWrt: AREDN 4.26.1.0, kernel 6.6.119 mips
```

Do not confuse this with PowerBeam 5AC Gen2 / ISO Gen2 hardware. The Gen2 models can differ from the supported non-Gen2 PowerBeam 5AC 500 / 500 ISO family.

## Current RFeye behavior

RFeye currently uses the upstream ath10k spectral scan path exposed through debugfs:

```text
/sys/kernel/debug/ieee80211/phy0/ath10k/spectral_scan_ctl
/sys/kernel/debug/ieee80211/phy0/ath10k/spectral_bins
/sys/kernel/debug/ieee80211/phy0/ath10k/spectral_scan0
```

This path is current-channel spectral capture. On the tested node:

```text
Mode: IBSS
Channel: 141
Center frequency: 5705 MHz
Configured width: 20 MHz
Observed spectral view: current-channel FFT only
```

A full 5.1-5.9 GHz band view is not exposed by the normal ath10k debugfs interface. A full-band AirView-style display likely requires one of these:

1. wider operating channel width, such as 40/80 MHz, if the node is configured that way;
2. experimental WMI spectral report settings that expose more bins or a wider report;
3. lab-only retuning/channel hopping and stitching;
4. a second radio dedicated to scanning;
5. vendor-private firmware behavior that is not exposed by upstream ath10k.

## Public kernel evidence

Important public kernel files:

```text
drivers/net/wireless/ath/ath10k/spectral.c
drivers/net/wireless/ath/ath10k/spectral.h
drivers/net/wireless/ath/ath10k/wmi.h
drivers/net/wireless/ath/spectral_common.h
```

Key observations:

- `spectral_scan0` is a relay file receiving FFT sample TLVs from the driver.
- `ath10k_spectral_scan_config()` builds a `wmi_vdev_spectral_conf_arg` and sends it to firmware.
- `ath10k_spectral_scan_trigger()` enables and triggers spectral scan on a selected vdev.
- `ath10k_get_spectral_vdev()` selects an existing virtual interface, which strongly suggests the open path is tied to the active radio channel/vdev.
- `ath10k_spectral_process_fft()` wraps firmware FFT/PHY-error data into `ATH_FFT_SAMPLE_ATH10K` TLV frames.
- The driver maps nominal channel widths to approximate spectral display widths: 20 -> 22 MHz, 40 -> 44 MHz, 80 -> 88 MHz.

## WMI spectral configuration knobs

The public WMI structure includes these fields:

```text
scan_count
scan_period
scan_priority
scan_fft_size
scan_wb_rpt_mode
scan_rssi_rpt_mode
scan_pwr_format
scan_rpt_mode
scan_bin_scale
scan_dbm_adj
scan_chn_mask
```

The normal ath10k debugfs interface does not expose all of these as user controls. Many are currently hard-coded/defaulted inside the driver when the spectral config WMI command is built.

### Field summary

| Field | Public meaning / working assumption | Research priority |
|---|---|---|
| `scan_count` | Number of FFT samples; `0` means infinite. | Medium |
| `scan_period` | Sampling period/spacing. Default is `35`. | High |
| `scan_priority` | Firmware spectral scheduling priority. Default is `1`. | Medium |
| `scan_fft_size` | FFT size; public comment says bins are `2^(fft_size - bin_scale)`. | High |
| `scan_wb_rpt_mode` | Wideband report mode. Publicly underdocumented. | High, risky |
| `scan_rssi_rpt_mode` | RSSI report mode. | Low |
| `scan_pwr_format` | Power report format. | Medium |
| `scan_rpt_mode` | FFT report mode. Mode `2` is default in-band bins; mode `3` is documented as 2x oversampled bins/all. | Highest |
| `scan_bin_scale` | Bin-count scaling, paired with `scan_fft_size`. | High |
| `scan_dbm_adj` | dBm adjustment / calibration-related flag. | Medium |
| `scan_chn_mask` | Channel mask; meaning needs empirical testing. | High, risky |

## What kernel has to be updated?

This is not primarily an RFeye userspace change. To expose these knobs, update the OpenWrt/AREDN kernel ath10k driver source, specifically the ath10k code carried by OpenWrt's `mac80211` package/backports.

### Upstream Linux files involved

The conceptual patch targets these upstream files:

```text
drivers/net/wireless/ath/ath10k/spectral.c
drivers/net/wireless/ath/ath10k/spectral.h
drivers/net/wireless/ath/ath10k/wmi.h
```

`wmi.h` already contains the WMI fields and defaults. In most cases it should not need a protocol-layout change. It is useful reference material and may need only helper enums/range constants if desired.

`spectral.h` currently has a small `struct ath10k_spec_scan` with only the basic user-tunable spectral fields. It would need extension or a new experimental config structure for the additional WMI settings.

`spectral.c` is the main implementation target. It currently creates the debugfs spectral controls, builds the WMI config, and fills the fields in `ath10k_spectral_scan_config()`. This is where new debugfs read/write controls would be added and where those values would be copied into `wmi_vdev_spectral_conf_arg`.

### OpenWrt/AREDN patch location

In an OpenWrt/AREDN build tree, this should be carried as a kernel/mac80211 patch, not as an RFeye package-only change.

Likely location:

```text
package/kernel/mac80211/patches/ath/<new-patch>.patch
```

Suggested patch name:

```text
999-ath10k-expose-experimental-spectral-wmi-knobs.patch
```

Exact directory may vary depending on the AREDN/OpenWrt tree layout and whether ath10k or ath10k-ct is selected. If AREDN uses ath10k-ct for the target, the corresponding CT driver source/patch path must be checked separately.

### Runtime kernel on tested node

The tested AREDN node reports kernel 6.6.119, so the relevant build target is the AREDN/OpenWrt 6.6 ath10k/mac80211 backport set used for that release.

## Proposed experimental driver design

Add experimental debugfs controls under the existing ath10k debugfs directory, for example:

```text
spectral_scan_count
spectral_scan_period
spectral_scan_priority
spectral_scan_fft_size
spectral_scan_wb_rpt_mode
spectral_scan_rssi_rpt_mode
spectral_scan_pwr_format
spectral_scan_rpt_mode
spectral_scan_bin_scale
spectral_scan_dbm_adj
spectral_scan_chn_mask
```

Design rules:

- Defaults must remain identical to current upstream values.
- Controls must be opt-in and clearly experimental.
- Writes should validate ranges conservatively.
- Unknown firmware rejection must fail cleanly.
- Production RFeye must continue to use safe defaults.
- No channel hopping or channel changes should be added to production RFeye.

## Suggested implementation sketch

1. Extend `struct ath10k_spec_scan` in `spectral.h` or add a new experimental sub-struct for the WMI fields.
2. Initialize the extra fields to the current `WMI_SPECTRAL_*_DEFAULT` values.
3. Add debugfs read/write handlers in `spectral.c` for each exposed field.
4. In `ath10k_spectral_scan_config()`, use the configured values instead of hard-coded defaults for the fields being researched.
5. Keep `spectral_scan_ctl` semantics unchanged.
6. Build a bench-only kernel/IPK image.
7. Test each setting individually before combining settings.

## First test matrix

Baseline:

```text
scan_rpt_mode=2
scan_fft_size=7
scan_bin_scale=1
scan_wb_rpt_mode=0
scan_chn_mask=1
```

Test A:

```text
scan_rpt_mode=3
scan_fft_size=7
scan_bin_scale=1
```

Test B:

```text
scan_rpt_mode=3
scan_fft_size=8
scan_bin_scale=1
```

Test C:

```text
scan_rpt_mode=3
scan_fft_size=8
scan_bin_scale=0
```

Test D:

```text
scan_rpt_mode=3
scan_wb_rpt_mode=1
```

Test E:

```text
scan_rpt_mode=3
alternate scan_chn_mask values
```

Only run later tests after earlier ones are stable.

## What to record per test

For each configuration, record:

```text
- exact WMI knob values
- whether firmware accepts the config
- whether spectral_scan0 produces data
- raw_capture_test bytes_read
- frames_emitted
- parser_probe result
- TLV/sample type
- bin count per frame
- apparent useful spectral span
- frame rate
- no_frame_count
- driver/kernel warnings
- dmesg/logread tail
- final spectral_scan_ctl state
- mesh link stability
```

## Expected outcomes

Possible outcomes:

1. `scan_rpt_mode=3` produces more bins or a more useful report for the current channel.
2. `scan_fft_size` / `scan_bin_scale` changes bin count or parser behavior.
3. `scan_wb_rpt_mode` or `scan_chn_mask` changes report content, fails, or is ignored by firmware.
4. No setting gives full-band 5 GHz coverage without retuning.

The most realistic expectation is better current-channel/in-channel reporting, not a true 5.1-5.9 GHz instantaneous sweep.

## RFeye GUI requirement

Until wider behavior is proven, the GUI must be honest:

```text
Current-channel FFT view
This display shows the node's current operating channel only.
It is not a full 5 GHz band sweep.
Full-band AirView-style scanning would require lab-only retuning/channel hopping, vendor-private behavior, or a second radio.
```

## Safety boundary

Do not implement production channel hopping in RFeye.

Do not change normal RFeye defaults.

Do not copy or decompile proprietary Ubiquiti firmware code. Vendor firmware can be observed behaviorally on hardware you own, but implementation must remain clean-room and based on public kernel/OpenWrt/ath10k information.

## Next OpenClaw task

Research/design only:

1. Locate the exact AREDN/OpenWrt mac80211 patch path for ath10k on the current build tree.
2. Confirm whether the target uses upstream ath10k or ath10k-ct.
3. Draft a bench-only patch exposing the WMI spectral knobs through debugfs.
4. Do not merge into normal RFeye until tested safely on a bench node.
