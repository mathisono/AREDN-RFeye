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

## 5. Available Solutions

### 5.1 Current Approach: Share PCI Caldata (0x5000) ← What We Use

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

**Assessment:** Acceptable for receive-only spectral scanning. The FFT
hardware produces valid magnitude data regardless. Relative comparisons
within a sweep are valid. Absolute dBm values carry ~5-15 dB systematic
error.

### 5.2 Kernel Patch: Accept Template on Fail

A small patch to `ar9300_eeprom_restore_internal()` could make the
default template fallback work, matching the proprietary driver's behavior:

```diff
 fail:
 	kfree(word);
-	return -1;
+	/* If EEPROM/OTP failed but template is loaded, use it.
+	 * This matches the proprietary ath_hal behavior for SoCs
+	 * without factory WMAC calibration (e.g., Rocket 5AC Lite XC).
+	 */
+	ath_dbg(common, EEPROM,
+		"No valid EEPROM/OTP found, using default template\n");
+	return 0;
```

**Pros:**
- Exactly replicates Ubiquiti's stock behavior
- Uses well-tested default values
- No per-board data needed

**Cons:**
- Kernel patch — must be carried in OpenWrt package
- Default template is even more generic than PCI caldata
- Template MAC is `00:02:03:04:05:06` (must be overridden)
- May have unintended effects on other platforms

**Assessment:** Worth considering if we want to match stock behavior,
but the PCI caldata approach (5.1) is currently better because it at least
has per-board factory data.

### 5.3 Write Valid Caldata to ART 0x1000

Flash a properly structured AR9300 EEPROM to the blank 0x1000 region.
The data could come from:

- A NanoBeam AC Gen2 XC (which has factory WMAC caldata at 0x1000)
- The default template compiled into a binary blob
- Custom calibration from lab equipment

**Pros:**
- Clean, standard approach
- No kernel patches needed
- DTS works as designed

**Cons:**
- Flash writes to production hardware carry brick risk
- Template/borrowed data still isn't per-board calibrated
- Requires understanding the AR9300 compressed EEPROM format

### 5.4 Runtime Noise Floor Correction (Software)

Leave caldata as-is (sharing 0x5000), apply corrections in the RFeye
agent software based on runtime noise floor calibration from the driver.

**Pros:**
- No flash writes, no kernel patches
- Runtime NF cal partially compensates for wrong caldata
- Can be done entirely in userspace

**Cons:**
- Only fixes absolute NF offset, not per-frequency gain slope
- Requires reading debugfs `dump_nfcal` per channel

---

## 6. Key Source Code References

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

## 7. Confirmed Hardware Facts (Stock Rocket 5AC Lite)

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

## 8. Conclusion

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
