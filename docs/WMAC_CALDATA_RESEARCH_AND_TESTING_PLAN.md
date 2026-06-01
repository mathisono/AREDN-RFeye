# WMAC Caldata Research & Testing Plan

> **Date:** 2026-05-27 (updated 2026-05-31)  
> **Status:** DRAFT — for review before push  
> **Context:** r16 WMAC spectral scanning works with shared PCI caldata,  
> but the shared PCI caldata is **not an appropriate WMAC calibration  
> source** — it may be structurally accepted by ath9k but was factory-  
> calibrated for a different RF chain. The working question is:  
>
> **What structurally valid AR9300/ath9k-compatible calibration source  
> should be used for the WMAC when XC boards have blank ART+0x1000?**  
>
> This document defines the reconnaissance and testing needed to answer  
> that question, and the safety rules that must hold until it is answered.

---

## 0. Safety Rules

> **These rules supersede any earlier framing that treats shared PCI caldata
> as an acceptable WMAC calibration source.**

1. **Do not feed blank ART+0x1000** to the WMAC. All-`0xFF` is not caldata.
2. **Do not feed random bytes.** Synthetic caldata without RF-path measurement is not caldata.
3. **Do not feed PCI radio ART+0x5000 as the production WMAC calibration source.**
   The 0x5000 data is structurally valid AR9300 EEPROM and ath9k will parse it,
   but it was factory-calibrated for a completely different RF chain (discrete
   QCA988x PCI radio with dedicated LNA/PA, vs. SoC-integrated AR9550 WMAC).
   Using it as the WMAC source is a known-bad fallback, acceptable only for
   deliberate bench testing of that specific fallback.
4. **Find a valid WMAC-calibrated source.** Preferred sources, in order:
   - WA-series (non-XC) board ART+0x1000 with factory WMAC caldata
   - Stock Ubiquiti DTS or firmware evidence of a WMAC caldata path
   - NanoBeam AC Gen2 XC caldata at ART+0x1000 (if populated)
   - Runtime-derived caldata with external RF reference
5. **Treat any substitute caldata as bench-only** until confirmed structurally
   valid and radio-appropriate for the target WMAC RF path.

---

## 1. Current State

### What's Working (r16)

The AREDN PR #2725 DTS patch (now superseded by [PR #2730](https://github.com/aredn/aredn/pull/2730))
enabled the QCA9558 on-chip WMAC on XC boards (PowerBeam 5AC 500, Rocket 5AC
Lite) by sharing the existing `cal_art_5000` nvmem cell between the ath10k
PCI radio and the ath9k WMAC.

**PR #2730 (merged)** takes a cleaner approach: DTS entries enable the WMAC
with `qca,no-eeprom`, which tells ath9k to load caldata from the filesystem.
No caldata is shipped with AREDN — the WMAC will not initialize until an app
(RFeye) installs the appropriate firmware file. See `docs/ROADMAP.md`.

**r16 soak test results:**
- 5-minute sweep, 24 channels including DFS
- ~130 frames/sec, ~1 full 24-channel sweep/sec
- 2.6 MB captured, zero mesh impact
- Spectral data is usable for relative visualization

> **⚠️ r16 caldata status:** r16 uses PCI caldata (ART+0x5000) for the WMAC.
> This produces structurally valid spectral data but is **not appropriate as
> a production WMAC calibration source.** See Section 0 safety rules. The
> r16 results should be treated as a bench-only proof-of-concept for the
> spectral scanning pipeline, not as evidence that the caldata question is
> resolved.

### The Caldata Situation

The ART partition layout on XC boards:

| Offset | Size | Content | Notes |
|--------|------|---------|-------|
| 0x0000 | 6 bytes | Ethernet MAC | Used by eth0 |
| 0x1000 | 0x440 | All `0xFF` | **Blank** — no WMAC caldata written at factory |
| 0x5000 | 0x844 | AR9300 compressed EEPROM | Factory-calibrated for the **ath10k PCI radio** |

The AR9300 EEPROM at 0x5000 contains:
- Header: `44 08 7f 34`
- Board ID: `CUS223-720-S0849`
- MAC: `f4:92:bf:bc:8a:de` (WMAC MAC, offset from PCI MAC)
- Per-frequency calibration piers (2G + 5G)
- Per-chain gain corrections
- Noise floor thresholds
- TX power target tables
- Regulatory CTL limits

**The key issue:** This caldata was factory-calibrated for the ath10k PCI
radio's RF chain — its LNA, PA, filters, antenna path, and board trace
characteristics. The on-chip WMAC has a **completely different RF front-end**:

| Property | ath10k PCI Radio | WMAC On-Chip Radio |
|----------|-----------------|-------------------|
| Chip | QCA988x (discrete) | AR9550 (SoC-integrated) |
| Bus | PCIe | AHB |
| RF front-end | Dedicated LNA/PA chain | SoC-integrated RF |
| Antenna | 19 dBi planar dish | ~2 dBi PCB trace / UFL |
| Intended bands | 5 GHz only | 2.4 + 5 GHz |
| Factory calibrated? | Yes (ART 0x5000) | **No** (ART 0x1000 is blank) |

### Why It Works Anyway

The ath9k driver can parse the AR9300 EEPROM at 0x5000 because it's the
native AR9300 format. For receive-only spectral scanning:

- ✅ FFT hardware produces valid magnitude data regardless of caldata accuracy
- ✅ Channel/frequency tuning works (synthesizer settings are silicon-level)
- ✅ Relative signal comparisons within a single sweep are valid
- ⚠️ Absolute dBm values are **wrong** — gain corrections are for a different RF path
- ⚠️ Noise floor reference is calibrated for a different chain
- ⚠️ Per-frequency flatness may have systematic errors across the band
- ⚠️ Cross-channel gain variation may not match reality

RFeye already disclaims "approximate/relative dBm, not lab-calibrated" — so
this is acceptable for the current use case.

---

## 2. What Proper WMAC Caldata Would Improve

### 2.1 Noise Floor Accuracy

The ath9k spectral scan power formula (from kernel docs):

```
power(i) = nf + RSSI + 10*log(b(i)^2) - bin_sum
```

Where `nf` is the noise floor. The driver reads `noiseFloorThreshCh[]` from
the AR9300 EEPROM `modalHeader` to bound the noise floor calibration. With
mismatched caldata, the noise floor reference can be off by **5–15 dB**,
shifting all reported power levels by a fixed offset.

**Impact on RFeye:** Absolute dBm readings would shift. Relative comparisons
within a sweep remain valid since the offset is constant.

### 2.2 Per-Chain Gain Correction

The AR9300 EEPROM stores per-chain, per-frequency calibration data in
`calPierData2G[]` and `calPierData5G[]`. Each calibration pier contains:

- `refPower` — measured output power at a reference setting
- `voltMeas` — voltage regulator reading at calibration time
- `tempMeas` — temperature at calibration time

These allow the driver to compensate for per-chain gain variation and
temperature drift. With PCI caldata on the WMAC:

- **Gain slope across frequency** will follow the PCI radio's curve, not the WMAC's
- **Temperature compensation** will use the wrong baseline
- Bands where the two RF paths diverge most (e.g., band edges, DFS) will have the worst errors

**Impact on RFeye:** The spectral "shape" across a wideband sweep may have
systematic tilt or ripple that doesn't match the true RF environment.

### 2.3 Per-Frequency Flatness

Factory calibration measures response at specific frequency piers and the
driver interpolates between them. The ath10k PCI radio and WMAC have different:

- Filter bandpass shapes
- LNA gain vs. frequency curves
- Board trace losses at different frequencies

With mismatched caldata, some frequency ranges will read systematically
high or low relative to their true level.

**Impact on RFeye:** A strong signal at 5300 MHz and the same signal at
5700 MHz might show different apparent power levels even though they're
identical. This makes cross-channel comparisons less trustworthy.

### 2.4 Regulatory/TX (Not Applicable)

The CTL (Conformance Test Limit) tables and TX power targets in the EEPROM
are irrelevant — the WMAC is receive-only for spectral scanning. RFeye
never transmits on this radio. TX should remain disabled or zero-power.

---

## 3. How Could We Get Proper WMAC Caldata?

### Option A: Runtime Noise Floor Calibration (Easiest)

The ath9k driver performs runtime noise floor calibration (NF_CAL) on each
channel change. This partially compensates for the missing factory data:

- The driver measures the actual noise floor on the current channel
- This updates the `nf` value used in the spectral power formula
- Per-channel NF readings are stored and aged

**What this fixes:** Absolute noise floor offset per channel.  
**What this doesn't fix:** Per-chain gain slope, frequency flatness, temperature drift.

**Test:** Compare NF readings from `ath9k` debugfs with those from a
known-good reference (e.g., an SDR or calibrated spectrum analyzer).

```bash
# Read current noise floor on phy1
cat /sys/kernel/debug/ieee80211/phy1/ath9k/dump_nfcal
```

### Option B: Cross-Calibrate Against the ath10k Radio (Medium)

Use the ath10k production radio as a reference source:

1. On a quiet channel where ath10k is operating, read the ath10k spectral
   power for the current channel
2. Simultaneously read the ath9k WMAC spectral power for the same channel
3. Compute the gain offset between the two radios
4. Apply this offset as a per-channel correction factor in RFeye

**Advantage:** Uses the factory-calibrated ath10k as a reference without
any external equipment.

**Limitation:** Only works on the single channel where ath10k is operating.
Can't calibrate across the full band unless you temporarily retune ath10k
(which disrupts the mesh — not acceptable for production).

### Option C: Generate WMAC Caldata from a Reference (Hard)

Use an external RF source or calibrated spectrum analyzer to:

1. Inject known-level signals at multiple frequency piers
2. Record the WMAC's reported levels
3. Compute gain correction tables
4. Write a custom AR9300 EEPROM to ART+0x1000

**Advantage:** Proper factory-style calibration for the actual WMAC RF path.  
**Disadvantage:** Requires lab equipment, per-board procedure, and a method
to write to the ART partition (flash write, risky on production hardware).

### Option D: Borrow NanoBeam AC Gen2 XC Caldata (Exploratory)

The NanoBeam AC Gen2 XC (`NBE-5AC-Gen2`) is the same board family but
**does have factory WMAC caldata at ART+0x1000**. The upstream OpenWrt DTS
uses `mtd-cal-data = <&art 0x1000>`.

If the WMAC RF path is physically similar across XC boards, one Gen2's
caldata might serve as a "better-than-nothing" template for other XC boards.

**Risk:** Board-to-board variation in component tolerances means one board's
caldata won't perfectly match another. Still likely better than sharing PCI
caldata.

**Test:** If we can obtain a Gen2 XC ART dump, compare the 0x1000 caldata
structure with the 0x5000 data. Look at which fields differ (gain piers,
NF thresholds, modal header).

---

## 4. Testing Plan

### T0: Stock Firmware Calibration Reconnaissance

**Goal:** Before testing RFeye with substitute caldata, inspect a stock
Ubiquiti Rocket / XC image to determine whether the vendor firmware exposes
a WMAC calibration source through device tree, firmware files, or runtime
extraction.

**Commands:**

1. Record whether `/proc/device-tree` exists on stock firmware.
2. Capture `ls -R /proc/device-tree`.
3. Capture `ls -laR /lib/firmware`.
4. Search device tree for `wifi`, `wmac`, `ath`, `cal`, `eeprom`, `art`,
   `nvmem`, `pci`, and `qca` nodes.
5. Search `/lib/firmware` for ath9k / AR9300 / QCA calibration files.
6. Hexdump any matching DT files to inspect structure and content.

**What we're looking for in device tree:**
```
/proc/device-tree/.../wifi...
/proc/device-tree/.../wmac...
/proc/device-tree/.../ath9k...
/proc/device-tree/.../qca9558...
/proc/device-tree/.../mtd-cal-data
/proc/device-tree/.../nvmem-cells
/proc/device-tree/.../calibration
/proc/device-tree/.../eeprom
/proc/device-tree/.../art
```

**What we're looking for in firmware:**
```
ath9k, ar9300, qca9558, caldata, eeprom,
board.bin, board-2.bin, ubnt, radio, wmac
```

**Success condition:**
- Find evidence that stock firmware either:
  - uses valid ART+0x1000 data on WA hardware,
  - loads WMAC caldata from a firmware file,
  - derives WMAC caldata at runtime,
  - or has no WMAC calibration source exposed (confirming the blank-ART
    situation is vendor-side, not an AREDN omission).

**Safety condition:**
- Do not use blank ART+0x1000.
- Do not generate random calibration data.
- Do not use ART+0x5000 as the production WMAC calibration source.
- Treat any substitute caldata as bench-only until confirmed structurally
  valid and radio-appropriate.

**Status:** ✅ COMPLETE (2026-05-29). See `docs/T0_STOCK_PROBE_RESULTS.md`.

**Key findings:**
- Target was a **WA board** (PowerBeam 5AC Gen2, PBE-5AC-Gen2), not XC
- **EEPROM+0x1000 contains REAL WMAC caldata** — NOT blank! AR9300 template
  format (`02 02`), ~512 bytes, with MAC `26:5A:4C:0E:25:14` and structured
  gain/calibration tables
- Stock firmware creates `airview1` monitor interface on WMAC for AirView
  spectral scanning — same use case as RFeye
- `ath_spectral` module loaded; WMAC hidden from web UI (`web_exclude=1`)
- No device tree on stock AirOS (kernel 2.6.32); caldata loaded directly
  from EEPROM MTD partition
- **WA boards have factory WMAC caldata; XC boards do not** — this is a
  vendor decision, not an AREDN omission
- WMAC caldata uses template format (`02 02`), PCI uses compressed (`44 08`)

---

### Phase 1: Quantify Current Accuracy (No Hardware Changes)

**Goal:** Establish baseline — how far off are the WMAC spectral readings
with shared PCI caldata?

| Test | Method | What It Tells Us |
|------|--------|-----------------|
| **T1: NF cal dump** | `dump_nfcal` on phy1 across all 24 channels | Whether runtime NF cal is running and producing reasonable values |
| **T2: Cross-radio comparison** | Compare ath10k and ath9k spectral on the mesh channel simultaneously | Gain offset between the two radios on one known channel |
| **T3: Ambient noise floor sweep** | Record min power per channel across full 5 GHz band, compare against expected thermal noise (~-95 dBm for 20 MHz BW) | Whether the absolute noise floor is in the right ballpark |
| **T4: Known signal injection** | Use a nearby node's beacon as a reference, compare reported power on ath10k vs ath9k | Real-world cross-radio offset |
| **T5: Band-edge flatness** | Compare spectral shape at 5150 MHz vs 5500 MHz vs 5850 MHz with the same environment | Whether frequency-dependent gain error is visible |

#### T1 Procedure
```bash
# On the node, after a sweep:
cat /sys/kernel/debug/ieee80211/phy1/ath9k/dump_nfcal
# Record values for each chain, each channel
```

#### T2 Procedure
```bash
# 1. Start ath10k spectral on current mesh channel
echo background > /sys/kernel/debug/ieee80211/phy0/ath10k/spectral_scan_ctl
# 2. Read ath10k spectral data
cat /sys/kernel/debug/ieee80211/phy0/ath10k/spectral_scan0 > /tmp/ath10k_ref.bin

# 3. Tune ath9k to the same channel
iw dev mon1 set channel <mesh_channel> HT20
echo background > /sys/kernel/debug/ieee80211/phy1/ath9k/spectral_scan_ctl
cat /sys/kernel/debug/ieee80211/phy1/ath9k/spectral_scan0 > /tmp/ath9k_ref.bin
echo disable > /sys/kernel/debug/ieee80211/phy1/ath9k/spectral_scan_ctl

# 4. Parse both, compare average power levels
# The delta is the cross-radio gain offset
```

#### T3 Procedure
```bash
# Capture spectral data across all channels during a quiet period (late night)
# Parse each channel's FFT bins, compute median power
# Expected: ~-90 to -100 dBm for thermal noise floor in 20 MHz
# If readings are consistently 10+ dB off, caldata is the likely cause
```

### Phase 1b: Hardware Register & Driver Diagnostics

**Goal:** Capture the exact hardware-level spectral engine configuration
and driver interactions that stock AirOS (ubnthal) and Linux ath9k use,
to detect proprietary register modifications or non-standard FFT behavior.

| Test | Method | What It Tells Us |
|------|--------|------------------|
| **T11: AR_PHY_SPECTRAL_SCAN register snapshot** | `devmem` or `debugfs/regidx` to read register `0x9910` before and after airviewd starts | Whether ubnthal writes proprietary bits to the spectral MAC register to alter FFT reporting interval or bypass standard behavior |
| **T12: LD_PRELOAD ioctl hook** | Compile a tiny C shim that hooks `ioctl()` via `LD_PRELOAD`, logs ubnthal ioctl commands to a `/dev/shm` ring buffer | Exact ioctl structs passed to ubnthal without strace overhead or frame drops |

#### T11: Hardware Register Snapshot (AR_PHY_SPECTRAL_SCAN — 0x9910)

The ath9k spectral engine is driven by the physical `AR_PHY_SPECTRAL_SCAN`
MAC register at offset **`0x9910`** on AR9003+ silicon (QCA955x, QCA956x).
If Ubiquiti's `ubnthal` kernel driver writes proprietary bits to this register
to alter FFT behavior, `strace` won't catch it because it happens in kernel
space. This test captures the register directly.

**Register bit fields (AR9003+ / AR9550):**

| Bits | Field | Description |
|------|-------|-------------|
| 0 | `SPECTRAL_SCAN_ENA` | Enable spectral scan |
| 1 | `SPECTRAL_SCAN_ACTIVE` | Scan is actively running |
| 7:2 | `SPECTRAL_SCAN_COUNT` | Number of reports to generate (0 = continuous) |
| 8 | `SPECTRAL_SCAN_SHORT_RPT` | Short report mode |
| 17:9 | `SPECTRAL_SCAN_PERIOD` | Period between successive scans (clocks) |
| 25:18 | `SPECTRAL_SCAN_FFT_PERIOD` | FFT period (resolution/speed tradeoff) |
| 28:26 | `SPECTRAL_SCAN_PRIORITY` | Priority vs normal traffic |
| 29 | `SPECTRAL_SCAN_GC_ENA` | Gain change enable |
| 30 | `SPECTRAL_SCAN_RESTART_ENA` | Auto-restart on gain change |
| 31 | `SPECTRAL_SCAN_NB_TONE_THR` | Narrowband tone detection threshold |

**Procedure:**

```bash
# --- On stock AirOS (via SSH) ---

# WMAC AHB base address is 0x18100000 (physical)
# AR_PHY_SPECTRAL_SCAN = base + 0x9910
WMAC_BASE=0x18100000
SPECTRAL_REG=$((WMAC_BASE + 0x9910))

# 1. Snapshot BEFORE airviewd is running
echo "=== BEFORE airviewd ==="
devmem $SPECTRAL_REG 2>/dev/null || echo "devmem not available"

# Alternative: read via debugfs regidx if available
if [ -f /sys/kernel/debug/ieee80211/phy*/ath9k/regidx ]; then
  echo 0x9910 > /sys/kernel/debug/ieee80211/phy*/ath9k/regidx
  cat /sys/kernel/debug/ieee80211/phy*/ath9k/regval
fi

# 2. Start airviewd (AirView spectral daemon)
# On stock AirOS, airviewd may auto-start or can be triggered from the UI
# If it's already running:
pidof airviewd && echo "airviewd running" || echo "airviewd not running"

# 3. Snapshot AFTER airviewd is active
echo "=== AFTER airviewd ==="
devmem $SPECTRAL_REG 2>/dev/null || echo "devmem not available"

# 4. Decode the register value
# Example: if devmem returns 0x00012801
#   bit 0 = 1 → spectral scan enabled
#   bits 7:2 = 0 → continuous mode
#   bits 17:9 = 0x94 → scan period 148 clocks
#   bits 25:18 = 0x00 → FFT period 0 (fastest)
```

**What to look for:**
- Are the `SPECTRAL_SCAN_COUNT`, `SPECTRAL_SCAN_PERIOD`, or `SPECTRAL_SCAN_FFT_PERIOD`
  fields set to non-standard values that wouldn't match Linux ath9k defaults?
- Is `SPECTRAL_SCAN_SHORT_RPT` enabled (bit 8)? If so, the TLV reports may
  be truncated compared to standard ath9k output.
- Are any reserved/undocumented bits set? These could indicate proprietary
  ubnthal extensions.

#### T12: LD_PRELOAD ioctl Hook (Optional — High Fidelity)

`strace` has overhead that can skew strict kernel timing, especially when
polling thousands of high-speed FFT frames per second. If ioctl payloads
look truncated or misaligned in other diagnostics, a tiny C shim that hooks
`ioctl()` via `LD_PRELOAD` and logs specifically ubnthal commands to a
`/dev/shm` ring buffer is the cleanest way to read the exact structs without
dropping frames.

**C shim source (`ioctl_hook.c`):**

```c
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <string.h>
#include <time.h>

/* Ring buffer in shared memory for zero-copy logging */
#define RING_PATH "/dev/shm/ioctl_ring.bin"
#define RING_SIZE (4 * 1024 * 1024)  /* 4 MB ring */
#define ENTRY_MAX 256

static int ring_fd = -1;
static char *ring_buf = NULL;
static volatile size_t ring_pos = 0;

typedef int (*real_ioctl_t)(int fd, unsigned long request, ...);
static real_ioctl_t real_ioctl = NULL;

__attribute__((constructor))
static void init(void) {
    real_ioctl = (real_ioctl_t)dlsym(RTLD_NEXT, "ioctl");
    ring_fd = open(RING_PATH, O_CREAT | O_RDWR, 0644);
    if (ring_fd >= 0) {
        ftruncate(ring_fd, RING_SIZE);
        ring_buf = mmap(NULL, RING_SIZE, PROT_WRITE, MAP_SHARED, ring_fd, 0);
    }
}

static void log_entry(int fd, unsigned long req, void *arg, int ret) {
    if (!ring_buf) return;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    char entry[ENTRY_MAX];
    int n = snprintf(entry, ENTRY_MAX,
        "%ld.%09ld fd=%d req=0x%lx arg=%p ret=%d\n",
        ts.tv_sec, ts.tv_nsec, fd, req, arg, ret);
    if (n <= 0 || n >= ENTRY_MAX) return;

    size_t pos = __sync_fetch_and_add(&ring_pos, n) % RING_SIZE;
    memcpy(ring_buf + pos, entry, n);
}

int ioctl(int fd, unsigned long request, ...) {
    va_list ap;
    va_start(ap, request);
    void *arg = va_arg(ap, void *);
    va_end(ap);

    int ret = real_ioctl(fd, request, arg);
    log_entry(fd, request, arg, ret);
    return ret;
}
```

**Build (cross-compile for MIPS or build on-device if toolchain available):**

```bash
# Cross-compile for MIPS (from build host)
mips-openwrt-linux-musl-gcc -shared -fPIC -o ioctl_hook.so ioctl_hook.c -ldl

# Deploy to target
scp ioctl_hook.so root@<node>:/tmp/

# Run airviewd with the hook
LD_PRELOAD=/tmp/ioctl_hook.so airviewd &

# Read captured ioctls
hexdump -C /dev/shm/ioctl_ring.bin | head -100
```

**What to look for:**
- ioctl request codes used by airviewd to communicate with ubnthal
- Frequency of ioctl calls (correlated with FFT frame rate)
- Any proprietary struct payloads that configure spectral behavior
  outside the standard `AR_PHY_SPECTRAL_SCAN` register

---

### Phase 2: Runtime Correction (Software Only)

**Goal:** Improve accuracy without touching flash or caldata.

| Test | Method | What It Tells Us |
|------|--------|-----------------|
| **T6: NF-corrected sweep** | Read `dump_nfcal` per channel, apply correction to spectral power formula | Whether runtime NF cal alone closes the gap |
| **T7: Cross-radio offset table** | Build a per-channel offset table from T2/T4, apply in RFeye | Whether a simple gain offset per channel improves accuracy |

#### T6 Implementation Sketch
```javascript
// In the GUI or agent, after each channel:
// 1. Read the runtime NF value from the driver
// 2. Use it instead of the default -96 dBm assumption
// 3. power(i) = nf_actual + RSSI + 10*log(b(i)^2) - bin_sum
```

### Phase 3: Hardware Caldata (If Needed)

**Goal:** Only pursue if Phase 1/2 show unacceptable errors.

| Test | Method | Depends On |
|------|--------|-----------|
| **T8: Gen2 XC caldata comparison** | Obtain NanoBeam Gen2 ART dump, compare 0x1000 vs 0x5000 structure | Access to a Gen2 board |
| **T9: ART 0x1000 write test** | Write Gen2-derived caldata to 0x1000 on bench PBE-5AC-500, point WMAC DTS at 0x1000 | Bench-only, not production |
| **T10: External reference cal** | Use SDR or calibrated SA to measure true power, build correction table | Lab equipment |

---

## 5. Success Criteria

| Level | Criteria | Action |
|-------|----------|--------|
| **Good enough** | Ambient noise floor reads within ±6 dB of expected; wideband shape is visually consistent; relative comparisons are valid | Ship as-is, document limitations |
| **Improved** | Runtime NF correction brings readings within ±3 dB; cross-channel gain is flat within ±4 dB | Add NF correction to RFeye agent |
| **Lab-grade** | All readings within ±2 dB of reference SA; per-channel flatness within ±2 dB | Requires proper caldata (Phase 3) |

RFeye's design intent is "Good enough" — it's a field diagnostic tool, not
a lab instrument. "Improved" is worth pursuing if it's achievable in software.
"Lab-grade" is a stretch goal for dedicated bench hardware.

---

## 6. Caldata Assessment Summary (Corrected)

Correcting the earlier assessment that was written before r15-r16 proved the
WMAC spectral path works:

| Claim | Status | Notes |
|-------|--------|-------|
| PR #2725 enables QCA9558 WMAC on XC boards | ✅ Correct | Superseded by PR #2730 (merged) which uses `qca,no-eeprom` filesystem caldata |
| Patch uses `cal_art_5000` (not 0x1000) | ✅ Correct | 0x1000 is blank on Gen1 XC boards; 0x5000 has valid AR9300 EEPROM |
| "Cannot work" — no valid caldata | ⚠️ Partially wrong | The WMAC *functions* with PCI caldata (structurally valid AR9300 EEPROM), but PCI caldata is **not an appropriate WMAC calibration source** — it is calibrated for a different RF chain. See Section 0 safety rules |
| Template EEPROM fallback | ⚠️ Misleading | The 0x5000 data IS factory-written (unique MAC, board ID). It's not a template — it's caldata for a *different radio on the same board* |
| "RFeye should not depend on PR #2725" | ❌ Outdated | r16 depended on #2725; r17+ targets #2730 (merged) |
| Proper WMAC caldata is required | ✅ Correct | Not just "would help" — using PCI caldata is a known-bad fallback acceptable only for bench testing. A valid WMAC-calibrated source must be identified before any production use |
| Spectral scanning ≠ radio communication | ✅ Key insight | FFT hardware works without TX-quality caldata |

---

## 7. Recommended Next Steps

> **r17 goal (revised 2026-05-29):** r17 should not be "make 0x5000 better."
> r17 should be **"identify a valid WMAC caldata source and reject unsafe
> sources."**

1. **Run T0 (stock firmware reconnaissance)** on the bench Rocket
   - Highest priority — determines whether Ubiquiti ever ships WMAC caldata
     on XC boards, and if so, how it's exposed
   - Results gate all subsequent decisions

2. **Run Phase 1 tests (T1–T5)** on the bench PBE-5AC-500
   - Quantifies how far off the PCI-caldata fallback actually is
   - Results determine whether runtime correction is worth pursuing

3. **If T0 finds a valid WMAC caldata source:** Evaluate using it
   - WA-series 0x1000 data or Gen2 XC 0x1000 data
   - Compare structure against 0x5000 to confirm it's WMAC-calibrated
   - Test on bench hardware only

4. **If T0 finds no WMAC caldata anywhere:** Document this as a vendor gap
   - XC boards ship with no WMAC calibration — the blank 0x1000 is
     intentional because Ubiquiti never activates the WMAC on these boards
   - Evaluate Option A (runtime NF cal) and Option B (cross-radio offset)
     as software-only mitigations
   - Ship with explicit "uncalibrated WMAC" disclaimer

5. **If NF is off by >10 dB:** Implement T6 (runtime NF correction)
   - Software-only fix, no flash writes

6. **If cross-channel gain varies >8 dB:** Implement T7 (offset table)
   - Build empirical correction table from T2/T4 measurements

7. **Update README** with measured accuracy characteristics and caldata
   provenance after testing

---

## 8. Spectral Data Format Reference

### TLV Binary Stream Structure

The spectral data flowing out of the ath9k relayfs endpoint
(`/sys/kernel/debug/ieee80211/phy1/ath9k/spectral_scan0`) is **not raw
ASCII or CSV text**. It is a highly optimized, concatenated stream of
binary **TLV (Type-Length-Value)** structures defined in the upstream
Linux kernel (`drivers/net/wireless/ath/spectral_common.h`).

Every time the hardware finishes one FFT sweep, it pushes one TLV struct
into the relayfs buffer:

```
┌──────────────────────────────────────────────────────┐
│ FFT Sample TLV Header                                │
├──────────┬──────────┬────────────────────────────────┤
│ type (1) │ len (2)  │ payload (variable)             │
├──────────┴──────────┴────────────────────────────────┤
│                                                      │
│  ┌─────────────────────────────────────────────────┐  │
│  │ Signature:  0x1756 (magic bytes)                │  │
│  │ Type:       1 = HT20 (56 bins)                 │  │
│  │             2 = HT40 (128 bins)                │  │
│  │             3 = HT40 (64 bins, AR9003+)        │  │
│  ├─────────────────────────────────────────────────┤  │
│  │ Payload Header:                                │  │
│  │   timestamp    — hardware TSF timestamp        │  │
│  │   freq         — center frequency of sweep     │  │
│  │   noise_floor  — hardware NF reading (dBm)     │  │
│  │   rssi         — combined RSSI                 │  │
│  │   rssi_ctl[3]  — per-chain control RSSI        │  │
│  │   rssi_ext[3]  — per-chain extension RSSI      │  │
│  ├─────────────────────────────────────────────────┤  │
│  │ bin_pwr[N]:                                    │  │
│  │   N = 56  for HT20                            │  │
│  │   N = 128 for HT40                            │  │
│  │                                                │  │
│  │   Raw 8-bit unsigned integers (0–255)          │  │
│  │   representing RF magnitude per subcarrier bin │  │
│  │   These values are RELATIVE — not absolute dBm │  │
│  └─────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────┘
```

### AR9003+ (QCA955x/QCA956x) FFT Sample Structure

The AR9003 series uses `struct fft_sample_ath9k` (from `spectral_common.h`):

```c
struct fft_sample_tlv {
    uint8_t  type;        /* FFT_SAMPLE_ATH9K = 3 (AR9003+) */
    __be16   length;      /* payload length (big-endian) */
} __packed;

struct fft_sample_ath9k {
    struct fft_sample_tlv tlv;

    uint8_t  max_exp;       /* max exponent for bin scaling */

    __be16   freq;          /* center frequency (MHz) */
    int8_t   rssi;          /* combined RSSI */
    int8_t   noise;         /* noise floor (dBm) */

    __be16   max_magnitude; /* max FFT magnitude */
    uint8_t  max_index;     /* bin index of max magnitude */
    uint8_t  bitmap_weight; /* number of set bits in bitmap */
    __be64   tsf;           /* 64-bit hardware timestamp */

    uint8_t  data[];        /* FFT bin data (variable length) */
} __packed;
```

### Bin Count by Mode

| Mode | Bins | Bandwidth | Bin Width | Notes |
|------|------|-----------|-----------|-------|
| HT20 | 56 | 20 MHz | ~357 kHz | Standard spectral |
| HT40 | 128 | 40 MHz | ~312 kHz | Upper + lower |
| VHT20 | 64 | 20 MHz | ~312 kHz | AR9003+ native |

### Conversion: Raw Bins → Absolute dBm

The raw `bin_pwr` values are strictly **relative** — they represent the
magnitude of each subcarrier's FFT output as an 8-bit unsigned integer.
To convert to absolute dBm power per bin:

$$P_i = \text{NF} + \text{RSSI} + 20 \log_{10}(b_i) - \text{BinSum}$$

Where:

| Variable | Source | Description |
|----------|--------|-------------|
| $P_i$ | computed | Absolute power in dBm for bin $i$ |
| NF | `noise` field | Hardware noise floor reading (dBm), e.g. -95 |
| RSSI | `rssi` field | Combined RSSI from the sample header |
| $b_i$ | `data[i]` | Raw 8-bit bin magnitude (0–255) |
| BinSum | computed | $10 \log_{10}\left(\sum_{k=0}^{N-1} b_k^2\right)$ — total energy normalization |

**Step-by-step:**

```python
import math

def bins_to_dbm(bins, noise_floor, rssi):
    """
    Convert raw FFT bin values to absolute dBm.

    Args:
        bins: list of raw 8-bit bin values (0-255)
        noise_floor: hardware NF reading (dBm, e.g. -95)
        rssi: combined RSSI from TLV header

    Returns:
        list of per-bin power values in dBm
    """
    # Total energy normalization
    bin_sum = 10 * math.log10(sum(b**2 for b in bins if b > 0) or 1)

    result = []
    for b in bins:
        if b > 0:
            p = noise_floor + rssi + 20 * math.log10(b) - bin_sum
        else:
            p = noise_floor  # Below noise floor
        result.append(p)
    return result
```

**Example (HT20, 56 bins):**

```
NF = -95 dBm, RSSI = 30
Bin 28 (center) = 200, most other bins = 10

BinSum = 10*log10(200² + 55*10²) = 10*log10(40000 + 5500) = 46.6 dB

P_28 = -95 + 30 + 20*log10(200) - 46.6
     = -95 + 30 + 46.0 - 46.6
     = -65.6 dBm   ← strong signal in center bin

P_other = -95 + 30 + 20*log10(10) - 46.6
        = -95 + 30 + 20.0 - 46.6
        = -91.6 dBm  ← near noise floor
```

### AR9003+ `max_exp` Scaling

On AR9003+ silicon (QCA955x, QCA956x), the raw bin data may use a
compressed format where `max_exp` encodes a right-shift applied by the
hardware. The actual bin magnitude is:

```
actual_bin[i] = data[i] << max_exp
```

This must be applied **before** the `20*log10(b_i)` calculation.
The upstream `ath9k` driver handles this in `ath9k_spectral_scan_trigger()`
and the relayfs output already includes the `max_exp` field.

### Impact of Caldata on Spectral Readings

The caldata affects spectral accuracy through these fields:

| EEPROM Field | Effect on Spectral |
|-------------|--------------------|
| `noiseFloorThreshCh[]` | Bounds the NF calibration range — wrong values shift all readings |
| `calPierData5G[]` | Per-frequency gain corrections — wrong values cause frequency-dependent tilt |
| `rxGainType` | LNA gain table selection — wrong table shifts RSSI |
| `tempSlope` | Temperature compensation — wrong baseline causes drift |
| `antennaGain` | Antenna gain offset — shifts all absolute readings by a fixed dB |

With PCI caldata on the WMAC, the NF and gain corrections are calibrated
for the wrong RF path, causing systematic offsets. The `20*log10(b_i)`
and `BinSum` terms are computed from the actual FFT hardware output and
are **independent of caldata** — only the NF and RSSI header values are
affected.

---

## 9. References

- [ath9k spectral scan — Linux Wireless docs](https://wireless.docs.kernel.org/en/latest/en/users/drivers/ath9k/spectral_scan.html)
- [AR9300 EEPROM structure — iPXE reference](https://dox.ipxe.org/structar9300__eeprom.html)
- [ART partition caldata — CodeFetch collection](https://github.com/CodeFetch/art-collection)
- [OpenWrt NanoBeam AC Gen2 XC DTS patch](https://lists.openwrt.org/pipermail/openwrt-devel/2023-March/040697.html)
- AREDN PR #2725: WMAC DTS enablement for XC boards (superseded)
- [AREDN PR #2730](https://github.com/aredn/aredn/pull/2730): WMAC DTS hooks with `qca,no-eeprom` (merged)
- `docs/ONCHIP_SCANNER_RADIO_RESEARCH.md` — earlier research on the WMAC hardware
