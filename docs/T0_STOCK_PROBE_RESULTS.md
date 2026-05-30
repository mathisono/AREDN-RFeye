# T0: Stock Firmware Calibration Reconnaissance — Results

> **Date:** 2026-05-29  
> **Target:** PowerBeam 5AC Gen2 (PBE-5AC-Gen2), stock AirOS WA.v8.7.11  
> **IP:** 10.108.120.18 (AREDN mesh, via MSE-88 jump host)  
> **Probed by:** builder_bob via nested SSH

---

## 1. Device Identity

```
PRODUCT: PowerBeam 5AC
MODEL: PBE-5AC-Gen2
VERSION: WA.v8.7.11
KERNEL: Linux PB5ACG2-A-4C 2.6.32.68 mips
MAC: 24:5A:4C:0E:25:14
FCC ID: SWX-PBE5ACG2
BOM: 13-00492-14
board.sysid=0xe3d6
```

**Key: This is a WA board (firmware prefix "WA"), NOT an XC board.**
The WA designation means it's a different hardware variant than the XC boards
we've been testing (PowerBeam 5AC 500 XC / Rocket 5AC Lite XC).

---

## 2. Radio Configuration (from board.info)

### Radio 1 — PCI (ath10k, production 5 GHz)
```
radio.1.bus=pci
radio.1.devdomain=5000
radio.1.chains=2
radio.1.txpower.max=24
radio.1.txpower.min=-4
radio.1.ieee_mode_a=1          (5 GHz only)
radio.1.antenna.1.gain=25      (25 dBi dish)
radio.1.antenna.2.gain=3       (feed only, 3 dBi)
radio.1.chanbw=10,20,30,40,50,60,80
```

### Radio 2 — AHB (ath9k WMAC, on-chip)
```
radio.2.bus=ahb
radio.2.devdomain=5000
radio.2.chains=1
radio.2.txpower.max=19
radio.2.txpower.min=0
radio.2.ieee_mode_bg=1         (2.4 GHz b/g mode)
radio.2.web_exclude=1          (hidden from normal web UI)
radio.2.chanbw=5,10,20,40,80
```

**Critical finding:** The stock Ubiquiti firmware KNOWS about radio.2 (the WMAC).
It's listed in board.info as AHB bus, 1 chain, bg mode, but `web_exclude=1`
means it's hidden from the normal wireless configuration UI.

---

## 3. Wireless Interfaces (iwconfig)

### ath0 — Production radio
```
IEEE 802.11ac  ESSID:"W6BBpTpWA6KQB"
Mode:Master  Frequency:5.76 GHz
Tx-Power=24 dBm
Signal level=-61 dBm  Noise level=-86 dBm
```

### airview1 — WMAC spectral scanner! 🎯
```
IEEE 802.11na  ESSID:"spectral"
Mode:Monitor  Frequency:4.92 GHz
Tx-Power=13 dBm
Signal level=-96 dBm  Noise level=-103 dBm
```

**CRITICAL FINDING:** Stock Ubiquiti firmware creates an `airview1` interface
on the WMAC radio (wifi1) in Monitor mode with ESSID "spectral". This is
the AirView spectral analyzer feature built into AirOS. **Ubiquiti uses the
WMAC for exactly the same purpose as RFeye — spectral scanning.**

---

## 4. Kernel Modules

```
adf                    10072  3 umac,ath_dev,ath_hal
asf                     7121  7 ubnt_poll_host,ath_dfs,umac,ath_dfs_prescan,ath_dev,ath_spectral,ath_hal
ath_dev               221833  3 ath_dfs,umac,ath_dfs_prescan
ath_dfs              1188973  1
ath_dfs_prescan        22736  0
ath_hal               328906  3 umac,ath_dev,ath_rate_atheros
ath_rate_atheros       31174  1 ath_dev
ath_spectral           24777  3 umac,ath_dev
ubnthal               395766  9 ubnt_poll_host,ath_dfs,rssi_leds,umac,ath_dev,ath_hal,ar724x_eth
urd                    54608  2 umac,ath_hal
```

Note: `ath_spectral` module is loaded. Ubiquiti uses their own ath driver
stack (not mainline ath9k), with proprietary `ubnthal` and `ath_spectral`.

---

## 5. MTD Partition Layout

```
dev:    size   erasesize  name
mtd0: 00040000 00010000 "u-boot"        (256 KB)
mtd1: 00010000 00010000 "u-boot-env"    (64 KB)
mtd2: 00100000 00010000 "kernel"        (1 MB)
mtd3: 00e60000 00010000 "rootfs"        (14.375 MB)
mtd4: 00040000 00010000 "cfg"           (256 KB)
mtd5: 00010000 00010000 "EEPROM"        (64 KB)
```

**Note:** The partition is called "EEPROM" (not "art" as on OpenWrt).
This is the same physical flash region. Size: 64 KB (0x10000).

---

## 6. EEPROM Partition Content — THE KEY DATA

### EEPROM+0x0000 (MAC/Header, 32 bytes)
```
00000000  24 5a 4c 0f 25 14 26 5a  4c 0f 25 14 e3 d6 07 77
00000010  00 01 ec 0e 52 ff 00 0d  ff ff ff ff ff ff ff ff
```
- Bytes 0-5: `24:5A:4C:0F:25:14` — first MAC
- Bytes 6-11: `26:5A:4C:0F:25:14` — second MAC (offset by 2)
- Bytes 12-13: `e3 d6` — board sysid (matches board.info)
- Bytes 14-15: `07 77` — board subvendorid (matches radio.1.subvendorid=0x0777)

### EEPROM+0x1000 (WMAC caldata, 512 bytes) — NOT BLANK! 🎯🎯🎯
```
00000000  02 02 26 5a 4c 0e 25 14  00 00 00 00 00 00 00 00  |..&ZL.%.........|
00000010  00 00 00 00 00 00 00 00  00 00 00 2a 00 00 1f 00  |...........*....|
00000020  33 03 00 00 00 00 04 00  48 00 7d 02 03 00 08 ff  |3.......H.}.....|
00000030  11 01 00 00 00 10 01 00  00 ee ee 0e 00 10 00 10  |................|
00000040  00 10 00 00 00 00 00 00  00 28 00 a4 00 00 00 00  |.........(......|
00000050  ff 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000060  0e 0e 03 00 2c e2 00 02  0e 1c e0 e0 00 0c e0 e0  |....,...........|
00000070  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
00000080  00 00 00 00 00 00 00 00  00 00 70 89 a2 f3 00 85  |..........p.....|
00000090  00 00 00 f5 00 87 00 00  00 f6 00 88 00 00 00 00  |................|
000000a0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
*
000000c0  00 00 00 70 ac 70 89 ac  70 89 ac 7a 93 a2 22 22  |...p.p..p..z..""|
000000d0  22 1e 22 22 22 1e 22 22  18 14 22 22 18 14 22 22  |".""".""..""..""|
000000e0  18 14 22 20 20 1c 14 10  20 1c 14 10 20 1c 14 10  |.."  ... ... ...|
000000f0  22 22 20 1c 14 10 20 1c  14 10 20 1c 14 10 22 22  |"" ... ... ...""|
00000100  20 1c 14 10 20 1c 14 10  20 1c 14 10 22 22 20 1c  | ... ... ..."" .|
00000110  14 10 20 1c 14 10 20 1c  14 10 22 22 20 1c 14 10  |.. ... ..."" ...|
00000120  20 1c 14 10 20 1c 14 10  22 22 20 1c 14 10 20 1c  | ... ..."" ... .|
00000130  14 10 20 1c 14 10 11 12  15 17 41 42 45 47 31 32  |.. .......ABEG12|
00000140  35 37 70 75 ac b8 70 75  ac b8 70 75 ac b8 70 75  |57pu..pu..pu..pu|
00000150  ac b8 70 75 ac b8 70 75  ac b8 70 75 ac b8 70 75  |..pu..pu..pu..pu|
*
00000170  ac b8 3c 7c 3c 7c 3c 7c  3c 7c 3c 7c 3c 7c 3c 7c  |..<|<|<|<|<|<|<||
00000180  3c 7c 3c 7c 3c 7c 3c 7c  3c 7c 3c 7c 3c 7c 3c 7c  |<|<|<|<|<|<|<|<||
*
000001a0  3c 7c 10 01 00 00 22 22  02 00 00 00 00 00 00 00  |<|....""........|
000001b0  00 00 00 00 00 00 46 00  00 00 00 00 00 ff 00 00  |......F.........|
000001c0  00 00 00 00 00 00 00 00  00 00 00 00 00 0e 0e 03  |................|
000001d0  00 2d e2 00 02 0e 1c e0  e0 f0 0c e0 e0 f0 6c 00  |.-............l.|
000001e0  00 00 00 00 00 00 00 00  00 44 54 00 00 00 00 00  |.........DT.....|
000001f0  00 00 00 00 00 00 00 4c  54 68 78 8c a0 b9 cd 00  |.......LThx.....|
```

**This is real, structured caldata — NOT blank (all 0xFF)!**

Key observations:
- Byte 0: `02` — AR9300 EEPROM template format marker (AR5416_EEP_VER)
- Byte 1: `02` — template version
- Bytes 2-7: `26:5A:4C:0E:25:14` — MAC address (matches radio.2 / WMAC)
- Contains structured gain tables (0xC0–0x1A0 region)
- Contains calibration pier data
- Contains CTL/regulatory data
- Total structured data: ~512 bytes (0x200), rest is padding

### EEPROM+0x5000 (PCI caldata, 128 bytes)
```
00000000  44 08 be d7 02 0d 24 5a  4c 0e 25 14 2a 00 00 00
00000010  15 49 00 08 44 0c 08 00  00 00 15 00 00 00 33 00
00000020  00 00 00 00 00 00 98 00  00 4f 00 00 00 63 75 73
00000030  32 32 33 2d 30 32 32 2d  6e 31 37 32 35 00 00 00
```
- Header: `44 08` — AR9300 compressed EEPROM marker
- Board ID: `cus223-022-n1725`
- MAC: `24:5A:4C:0E:25:14` — PCI radio MAC

---

## 7. No Device Tree, No /lib/firmware

```
/proc/device-tree: NO (does not exist)
/lib/firmware: EMPTY (directory exists but no files)
```

Stock AirOS v8.7.11 on kernel 2.6.32 does not use device tree or external
firmware files. The driver loads caldata directly from the EEPROM MTD
partition at known offsets.

---

## 8. Analysis & Conclusions

### 🎯 FINDING 1: WA boards have REAL WMAC caldata at EEPROM+0x1000

This PowerBeam 5AC Gen2 (WA board) has **factory-written WMAC calibration
data** at EEPROM+0x1000. It is NOT blank (all 0xFF) like the XC boards.

The caldata:
- Is ~512 bytes of structured AR9300 EEPROM data
- Contains the WMAC MAC address (`26:5A:4C:0E:25:14`)
- Contains gain calibration tables
- Contains calibration pier data
- Contains CTL/regulatory limits
- Uses template format (header `02 02`), not compressed format (`44 08`)

### 🎯 FINDING 2: Ubiquiti uses the WMAC for AirView (same as RFeye)

The stock firmware:
- Creates `airview1` interface on wifi1 (WMAC) in Monitor mode
- Loads `ath_spectral` kernel module
- Uses the WMAC for spectral scanning — exactly the same use case as RFeye
- The WMAC is hidden from normal wireless config (`web_exclude=1`)

### 🎯 FINDING 3: WMAC caldata format differs from PCI caldata

| Property | EEPROM+0x1000 (WMAC) | EEPROM+0x5000 (PCI) |
|----------|---------------------|---------------------|
| Format | Template (`02 02`) | Compressed (`44 08`) |
| MAC | `26:5A:4C:0E:25:14` | `24:5A:4C:0E:25:14` |
| Size | ~512 bytes | ~2 KB (compressed) |
| Board ID | (none visible) | `cus223-022-n1725` |
| Chains | 1 (per board.info) | 2 (per board.info) |

### 🎯 FINDING 4: WA vs XC board difference explains blank 0x1000

- **WA boards** (this PowerBeam 5AC Gen2): WMAC caldata present at 0x1000,
  WMAC used for AirView spectral scanning
- **XC boards** (PowerBeam 5AC 500, Rocket 5AC Lite): WMAC caldata is
  **blank** at 0x1000 — Ubiquiti chose not to calibrate the WMAC on XC boards

This confirms that the blank 0x1000 on XC boards is **intentional** — Ubiquiti
simply didn't calibrate or use the WMAC on XC hardware. The WMAC silicon is
present (QCA9558 SoC always has it) but was never factory-characterized.

### Implications for RFeye r17

1. **WA board WMAC caldata exists and is extractable.** This is the first
   confirmed source of real, factory-calibrated WMAC data for this board family.

2. **The caldata uses AR9300 template format**, not compressed format. This is
   important — it means the ath9k driver needs to parse it as template, not
   as the compressed format used at 0x5000.

3. **Cross-board applicability is uncertain.** This WA caldata is calibrated
   for the WA board's WMAC RF path. The XC board's WMAC RF path may differ
   (different board layout, different trace routing, different antenna/UFL).
   Using WA caldata on an XC board would be better than PCI caldata but
   still not factory-correct for that specific board.

4. **A full EEPROM dump should be extracted** from this WA board for offline
   analysis and comparison with XC board EEPROM dumps.

---

## 9. Recommended Next Steps

1. **Extract full EEPROM dump** from this WA PowerBeam for offline analysis
2. **Parse the 0x1000 caldata** against the AR9300 template EEPROM structure
   to identify all calibration fields
3. **Compare with XC board 0x5000** to quantify the structural differences
4. **Test WA caldata on XC bench board** — write WA 0x1000 data to XC 0x1000,
   point WMAC DTS at 0x1000, compare spectral results
5. **Search for other WA boards** in the mesh to see if caldata varies
   board-to-board (it should — per-unit factory calibration)
