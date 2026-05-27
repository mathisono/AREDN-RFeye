# AREDN Developer Message: Enable QCA955x On-Chip WMAC for Spectral Scanning

**To:** AREDN lead developer
**From:** Bill / KJ6DZB
**Re:** Patch to enable on-chip WMAC radio on Ubiquiti XC boards
**Date:** 2026-05-26

---

## Summary

I'd like to propose a small DTS patch that enables the on-chip WMAC radio on the
Ubiquiti XC board family (PowerBeam 5AC 500, Rocket 5AC Lite, NanoBeam AC XC).
This gives these boards a **dedicated second radio** that can be used for spectral
scanning and RF interference detection without touching the production ath10k mesh
radio.

## What the patch does

One new shared DTS include file (`qca955x_ubnt_xc-wmac.dtsi`) and one `#include`
line added to each of the three XC board DTS files. Total: **4 lines of functional
change**.

```dts
&wmac {
	status = "okay";
	nvmem-cells = <&cal_art_5000>;
	nvmem-cell-names = "calibration";
};
```

This changes the WMAC node from `status = "disabled"` (inherited from the base
`qca955x.dtsi`) to `status = "okay"`, which causes the ath9k platform driver to
probe the on-chip radio and expose it as `phy1`.

## Why this is safe

1. **The hardware is already there.** The QCA9558/QCA9560 SoC contains an
   AHB-attached 802.11n radio at MMIO address 0x18100000. It's present on every
   XC board — Ubiquiti's stock firmware uses it for their AirView spectral
   scanner feature.

2. **The driver is already loaded.** AREDN firmware already includes `ath9k`,
   `ath9k_hw`, and `ath9k_common` kernel modules. They load at boot with
   refcount 0 because nothing probes them. This patch just lets the device tree
   tell the driver there's hardware to bind to.

3. **The calibration data is already there.** The ART partition (mtd7) at offset
   0x5000 contains valid AR9300 compressed EEPROM data with a device-specific
   MAC address. The existing `cal_art_5000` nvmem cell is already defined in
   `qca955x_ubnt_xc.dtsi`. Both ath10k and ath9k read different fields from
   the same caldata block — this is how the Ubiquiti stock firmware uses it too.

4. **It follows proven upstream patterns.** 11+ QCA955x devices in upstream
   OpenWrt already enable `&wmac` with ath9k: Archer C7 v1, Aruba AP-115,
   Huawei AP5030DN, LibreRouter v1, OpenMesh MR, Sophos AP15, Ruckus R500,
   Meraki MR18, and others.

5. **The production radio is never touched.** The ath10k PCI radio (`phy0` /
   `wlan0`) continues to serve the AREDN mesh exactly as before. The WMAC
   radio (`phy1`) is a completely separate hardware path.

6. **No new packages or dependencies.** The ath9k modules are already in the
   firmware image. No additional kernel modules, firmware blobs, or userspace
   packages are required.

## What it enables

With `phy1` exposed, AREDN apps (like RFeye) can:

- Create a monitor-mode interface on `phy1`
- Use the upstream ath9k spectral scan debugfs interface
  (`/sys/kernel/debug/ieee80211/phy1/ath9k/spectral_scan_ctl`)
- Sweep channels on `phy1` to build a wideband spectral view
- Detect RF interference across the 5 GHz band without any impact on
  mesh performance

This is the same architecture Ubiquiti uses for AirView — we observed it
running on a stock Rocket 5AC Lite via clean-room behavioral analysis
(SSH shell commands only, no code copied or decompiled).

## Affected boards

| Board | SoC | DTS file |
|-------|-----|----------|
| PowerBeam 5AC 500 | QCA9558 | `qca9558_ubnt_powerbeam-5ac-500.dts` |
| Rocket 5AC Lite | QCA9558 | `qca9558_ubnt_rocket-5ac-lite.dts` |
| NanoBeam AC XC | QCA9558 | `qca9558_ubnt_nanobeam-ac-xc.dts` |

All three share the `qca955x_ubnt_xc.dtsi` include and have the same
SoC, ART layout, and WMAC hardware.

## What I've verified

- [x] WMAC node exists in device tree (`wmac@18100000`, `compatible = "qca,qca9550-wmac"`)
- [x] Valid ath9k caldata at ART 0x5000 (header `44 08`, device-specific MAC)
- [x] ath9k module loaded, platform driver registered, just needs device to probe
- [x] 11+ upstream OpenWrt reference devices use same pattern
- [x] Patch applies cleanly against OpenWrt v25.12.4 (AREDN base)
- [x] RF antenna path confirmed usable on PBE-5AC-500
- [x] AirView on stock R5AC-Lite uses this same on-chip radio for spectral scanning
- [ ] Firmware build and flash to bench hardware (next step)
- [ ] Confirm phy1 appears and ath9k spectral scan works

## Files changed

```
patches/758-enable-wmac-spectral-scanner.patch   (new, 50 lines)
patches/series                                   (1 line added)
```

The patch itself creates:
- `target/linux/ath79/dts/qca955x_ubnt_xc-wmac.dtsi` (new, 17 lines)
- Adds `#include "qca955x_ubnt_xc-wmac.dtsi"` to 3 existing DTS files

## Risk assessment

**Low risk.** The change enables a radio that's already physically present
and already has driver support loaded. The worst case is that ath9k probes
a radio that was previously ignored — this has no effect on ath10k mesh
operation. If there were any issue, the WMAC can be disabled again by
reverting the patch.

The only consideration is that enabling a second radio slightly increases
boot time (ath9k probe) and memory usage (ath9k state for one more phy).
Both are negligible on these devices.

## Upstream potential

Once validated on AREDN, this DTS change could be submitted to upstream
OpenWrt as well. The Ubiquiti XC boards are the only QCA955x devices in
OpenWrt that don't enable their WMAC — it was likely just an oversight
since Ubiquiti's stock firmware manages this radio through their
proprietary driver stack.

---

Let me know if you'd like to review the patch or have questions about the
research behind it. Happy to provide the full behavioral analysis docs
from the AirView observation session.
