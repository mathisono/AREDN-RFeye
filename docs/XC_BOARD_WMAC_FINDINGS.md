# XC Board WMAC Findings — AREDN PR #2725

## Purpose

This note updates the RFeye AirView/wideband scanner roadmap based on AREDN pull request #2725 and the follow-up hardware verification work around Ubiquiti XC boards.

The short version: enabling the QCA955x/QCA9560 on-chip WMAC is still the right *architecture idea* for AirView-like scanning, but PR #2725 should not be treated as a working dependency yet. Hardware verification indicates the PR cannot work as written on the tested XC boards because expected AR9300 WMAC calibration data is not present.

## PR summary

AREDN PR #2725 proposes enabling the AHB-attached 802.11n WMAC present inside the QCA9558/QCA9560 SoC on Ubiquiti XC board-family devices:

- PowerBeam 5AC 500
- Rocket 5AC Lite
- NanoBeam AC XC

The goal is to expose the on-chip WMAC as an ath9k radio that could serve as a dedicated spectral scanner while the main PCI-attached QCA988X / ath10k radio continues serving the AREDN mesh.

This matches the AirView-style architecture RFeye wants: a main mesh radio plus a separate scanner radio.

## Patch shape

The PR adds a shared DTS include:

```text
target/linux/ath79/dts/qca955x_ubnt_xc-wmac.dtsi
```

with:

```dts
&wmac {
	status = "okay";
	nvmem-cells = <&cal_art_5000>;
	nvmem-cell-names = "calibration";
};
```

Then it includes that file from the PowerBeam 5AC 500, Rocket 5AC Lite, and NanoBeam AC XC device trees.

It is carried in AREDN as:

```text
patches/758-enable-wmac-spectral-scanner.patch
```

and added to:

```text
patches/series
```

## Initial value to RFeye

If this worked, RFeye could gain a hardware path for AirView-like scanning:

```text
phy0 / ath10k / QCA988X = AREDN mesh radio, stays on channel
phy1 / ath9k / QCA955x WMAC = scanner radio, monitor/spectral/sweep use
```

That would avoid disrupting the mesh link while scanning the band.

This is the same broad architecture observed on stock Rocket 5AC Lite AirView behavior: the main AP radio stays up while a separate scanner path sweeps and stitches spectrum data.

## Review and hardware verification findings

The PR discussion raised several important issues.

### AREDN must not automatically use the scanner radio

If the extra WMAC appears, it must be excluded from normal AREDN mesh use. The reviewer pointed to `/etc/radios.json` and existing disabled-radio examples as the correct pattern.

RFeye implication:

```text
A scanner radio must be reserved for RFeye/AirView-style scanning.
AREDN must not configure it as a normal mesh interface.
```

### Calibration source is the blocker

The patch assumes the on-chip WMAC can use the existing `cal_art_5000` nvmem cell.

Hardware verification reported that this is not valid for the tested XC boards:

- Offset `0x1000` is where upstream OpenWrt QCA955x devices normally store on-chip WMAC / ath9k AR9300 compressed EEPROM calibration data.
- Offset `0x5000` is where the PCI QCA988x / ath10k board data lives.
- The tested XC boards did not contain valid AR9300 WMAC calibration data.

Conclusion from the verification: the PR cannot work as written.

## Current RFeye interpretation

This does **not** invalidate the dual-radio scanner architecture. It does mean RFeye cannot assume Ubiquiti XC boards expose a usable calibrated on-chip WMAC under AREDN/OpenWrt.

The architecture is still correct:

```text
AirView-like full-band scanning wants a dedicated scanner radio.
```

But the XC-board built-in WMAC path is currently unproven / blocked by calibration data.

## Roadmap impact

### Production RFeye path remains unchanged

RFeye production mode remains:

```text
QCA988X / ath10k current-channel FFT
no channel hopping
no channel changes
captures under /tmp/rfeye
```

This path does not depend on PR #2725.

### PR #2725 becomes a research track, not a dependency

Treat PR #2725 as:

```text
Research track: Can Ubiquiti XC boards expose a usable second scanner radio?
Status: blocked / cannot work as written until calibration issue is resolved
```

Do not make RFeye mainline depend on this PR.

### AirView-like mode remains separate

AirView-like wideband mode requires one of:

1. a proven second scanner radio with valid calibration;
2. an external second radio/scanner node;
3. lab-only retune/channel-step sweep with service impact;
4. vendor/private firmware behavior that can be understood clean-room.

## Required acceptance before using XC WMAC for RFeye

Before building RFeye scanner mode on the XC on-chip WMAC, require all of these:

```text
- phy1 appears reliably
- ath9k binds cleanly
- valid calibration is loaded
- monitor mode works
- spectral scan works
- AREDN excludes the radio from normal mesh use
- main ath10k mesh radio remains stable
- RFeye can read spectral data from the scanner radio
```

## Recommended next tests

On any experimental PR #2725 firmware build, collect:

```sh
dmesg | grep -iE 'ath|wmac|cal|eeprom|art|qca|phy'
iw phy
iw dev
ls /sys/class/ieee80211
cat /etc/radios.json 2>/dev/null || true
hexdump -C /dev/mtdblock* 2>/dev/null | head
```

More focused caldata inspection should compare:

```text
ART offset 0x1000: expected AR9300 / ath9k WMAC calibration on many QCA955x boards
ART offset 0x5000: expected QCA988x / ath10k PCI radio board data on XC boards
```

Do not write calibration or ART data during RFeye testing.

## Updated conclusion

AREDN PR #2725 is important because it targets the exact hardware architecture RFeye needs for safe AirView-like scanning: a separate scanner radio.

However, current hardware verification indicates that the Ubiquiti XC boards tested do not contain the AR9300 WMAC calibration data needed for the patch to work as written. Therefore:

```text
RFeye should keep PR #2725 as a research dependency only.
Production RFeye should continue on the current-channel QCA988X path.
AirView-like full-band scanning should use a proven second radio or remain lab-only.
```
