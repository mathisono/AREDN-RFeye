# ATH9K Calibration Data: Kernel Source Analysis

> **Date:** 2026-05-28  
> **Context:** Rocket 5AC Lite XC — AR9342 on-chip WMAC (ath9k, AHB bus)  
> **Kernel:** Linux 6.6.73 (OpenWrt 24.10)  
> **Source:** `drivers/net/wireless/ath/ath9k/ar9003_eeprom.c` and related files

---

## 1. Executive Summary

The AR9342 on-chip WMAC on Rocket 5AC Lite (XC) boards has **no factory
calibration data** — the ART partition at offset 0x1000 is blank (all 0xFF),
and the SoC OTP is empty. Ubiquiti's stock proprietary driver (`ath_hal`)
handles this by falling back to a **built-in default template**. The mainline
Linux `ath9k` driver has the same template data compiled in, but its error
handling **does not fall back gracefully** — it returns failure when both
EEPROM and OTP reads fail, causing radio init to abort.

This document traces the exact code path and explains why:
- Feeding **all-0xFF** (blank ART) → radio init fails
- Feeding **all-zeros** → radio init fails
- Feeding **random bytes** → works ~50% of the time (dangerous)
- Feeding **0x5000 PCI caldata** → works (wrong radio, but structurally valid)
- Feeding **no caldata** (no DTS reference) → radio init fails (AHB stub)

---

## 2. Stock Firmware Behavior (Reference)

### Investigation Method

SSH to stock Rocket 5AC Lite at 192.168.3.238, running Ubiquiti firmware
(Linux 2.6.32.68, proprietary `ath_hal` / `ath_ahb` drivers).

### Key Findings

```
Board:   Rocket 5AC Lite (sysid 0xe1f5, model R5AC-Lite)
SoC:     QCA9558 (AR9342-compatible WMAC)
Kernel:  2.6.32.68 (no device tree — uses board files)
```

**Two radios defined in `/etc/board.info`:**

| Property | Radio 1 (wifi0/ath0) | Radio 2 (wifi1/airview1) |
|----------|---------------------|------------------------|
| Chip | QCA9882 | AR9342 (SoC built-in) |
| Bus | **PCI** | **AHB** |
| Driver | `ath_pci` (proprietary) | `ath_ahb` (proprietary) |
| Purpose | 5GHz main radio | Spectrum analyzer (airview) |
| MAC | 04:18:D6:A4:71:C1 (real) | **00:02:03:04:05:06** (template!) |
| TX power | 27 dBm max | 19 dBm max |
| Modes | 802.11ac | 802.11na |
| Memory | PCI BAR 0x10000000 | AHB 0xb8100000 |

**EEPROM partition (mtd5, "EEPROM", 64KB):**

| Offset | Content |
|--------|---------|
| 0x0000 | MAC addresses + subsystem IDs (18 bytes real data, rest 0xFF) |
| 0x1000 | **All 0xFF** — no WMAC caldata |
| 0x5000 | QCA9882 PCI radio factory calibration |

**Critical dmesg line:**
```
ar9300_eeprom_restore_internal[4673] No vaid CAL, calling default template
```

**Confirmed:** wifi1 uses the dummy MAC `00:02:03:04:05:06` from the default
template, proving no real calibration data exists. The proprietary `ath_hal`
successfully falls back to the built-in template and the radio operates
(in monitor mode only, for the airview spectrum analyzer feature).

**No calibration files anywhere:**
- `/lib/firmware/` — empty
- `/tmp/` — no caldata extracted at runtime
- EEPROM partition unchanged after airview starts

---

## 3. Mainline ath9k Code Path Analysis

### 3.1 Data Flow Overview

```
DTS caldata reference (nvmem cell)
    ↓
ath9k_nvmem_request_eeprom()     [init.c:572]
    → devm_nvmem_cell_get(sc->dev, "calibration")
    → nvmem_cell_read(cell, &len)
    → ah->nvmem_blob = data
    → ah->ah_flags &= ~AH_USE_EEPROM    (sets use_flash = true)
    ↓
ath9k_hw_ar9300_fill_eeprom()    [ar9003_eeprom.c:3391]
    → ar9300_eeprom_restore_internal(ah, mptr, size)
        ↓
    [if use_flash]
        → ar9300_eeprom_restore_flash()  — reads nvmem_blob
        → check txrxMask: valid? → return 0 (success)
        → invalid (0x00 or 0xFF)? → fall through
    [always]
        → memcpy(mptr, &ar9300_default, size)  — LOAD DEFAULT TEMPLATE
        → Try EEPROM reads at 3 addresses (via nvram_read → nvmem_blob)
        → Try OTP reads at 2 addresses (direct hardware)
        → All fail? → goto fail → return -1
    ↓
fill_eeprom checks return:
    >= 0 → return true (radio init continues)
    < 0  → return false (RADIO INIT FAILS)
```

### 3.2 The txrxMask Validity Check

From `ar9300_eeprom_restore_internal()` (ar9003_eeprom.c line ~3282):

```c
if (ath9k_hw_use_flash(ah)) {
    u8 txrx;

    if (ar9300_eeprom_restore_flash(ah, mptr, mdata_size))
        return -EIO;

    /* check if eeprom contains valid data */
    eep = (struct ar9300_eeprom *) mptr;
    txrx = eep->baseEepHeader.txrxMask;
    if (txrx != 0 && txrx != 0xff)
        return 0;   // ← VALID: radio init succeeds with this data
}
// Falls through here if txrxMask is 0x00 or 0xFF (invalid)
```

This is the **gatekeeper**. It determines whether the flash/nvmem data
is accepted or rejected:

| txrxMask value | Condition `!= 0 && != 0xff` | Result |
|---------------|----------------------------|--------|
| `0x00` | FALSE | **Rejected** — falls through |
| `0xFF` | FALSE | **Rejected** — falls through |
| `0x77` | TRUE | **Accepted** — radio inits with this data |
| `0x33` | TRUE | **Accepted** — radio inits with this data |
| Any other | TRUE (~99.2%) | **Accepted** — radio inits with this data |

**This is why random bytes work ~50% of the time** — more precisely ~99.2%
of the time the txrxMask byte won't be 0x00 or 0xFF. But even when txrxMask
passes, the rest of the random data produces nonsensical calibration values
(wrong power levels, wrong frequencies, wrong gain corrections). The radio
"initializes" but operates with garbage parameters.

### 3.3 The Default Template

After the flash data is rejected, the function loads a built-in default:

```c
memcpy(mptr, &ar9300_default, mdata_size);
```

The `ar9300_default` template (ar9003_eeprom.c line 46) contains:

```c
static const struct ar9300_eeprom ar9300_default = {
    .eepromVersion = 2,
    .templateVersion = 2,
    .macAddr = {0, 2, 3, 4, 5, 6},    // ← matches stock wifi1!
    .baseEepHeader = {
        .txrxMask = 0x77,              // 3x3 TX/RX
        .opCapFlags.opFlags = AR5416_OPFLAGS_11G | AR5416_OPFLAGS_11A,
        .featureEnable = 0x0c,         // fastClock + doubling
        ...
    },
    // ... full 2G + 5G modal headers, cal piers, power tables, CTLs
};
```

This template is a **complete, structurally valid** AR9300 EEPROM with
conservative power settings. The stock Ubiquiti `ath_hal` uses an equivalent
template — confirmed by the matching dummy MAC `00:02:03:04:05:06`.

### 3.4 Post-Template: EEPROM and OTP Attempts

After loading the default template, the driver tries to find compressed
EEPROM data blocks that would override (patch) the template:

```c
read = ar9300_read_eeprom;  // reads via ath9k_hw_nvram_read()

// Try 3 EEPROM base addresses
cptr = AR9300_BASE_ADDR;     // 0x3ff
if (ar9300_check_eeprom_header(ah, read, cptr)) goto found;
cptr = AR9300_BASE_ADDR_4K;  // 0xfff
if (ar9300_check_eeprom_header(ah, read, cptr)) goto found;
cptr = AR9300_BASE_ADDR_512; // 0x1ff
if (ar9300_check_eeprom_header(ah, read, cptr)) goto found;

// Try 2 OTP addresses (direct hardware read, bypasses nvmem_blob)
read = ar9300_read_otp;
cptr = AR9300_BASE_ADDR;     // 0x3ff
if (ar9300_check_eeprom_header(ah, read, cptr)) goto found;
cptr = AR9300_BASE_ADDR_512; // 0x1ff
if (ar9300_check_eeprom_header(ah, read, cptr)) goto found;

goto fail;  // ← ALL FAILED
```

For our case:
- **EEPROM reads**: `ar9300_read_eeprom` → `ath9k_hw_nvram_read()` →
  reads from `ah->nvmem_blob` (all 0xFF) → no valid headers found
- **OTP reads**: `ar9300_read_otp` → `ar9300_otp_read_word()` → reads
  from hardware OTP registers at `AR9300_OTP_BASE(ah)` → **OTP is empty**
  (confirmed by stock firmware "No vaid CAL" message)

Result: `goto fail` → `kfree(word); return -1;`

### 3.5 The AHB Eeprom Stub

Separately from the nvmem path, the AHB bus driver has a hard-coded stub
(ahb.c line 60):

```c
static bool ath_ahb_eeprom_read(struct ath_common *common, u32 off, u16 *data)
{
    ath_err(common, "%s: eeprom data has to be provided externally\n",
            __func__);
    return false;
}
```

This means if no nvmem cell is defined in the DTS, `ath9k_hw_nvram_read()`
falls through to this stub, which **always fails**. AHB devices cannot
discover calibration data on their own — it MUST be provided via DTS/nvmem,
platform_data, or firmware blob.

### 3.6 The Difference: Proprietary ath_hal vs. Mainline ath9k

| Behavior | Ubiquiti ath_hal | Mainline ath9k |
|----------|-----------------|----------------|
| Load default template | ✅ Yes | ✅ Yes |
| Try EEPROM/OTP | ✅ Yes | ✅ Yes |
| On all-fail: use template anyway | ✅ **Yes** | ❌ **No — returns -1** |
| Radio works with template only | ✅ Yes (monitor mode) | ❌ Init fails |

The proprietary driver treats the default template as a valid fallback.
The mainline driver treats failure to find EEPROM/OTP data as a hard error,
even though the template is already loaded in memory. **This is the root
cause of the calibration problem.**

---

## 4. Outcome Matrix: What Happens With Each Caldata Option

| Caldata Source | txrxMask | Flash Check | EEPROM/OTP | Final Result | Quality |
|---------------|----------|-------------|------------|--------------|---------|
| **ART 0x1000** (all 0xFF) | 0xFF | Rejected | Fail | ❌ **Init fails** | N/A |
| **All zeros** | 0x00 | Rejected | Fail | ❌ **Init fails** | N/A |
| **No DTS reference** | N/A | Skipped | AHB stub fails | ❌ **Init fails** | N/A |
| **Random bytes** | Random | ~99.2% accepted | N/A | ⚠️ **Works with garbage** | Dangerous |
| **ART 0x5000** (PCI caldata) | 0x77 | Accepted | N/A | ✅ **Works** | Wrong radio's cal |
| **Default template** (if allowed) | 0x77 | N/A | N/A | ✅ Would work | Generic/conservative |
| **Proper WMAC caldata** | Valid | Accepted | N/A | ✅ **Works** | Correct |

---

## 5. Why Mainline ath9k Differs from Proprietary ath_hal

In Ubiquiti's proprietary `ath_hal` (and the FreeBSD HAL port), there is
a massive array of hardcoded fallback templates (e.g., `ar9300template_generic`,
`ar9300template_ap121`, etc.). If the HAL reads a blank or corrupted EEPROM,
it simply loads one of these default templates into memory and carries on.

When Atheros/Qualcomm released ath9k to the upstream Linux kernel, they
**intentionally stripped out most of these hardcoded template arrays** to
reduce source code bloat. Upstream ath9k retains a single `ar9300_default`
template (with MAC `00:02:03:04:05:06`) that gets loaded into memory during
the restore process, but **assumes that if a radio is present, valid
calibration data will be provided by the system** — either via flash,
OTP, device tree nvmem cells, or the firmware loader.

When you point mainline ath9k at an ART partition filled with 0xFF:

1. `ar9300_eeprom_restore_flash()` reads the data via `nvram_read()`
2. It checks `txrxMask` — `0xFF` is treated as invalid (same as `0x00`)
3. Falls through; default template is loaded into memory
4. EEPROM read attempts against the nvmem blob all fail (all 0xFF, no headers)
5. OTP reads from hardware fail (OTP is empty on these boards)
6. `goto fail; return -1;` — **radio init aborts**

The proprietary `ath_hal` would accept the template and continue. Mainline
ath9k treats the failure as fatal.

---

## 6. Available Solutions

### 6.1 Firmware Fallback via `qca,no-eeprom` (RECOMMENDED)

When the DTS WMAC node includes the `qca,no-eeprom` property and does
**not** bind to any ART partition caldata, ath9k falls back to the Linux
firmware loader (`request_firmware()`). From `init.c` line ~680:

```c
if (of_property_read_bool(np, "qca,no-eeprom")) {
    scnprintf(eeprom_name, sizeof(eeprom_name),
              "ath9k-eeprom-%s-%s.bin",
              ath_bus_type_to_string(bus_type), dev_name(ah->dev));
    ret = ath9k_eeprom_request(sc, eeprom_name);
    ...
}
```

For the QCA9558 WMAC at AHB address `0x18100000`, the driver requests:

```
/lib/firmware/ath9k-eeprom-ahb-18100000.wmac.bin
```

This file must contain a valid AR9300 EEPROM binary with correct magic
and checksum. It can be a pre-compiled dump of the generic AR9300 default
template.

**Real-world precedent:** The Meraki MR18 (QCA9557) uses exactly this
approach — its DTS has `qca,no-eeprom` on the WMAC node:

```dts
&wmac {
    status = "okay";
    qca,no-eeprom;
};
```

**DTS change required** (in our AREDN patch):

```dts
&wmac {
    status = "okay";
    qca,no-eeprom;
    /* Do NOT bind to &art — ART 0x1000 is blank on XC boards */
};
```

**Build change:** Include the template binary in the firmware image at
`/lib/firmware/ath9k-eeprom-ahb-18100000.wmac.bin`.

**Pros:**
- Standard, upstream-friendly mechanism
- No kernel patches
- No flash writes to production hardware
- Exactly matches how other boards with missing caldata handle this
- Template file can be improved/replaced without reflashing

**Cons:**
- Must generate/obtain a valid AR9300 template binary
- Template MAC is `00:02:03:04:05:06` (must be overridden in DTS or at runtime)
- Generic caldata — not per-board calibrated

**Assessment:** This is the cleanest, most upstream-friendly approach.
For a receive-only spectrum analyzer radio, a generic template is perfectly
adequate.

### 6.2 Current Approach: Share PCI Caldata (0x5000) ← What We Use Now

The DTS points the WMAC at `<&art 0x5000>` (the ath10k PCI radio's
factory calibration). This data has a valid structure (txrxMask=0x77,
proper AR9300 format) and passes all checks.

**Pros:**
- Works reliably
- Structurally correct AR9300 EEPROM
- Unique per-board MAC and subsystem IDs

**Cons:**
- Calibration data is for a different RF path (different LNA/PA/antenna)
- Absolute power readings will be systematically wrong
- Per-frequency gain slope won't match the WMAC's actual response
- Sharing caldata between two different drivers (ath10k + ath9k) is fragile

**Assessment:** Works for receive-only spectral scanning but isn't the
cleanest approach. The FFT hardware produces valid magnitude data
regardless. Relative comparisons within a sweep are valid. Absolute dBm
values carry ~5-15 dB systematic error.

### 6.3 Write Valid Caldata to ART 0x1000

Flash a properly structured AR9300 EEPROM to the blank 0x1000 region.
The data could come from:

- A NanoBeam AC Gen2 XC (which has factory WMAC caldata at 0x1000)
- The default template compiled into a binary blob
- Custom calibration from lab equipment

**Pros:**
- Clean, standard approach
- No kernel patches needed
- DTS works as designed with `<&art 0x1000>`

**Cons:**
- Flash writes to production hardware carry brick risk
- Template/borrowed data still isn't per-board calibrated
- Requires understanding the AR9300 compressed EEPROM format

### 6.4 Runtime Noise Floor Correction (Software)

Leave caldata as-is (sharing 0x5000 or using firmware template), apply
corrections in the RFeye agent software based on runtime noise floor
calibration from the driver.

**Pros:**
- No flash writes, no kernel patches
- Runtime NF cal partially compensates for wrong/generic caldata
- Can be done entirely in userspace

**Cons:**
- Only fixes absolute NF offset, not per-frequency gain slope
- Requires reading debugfs `dump_nfcal` per channel

---

## 7. Key Source Code References

| File | Function/Symbol | Role |
|------|----------------|------|
| `ar9003_eeprom.c:46` | `ar9300_default` | Default template struct (MAC 00:02:03:04:05:06) |
| `ar9003_eeprom.c:2940` | `ar9300_eep_templates[]` | Array of all built-in templates |
| `ar9003_eeprom.c:3266` | `ar9300_eeprom_restore_internal()` | Main EEPROM restore logic |
| `ar9003_eeprom.c:3282` | `ath9k_hw_use_flash()` check | Flash/nvmem data validation |
| `ar9003_eeprom.c:3299` | `memcpy(mptr, &ar9300_default)` | Template loaded before EEPROM/OTP |
| `ar9003_eeprom.c:3391` | `ath9k_hw_ar9300_fill_eeprom()` | Caller that checks return code |
| `eeprom.c:146` | `ath9k_hw_nvram_read()` | Read dispatch (nvmem → firmware → pdata → bus) |
| `eeprom.h:111` | `ath9k_hw_use_flash()` | `!(ah_flags & AH_USE_EEPROM)` |
| `init.c:572` | `ath9k_nvmem_request_eeprom()` | DTS nvmem cell → nvmem_blob |
| `ahb.c:60` | `ath_ahb_eeprom_read()` | AHB stub — always returns false |
| `ar9003_eeprom.c:3085` | `ar9300_otp_read_word()` | Direct OTP hardware read |

---

## 8. Confirmed Hardware Facts (Stock Rocket 5AC Lite)

Verified via SSH to stock firmware (2026-05-28):

1. **No device tree** — kernel 2.6.32, uses board files
2. **EEPROM partition (mtd5)** is 64KB:
   - `0x0000`: `04:18:d6:a5:71:c1` + `06:18:d6:a5:71:c1` + subsys IDs + 0xFF
   - `0x1000`: All `0xFF` (blank — no WMAC caldata)
   - `0x5000`: Full QCA9882 factory calibration (CUS223-720-S0849)
3. **wifi1 MAC = `00:02:03:04:05:06`** — default template, not real
4. **wifi1 at `0xb8100000`** — AHB-mapped WMAC
5. **No caldata files on disk** — `/lib/firmware/` is empty
6. **No caldata extracted at runtime** — nothing appears in `/tmp/` when airview runs
7. **`ubnt-caldata` says "Unsupported board: 0xe1f5"** — not used on Rocket 5AC Lite
8. **dmesg confirms template fallback**: `"No vaid CAL, calling default template"`
9. **Radio 2 is receive-only** — airview/spectral in monitor mode, 19 dBm max TX
10. **Proprietary `ath_hal 0.9.17.1 (AR9380)`** handles the fallback gracefully

---

## 9. Generating the Template Binary

To create the firmware file for the `qca,no-eeprom` approach, we need a
valid AR9300 EEPROM binary. Options:

### Option A: Extract from a board with valid WMAC caldata

A NanoBeam AC Gen2 XC has factory data at ART+0x1000:
```bash
# On a Gen2 XC board:
dd if=/dev/mtd5 bs=1 skip=4096 count=2116 of=ath9k-eeprom-ahb-18100000.wmac.bin
```

### Option B: Build from the kernel's default template

The `ar9300_default` struct in `ar9003_eeprom.c` can be compiled into a
standalone binary. This requires serializing the struct into the AR9300
compressed EEPROM wire format with correct magic (0x5cb8) and checksums.

### Option C: Use ath9k-caldata tools

The `ath9k-caldata` package or similar tools can generate a minimal valid
EEPROM binary from template parameters.

---

## 10. Conclusion

The Rocket 5AC Lite (XC) WMAC was **never factory-calibrated by Ubiquiti**.
They designed it as a spectrum analyzer appendage that runs on generic
template defaults. The mainline ath9k driver has the same template data
but handles the "no valid caldata found" case differently — as a hard
failure instead of a graceful fallback.

**Current best approach:** Continue using `<&art 0x5000>` (PCI caldata)
for the WMAC. It's structurally valid, per-board unique, and sufficient
for receive-only spectral scanning. The systematic calibration errors
are acceptable for RFeye's use case (field diagnostic tool, not lab
instrument).

**Future option:** If exact stock-firmware replication is desired, a small
kernel patch to `ar9300_eeprom_restore_internal()` could enable the template
fallback path. This would match the proprietary driver's behavior exactly.
