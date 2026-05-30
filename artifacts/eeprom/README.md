# EEPROM Dumps

## eeprom_wa_pbe5ac_gen2.bin

Full 64 KB EEPROM partition dump from a stock Ubiquiti PowerBeam 5AC Gen2.

| Field | Value |
|-------|-------|
| **Source** | PowerBeam 5AC Gen2 (PBE-5AC-Gen2) |
| **Board type** | WA |
| **Firmware** | AirOS WA.v8.7.11 |
| **FCC ID** | SWX-PBE5ACG2 |
| **BOM** | 13-00492-14 |
| **SysID** | 0xe3d6 |
| **MTD partition** | mtd5 "EEPROM" (64 KB) |
| **Extracted** | 2026-05-29 via `dd if=/dev/mtdblock5 bs=65536 count=1` |
| **MD5** | `4c34965022dc0c45dcd595feec100975` |
| **SHA256** | `825d652b92c82c914ba557f78ca46243f5c1a9660fbbca3b19bd89d4ccb94c88` |

### Partition Layout

| Offset | Size | Content |
|--------|------|---------|
| 0x0000 | 32 bytes | Header: MAC1 `24:5a:4c:0f:25:14`, MAC2 `26:5a:4c:0f:25:14`, SysID `0xe3d6`, SubVendor `0x0777` |
| 0x0020–0x0FFF | 4064 bytes | All `0xFF` (padding) |
| **0x1000** | **1024 bytes** | **WMAC caldata** — AR9300 template format (`02 02`), MAC `26:5a:4c:0e:25:14`, 1021/1024 bytes non-0xFF |
| 0x1400–0x4FFF | 15360 bytes | Mostly `0xFF` (64 non-0xFF bytes — unknown) |
| **0x5000** | **2116 bytes** | **PCI caldata** — AR9300 compressed format (`44 08`), MAC `24:5a:4c:0e:25:14`, Board ID `cus223-022-n1725` |
| 0x5A00–0xFFFF | 42496 bytes | 12214 non-0xFF bytes (likely board config, signatures, or extended cal) |

### Key Findings

- **WMAC caldata at 0x1000 is REAL** — 1024 bytes of structured AR9300 template EEPROM, factory-written per-unit data
- **Different format than PCI caldata** — template (`02 02`) vs. compressed (`44 08`)
- **Different MACs** — WMAC `26:5a:4c:0e:25:14` vs. PCI `24:5a:4c:0e:25:14` (offset by 2 in first octet)
- **XC boards have blank (all 0xFF) 0x1000** — this WA board confirms Ubiquiti does calibrate the WMAC when they intend to use it

### Context

This dump was extracted as part of the T0 stock firmware reconnaissance
(see `docs/T0_STOCK_PROBE_RESULTS.md`) to determine whether Ubiquiti ships
WMAC calibration data on any board in the XC/WA family.

The WMAC on this WA board is used by Ubiquiti's AirView spectral analyzer
(`airview1` interface, monitor mode, `ath_spectral` module). This is the
same use case as RFeye's wideband spectral scanner.
