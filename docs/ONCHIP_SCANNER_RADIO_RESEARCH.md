# On-Chip Scanner Radio Research — QCA9558 / ath9k WMAC

> **Date:** 2026-05-26
> **Goal:** Enable the QCA9558 on-chip radio as a dedicated spectral scanner
> under AREDN/OpenWrt, submit upstream driver patches.
> **Method:** Clean-room behavioral observation + open-source driver work.

## Discovery

The Rocket 5AC Lite (stock Ubiquiti firmware) uses its QCA9560 SoC on-chip radio
(`wifi1`, AHB bus) as a dedicated spectral scanner for AirView, while the PCI radio
(`wifi0`) serves the AP. See `AIRVIEW_ARCHITECTURE_FINDINGS.md`.

The PBE-5AC-500 has the same SoC family (**QCA9558**) with the same on-chip radio,
but under AREDN/OpenWrt the WMAC is **disabled in the device tree** and never
probed. The hardware is present and has valid calibration data.

## Current State on PBE-5AC-500 (AREDN)

### What exists

```
Device tree node:  /ahb/wmac@18100000
  compatible:      qca,qca9550-wmac
  reg:             0x18100000, size 0x00010000
  status:          disabled            ← THIS IS THE GATE
```

```
ART partition (mtd7):
  0x0000:  ath10k PCI radio caldata (MAC f4:92:bf:bd:8a:de)
  0x5000:  ath9k WMAC caldata (MAC f4:92:bf:bc:8a:de) ← VALID
           Header: 44 08 7f 34, board: CUS223-720-S0849
```

```
Kernel modules loaded:
  ath9k          114688  0    ← loaded, refcount 0 (no device bound)
  ath9k_hw       360448  2
  ath9k_common    32768  1
```

```
Platform drivers registered:
  /sys/bus/platform/drivers/ath9k/    ← driver present, no device bound
  /sys/bus/platform/drivers/ath10k_ahb/ ← also present, not bound
```

### What's missing

- The DT `status = "disabled"` prevents the WMAC platform device from being
  created, so the ath9k driver never probes it.
- No `phy1` appears in `/sys/class/ieee80211/`.
- No second wireless interface is created.

### Why it's disabled

The AREDN/OpenWrt DTS for `ubnt,powerbeam-5ac-500` sets the WMAC to `disabled`
because:
1. Ubiquiti's stock firmware manages this radio through its proprietary driver
   stack, not through ath9k.
2. The PBE-5AC-500 product doesn't externally expose or antenna-connect the
   on-chip radio — it's physically present on the SoC but the RF path may go
   to an internal test point or may share antenna paths.
3. OpenWrt conservatively disables hardware that isn't validated for the platform.

## What Enabling Would Require

### Phase 1: Device Tree — Enable WMAC probe

The upstream OpenWrt DTS chain is:

```
qca9558_ubnt_powerbeam-5ac-500.dts
  └─ includes qca955x_ubnt_xc.dtsi
       └─ includes qca955x.dtsi (defines wmac@18100000, status="disabled")
```

Neither the PBE-5AC-500 DTS nor the `ubnt_xc` include ever enables `&wmac`.
The caldata nvmem cell `cal_art_5000` (ART offset 0x5000, size 0x844) is
already defined in the `ubnt_xc` include but is currently fed only to the
ath10k PCI radio.

The fix: add a WMAC caldata cell and enable the node. Reference pattern from
`qca9558_tplink_archer-c7-v1.dts` which uses the same SoC:

```dts
/* In qca955x_ubnt_xc.dtsi or the PBE-5AC-500 DTS overlay: */

/* Add WMAC caldata cell to ART partition nvmem-layout */
cal_art_1000: calibration@1000 {
    reg = <0x1000 0x440>;
};

&wmac {
    status = "okay";
    nvmem-cells = <&cal_art_1000>;
    nvmem-cell-names = "calibration";
};
```

**Note on caldata offset:** The ART partition layout needs verification.
On the PBE-5AC-500 we observed:
- `0x0000`: Board MAC data (6 bytes, used by eth0)
- `0x1000`: All 0xFF (possibly unused, or ath10k OTP)
- `0x5000`: ath9k WMAC caldata (header `44 08`, board `CUS223-720-S0849`)

The `ubnt_xc` include currently assigns `cal_art_5000` (0x5000, 0x844) to
the ath10k PCI radio, which seems wrong for ath10k caldata. This may work
because the ath10k-ct firmware extracts what it needs from the OTP region
elsewhere. The WMAC caldata at 0x5000 may need to be referenced separately
for ath9k. Testing will clarify the correct offset.

This is a well-understood pattern — many QCA955x OpenWrt targets enable
the WMAC this way (Archer C7 v1, LibreRouter v1, Sophos AP100, etc.).

**Risk:** The on-chip radio's RF path on the PBE-5AC-500 PCB is unknown.
It may have no antenna connection, a shared/switched antenna, or an internal
test point only. Enabling the radio with no antenna path would be harmless for
spectral *receive*, but TX must be disabled or zero-power.

### Phase 2: ath9k Spectral Scan on the WMAC

Once ath9k probes the WMAC and creates `phy1`/`wlan1`:

1. **Verify spectral scan support:**
   ```
   cat /sys/kernel/debug/ieee80211/phy1/ath9k/spectral_scan_ctl
   ```
   ath9k has upstream spectral scan support for AR9003+ chips. The QCA9558
   WMAC is AR9550-based (AR9003 family), so spectral should work.

2. **Create a monitor VAP:**
   ```
   iw phy phy1 interface add mon1 type monitor
   ip link set mon1 up
   ```

3. **Trigger spectral scan:**
   ```
   echo background > /sys/kernel/debug/ieee80211/phy1/ath9k/spectral_scan_ctl
   iw dev mon1 set channel 36 HT20
   cat /sys/kernel/debug/ieee80211/phy1/ath9k/spectral_scan0 > /tmp/fft.bin
   ```

4. **Verify FFT data is produced.**

### Phase 3: Channel Sweep for Wideband Coverage

With the WMAC running as a dedicated scanner (no AP duty):

1. Cycle through channels on `phy1` while collecting spectral FFT data on each.
2. Stitch per-channel FFT bins into a wideband display — same approach as
   Ubiquiti's `ubntspecd` but using the upstream ath9k spectral debugfs path.
3. The production radio (`phy0` / `wlan0` / ath10k) is never touched.

### Phase 4: Upstream Patches

Potential upstream contributions:

| Target | Change | Scope |
|--------|--------|-------|
| OpenWrt DTS | Enable WMAC on `ubnt,powerbeam-5ac-500` (and similar) | Device tree |
| OpenWrt DTS | Add caldata nvmem reference for ART 0x5000 | Device tree |
| ath9k | Verify/fix spectral scan on QCA9558 WMAC | Driver (if needed) |
| ath9k | Spectral scan channel-sweep helper | New feature |
| AREDN | RFeye integration: use WMAC as scanner when available | Application |

## Comparison: R5AC-Lite (stock) vs PBE-5AC-500 (target)

| Aspect | R5AC-Lite (Ubiquiti) | PBE-5AC-500 (target) |
|--------|---------------------|---------------------|
| SoC | QCA9560 | QCA9558 |
| On-chip radio | AHB, active as `wifi1` | AHB, present but `disabled` in DT |
| On-chip driver | Vendor `ath_hal`/`umac` | Open `ath9k` (not probed) |
| Caldata | EEPROM mtd5 @ 0x5000 | ART mtd7 @ 0x5000 |
| Scanner daemon | `ubntspecd` (proprietary) | RFeye (to be built, open) |
| Spectral path | `ath_spectral` → netlink | `ath9k` → debugfs `spectral_scan0` |
| FFT delivery | netlink → JSON/WebSocket | debugfs → TLV parse → JSON/WebSocket |
| Production radio | `wifi0` (PCI, vendor) | `wlan0` (PCI, ath10k) |
| AP disruption | None | None (if WMAC works) |

## Immediate Next Steps

1. ~~Find the OpenWrt DTS file for `ubnt,powerbeam-5ac-500`.~~ **Done.**
   Chain: `qca9558_ubnt_powerbeam-5ac-500.dts` → `qca955x_ubnt_xc.dtsi` → `qca955x.dtsi`.
   WMAC node `wmac@18100000` exists but `status = "disabled"`.

2. ~~Check if other QCA955x targets enable the WMAC.~~ **Done.**
   TP-Link Archer C7 v1 (`qca9558_tplink_archer-c7-v1.dts`) enables `&wmac`
   with `nvmem-cells` caldata reference. Same SoC, proven pattern.

3. **Build a test firmware** with `status = "okay"` on the WMAC node (bench
   hardware only). Requires an AREDN or OpenWrt build environment.

4. **Verify ath9k probe** — does `phy1` appear? Does spectral debugfs
   (`/sys/kernel/debug/ieee80211/phy1/ath9k/spectral_scan_ctl`) appear?

5. **RF path investigation** — does the on-chip radio receive anything useful
   without an external antenna? Even with poor sensitivity, spectral scanning
   of strong signals may work for interference detection.

6. **Determine correct ART caldata offset** for the WMAC on this board.
   Observed: 0x5000 contains valid ath9k caldata (header `44 08`). But the
   existing DTS assigns 0x5000 to ath10k. Test both 0x1000 and 0x5000.

7. **If ath9k spectral works on the WMAC:** build a channel-sweep scanner
   using the upstream `spectral_scan0` debugfs interface.

8. **Upstream submission plan:**
   - DTS patch: enable WMAC on `ubnt,powerbeam-5ac-500` (and similar `ubnt_xc` boards)
   - ath9k patch: any spectral scan fixes needed for QCA9558 WMAC (if any)
   - Document RF path findings for the board

## Safety Rules

- **Bench hardware only** — do not deploy on production AREDN nodes until
  validated.
- **TX must be disabled** on the WMAC — it has no known antenna path.
- **Do not copy or decompile** Ubiquiti proprietary code.
- **Clean-room only** — behavioral observation of stock firmware informs *what*
  to build, not *how* to build it. Implementation uses the open ath9k driver
  and upstream kernel spectral scan API.
- **Production radio untouched** — the ath10k AP radio is never retuned or
  disrupted.
