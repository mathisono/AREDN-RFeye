# Stock Firmware WMAC Caldata Reconnaissance Test Program

> Status: draft test program for RFeye caldata research  
> Target: stock Ubiquiti AirOS, OpenWrt, or AREDN lab nodes  
> Primary question: can the same device type running stock firmware prove a transferable WMAC caldata source or stock fallback behavior for AREDN/RFeye?

## Purpose

RFeye must not invent calibration data. The ath9k WMAC path on QCA9558/XC boards needs structurally valid AR9300-compatible data before the WMAC can be safely bound under AREDN/OpenWrt. The tested XC boards have blank `ART+0x1000`, while WA stock boards have shown real WMAC data at the same logical offset. This program collects stock-firmware evidence so RFeye can choose between:

1. importing same-device stock WMAC caldata through the Linux firmware-loader path;
2. reproducing a stock Ubiquiti default-template/fallback behavior;
3. labeling a donor or wrong-radio blob as bench-only; or
4. rejecting the candidate entirely.

This is a reconnaissance and evidence-capture program. It does not install, generate, or write caldata.

## Safety rules

These rules apply to the test program and to any follow-up RFeye implementation:

- Do not write ART/EEPROM flash.
- Do not feed blank `0xff` or all-zero data to ath9k.
- Do not feed random bytes.
- Do not treat PCI radio `ART+0x5000` as a production WMAC source.
- Do not rebind radios from the stock reconnaissance script.
- Do not channel-hop or disturb the production ath10k mesh radio.
- Treat full EEPROM/ART bundles as sensitive because they may contain board identifiers and MAC addresses.

## Deliverable added by this program

A node-side probe script is provided at:

```text
scripts/rfeye-stock-caldata-probe.sh
```

The script is intentionally BusyBox/ash compatible and read-only. It captures metadata, dumps the EEPROM/ART partition selected from `/proc/mtd`, extracts the likely WMAC and PCI regions, classifies their headers, and optionally captures a before/after AirView state.

## Test matrix

Run this on at least one board from each class:

| Device class | Firmware state | Reason |
|---|---|---|
| WA stock Ubiquiti | AirOS with AirView available | Confirms known-good factory WMAC caldata pattern at `EEPROM+0x1000`. |
| XC stock Ubiquiti same model as AREDN target | AirOS with AirView attempted | Determines whether XC stock uses real hidden caldata, built-in HAL template, AirView-only runtime setup, or no WMAC path. |
| XC AREDN/OpenWrt bench node | RFeye/PR #2730 path | Confirms how AREDN sees `ART+0x1000`, `ART+0x5000`, and the firmware-loader caldata path. |
| Optional Gen2/NanoBeam XC | Stock and/or OpenWrt | Tests whether a related XC device has usable factory WMAC caldata at `0x1000`. |

## Running the probe

Copy the script to the stock node:

```sh
scp scripts/rfeye-stock-caldata-probe.sh ubnt@<stock-node>:/tmp/
ssh ubnt@<stock-node>
sh /tmp/rfeye-stock-caldata-probe.sh
```

If AirView can be started from the stock web UI, use the before/after flow:

```sh
sh /tmp/rfeye-stock-caldata-probe.sh --airview-wait 60
```

During the wait period, start AirView in the stock Ubiquiti UI. The script captures baseline state, waits, captures after-state, re-dumps EEPROM/ART, and compares the before/after binary dump.

If the EEPROM/ART partition name is not detected automatically, force it:

```sh
RFEYE_MTD_NAME=EEPROM sh /tmp/rfeye-stock-caldata-probe.sh --airview-wait 90
# or
RFEYE_MTD_NAME=art sh /tmp/rfeye-stock-caldata-probe.sh
```

The result is a directory and usually a `.tar.gz` bundle under `/tmp`:

```text
/tmp/rfeye-stock-caldata-probe-<host>-<timestamp>/
/tmp/rfeye-stock-caldata-probe-<host>-<timestamp>.tar.gz
```

## What the bundle contains

Important files:

```text
manifest.txt
proc_mtd.before.txt
etc_version.before.txt
etc_board_info.before.txt
etc_board_json.before.txt
tmp_sysinfo_model.before.txt
tmp_sysinfo_board_name.before.txt
ifconfig-a.before.txt
iwconfig.before.txt
lsmod.before.txt
ps.before.txt
dmesg.before.txt
lib_firmware_tree.before.txt
proc_device_tree.before.txt
find_airview_spectral_cal.before.txt
eeprom.full.before.bin
eeprom.full.before.hash.txt
wmac_0x1000_1024.before.bin
wmac_0x1000_4096.before.bin
pci_0x5000_4096.before.bin
wmac_0x1000_1024.before.classification.txt
wmac_0x1000_4096.before.classification.txt
pci_0x5000_4096.before.classification.txt
```

When `--airview-wait` is used, the bundle also includes corresponding `after_airview` files and:

```text
eeprom.before_vs_after_airview.cmp.txt
```

## Classification rules

The probe uses simple structural classification. This does not prove RF correctness, but it quickly separates useful evidence from unsafe data.

| Classification | Meaning | Action |
|---|---|---|
| `blank_ff` | All bytes are `0xff`; common erased-flash pattern. | Reject as caldata. |
| `blank_00` | All bytes are `0x00`. | Reject as caldata. |
| `ar9300_template_0202_candidate` | Looks like AR9300 template-style EEPROM data. | Candidate for deeper validation. |
| `ar9300_compressed_4408_candidate` | Looks like compressed AR9300 EEPROM data. | Candidate for deeper validation, but likely PCI radio data at `0x5000`. |
| `unknown` | Not blank but not recognized by the simple header test. | Requires manual hexdump/code review. |

Do not promote a candidate to RFeye provisioning only because it is structurally accepted by ath9k. Transferability requires matching board family, radio path, and evidence that stock firmware uses the same data or fallback.

## Decision tree

### Case A: same-device stock `0x1000` contains real WMAC data

Evidence:

- `wmac_0x1000_*` classification is `ar9300_template_0202_candidate` or another recognized nonblank AR9300 structure.
- AirView creates a WMAC/monitor interface or otherwise confirms the stock WMAC path.
- The data is stable before/after AirView.

RFeye action:

- Build a validator that accepts this source only for matching board family/model evidence.
- Install it through `/lib/firmware/ath9k-eeprom-ahb-18100000.wmac.bin` on AREDN.
- Never write the blob to ART.
- Verify `phy*` creation and `ath9k/spectral_scan_ctl` on a bench node.

### Case B: same-device stock `0x1000` is blank but AirView works

Evidence:

- `wmac_0x1000_*` classification is `blank_ff`.
- AirView or `airviewd` still runs and exposes a WMAC scanner.
- No external firmware file appears under `/lib/firmware`.
- No EEPROM bytes change before/after AirView.

RFeye action:

- Treat this as evidence that stock Ubiquiti uses a HAL default-template or private runtime fallback.
- Reverse engineer the stock module/userland fallback before using donor caldata.
- Search stock modules for the default template MAC `00:02:03:04:05:06`, `ar9300_default`, `template`, `cal`, `eeprom`, and `spectral` strings.
- Implement a clearly labeled `stock_default_template` mode rather than presenting it as factory per-board calibration.

### Case C: stock has no AirView path and `0x1000` is blank

Evidence:

- `wmac_0x1000_*` is `blank_ff`.
- No `airview1`, `airviewd`, `ath_spectral`, or equivalent path is found.

RFeye action:

- Do not claim same-device stock support.
- Keep WA-reference or PCI `0x5000` only as explicit bench controls.
- Focus on software correction and external/reference calibration if the WMAC is still useful for receive-only spectral scanning.

### Case D: EEPROM changes after AirView starts

Evidence:

- `eeprom.before_vs_after_airview.cmp.txt` contains byte differences.

RFeye action:

- Stop. Investigate what changed before extracting or transferring anything.
- Determine whether the difference is a counter/state area or a real runtime calibration write.
- Do not use after-state data until understood.

## AREDN bench validation after a candidate is selected

Only after a candidate passes the stock reconnaissance stage:

```sh
scp ath9k-eeprom-ahb-18100000.wmac.bin root@localnode.local.mesh:/tmp/
ssh root@localnode.local.mesh
cp /tmp/ath9k-eeprom-ahb-18100000.wmac.bin /lib/firmware/ath9k-eeprom-ahb-18100000.wmac.bin
/usr/lib/rfeye/rfeye-wmac-rebind --status
/usr/lib/rfeye/rfeye-wmac-rebind --rebind
/usr/lib/rfeye/rfeye-wmac-rebind --status
```

Then confirm spectral readiness without changing the production mesh radio:

```sh
find /sys/kernel/debug/ieee80211 -maxdepth 4 -name spectral_scan_ctl -print
iw phy
dmesg | grep -iE 'ath9k|wmac|eeprom|cal|firmware' | tail -80
```

Pass condition:

```text
WMAC phy appears
ath9k debugfs exists
spectral_scan_ctl exists
RFeye parser can capture nonzero data
final spectral_scan_ctl state is disable
ART/EEPROM flash was not written
```

## Implementation follow-up for RFeye

The next code milestone should add:

1. `rfeye-caldata-dump` or integration of this probe into `rfeye-agent` for read-only bundles.
2. `rfeye-caldata-validate` that emits JSON classification and board compatibility warnings.
3. Atomic firmware-file installation using temporary file, fsync, and rename.
4. Explicit caldata modes:

```text
stock_same_device
stock_default_template
wa_reference_bench
pci_0x5000_bench_wrong_radio
disabled
```

5. A hard production default that rejects blank/random/wrong-radio data unless a bench-only override is set.

## Summary

The goal is not merely to find bytes that make ath9k load. The goal is to prove what the same device type does under stock firmware and then reproduce that behavior under AREDN in a way that is honest, reversible, and safe.

RFeye should only promote a caldata path when the evidence says what it is. If the evidence shows a generic fallback, label it as a generic fallback. If the evidence shows wrong-radio data, keep it bench-only. If the evidence shows same-device stock WMAC data, install it only through the firmware-loader path and never write it back to ART.
