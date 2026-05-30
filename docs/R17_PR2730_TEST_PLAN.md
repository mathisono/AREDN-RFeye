# r17 PR #2730 Compliance Test Plan

> **Date:** 2026-05-29  
> **Target firmware:** AREDN nightly with PR #2730 merged  
> **Target board:** PowerBeam 5AC 500 (PBE-5AC-500, sysid 0xe3d5) or Rocket 5AC Lite  
> **RFeye package:** `aredn-rfeye_0.1.0-r17_mips_24kc.ipk`

---

## Prerequisites

1. Flash the AREDN nightly firmware that includes PR #2730 onto the test board
2. Verify the node boots and mesh connectivity is working (ath10k radio)
3. SSH access to the node (port 2222 for AREDN)
4. Copy the r17 IPK to the node: `scp -P 2222 aredn-rfeye_0.1.0-r17_mips_24kc.ipk root@<node>:/tmp/`

---

## Test 1: Verify PR #2730 DTS Hooks Present

**Before installing RFeye.** Confirm the nightly firmware has the WMAC DTS entries.

```bash
# Check kernel DT has the WMAC node
ls /sys/firmware/devicetree/base/soc/wmac* 2>/dev/null && echo "WMAC DTS: PRESENT" || echo "WMAC DTS: ABSENT"

# Check if ath9k module is loaded
lsmod | grep ath9k

# Check if a second phy exists (phy1)
ls /sys/kernel/debug/ieee80211/phy*/ath9k 2>/dev/null && echo "ath9k phy: PRESENT" || echo "ath9k phy: ABSENT"

# Check if wlan1 is mentioned in hardware config
cat /etc/aredn_include/device_config.json 2>/dev/null | grep -A2 wlan1 || echo "no wlan1 in config"
```

**Expected result with PR #2730:**
- WMAC DTS node exists but ath9k may or may not have loaded
- If ath9k loaded, it failed to initialize the WMAC (no caldata on filesystem)
- `wlan1` should be present in config with `"disabled": true`

---

## Test 2: Pre-RFeye WMAC State (Baseline)

```bash
# What phys exist?
iw phy

# What wireless interfaces exist?
iw dev

# Is there firmware at the expected path?
ls -la /lib/firmware/ath9k-eeprom-* 2>/dev/null || echo "no ath9k firmware files"

# Kernel log for ath9k
dmesg | grep -i "ath9k\|wmac\|eeprom\|caldata\|firmware" | tail -20
```

**Expected:** No ath9k spectral phy. No firmware file. Kernel log should
show ath9k tried to init WMAC but failed (no caldata).

**Record the exact dmesg error** — this tells us what firmware path ath9k
expects and will guide the caldata installation path if our default guess
(`/lib/firmware/ath9k-eeprom-ahb-18100000.wmac.bin`) is wrong.

---

## Test 3: Install RFeye r17

```bash
opkg install --force-reinstall /tmp/aredn-rfeye_0.1.0-r17_mips_24kc.ipk

# Verify installed version
opkg info aredn-rfeye

# Verify new files present
ls -la /usr/lib/rfeye/rfeye-wmac-provision.sh
ls -la /usr/lib/rfeye/caldata/ath9k-caldata-wmac-wa-reference.bin
```

---

## Test 4: WMAC Provision Status (Before Provisioning)

```bash
/usr/sbin/rfeye-agent wmac_status
```

**Expected output (pretty-printed):**
```json
{
  "ok": true,
  "board": {
    "sysid": "0xe3d5",
    "model": "Ubiquiti PowerBeam 5AC 500",
    "supported": true
  },
  "wmac": {
    "phy": "",
    "initialized": false,
    "spectral_ready": false
  },
  "caldata": {
    "installed": false,
    "reference": "/usr/lib/rfeye/caldata/ath9k-caldata-wmac-wa-reference.bin"
  },
  "ath9k_loaded": false
}
```

**Key checks:**
- `board.supported` = `true`
- `wmac.initialized` = `false`
- `caldata.installed` = `false`

---

## Test 5: Run WMAC Provisioning

```bash
/usr/sbin/rfeye-agent wmac_provision
```

**Expected output (success):**
```json
{
  "ok": true,
  "action": "reloaded",
  "phy": "phy1",
  "spectral_ready": true,
  "message": "WMAC initialized, spectral scanning available"
}
```

**If spectral_ready is false but phy appeared:**
The WMAC initialized but spectral debugfs isn't available. May need a reboot.

**If the provisioning fails with "modprobe ath9k failed":**
Check `dmesg` for the firmware path ath9k is looking for. Update the
`CALDATA_PATHS` variable in `rfeye-wmac-provision.sh` to match.

---

## Test 6: Verify WMAC Initialized

```bash
# Check phys
iw phy

# Check for ath9k spectral
ls /sys/kernel/debug/ieee80211/phy*/ath9k/spectral_scan_ctl 2>/dev/null

# Verify production radio still works
iw dev
# ath10k radio should still be on its mesh channel

# WMAC status
/usr/sbin/rfeye-agent wmac_status
```

**Expected:**
- phy1 exists with ath9k driver
- `spectral_scan_ctl` present under phy1
- ath10k production radio (phy0/wlan0) **completely unaffected**
- `wmac_status` shows `initialized: true`, `spectral_ready: true`

---

## Test 7: Spectral Scan Smoke Test

```bash
# Quick capture test
/usr/sbin/rfeye-agent pipeline_test

# If that works, run a short capture
/usr/sbin/rfeye-agent start 30
sleep 35
/usr/sbin/rfeye-agent capture_status
```

**Expected:** Frames captured > 0, spectral data flowing.

---

## Test 8: Production Radio Safety

```bash
# Verify mesh connectivity is still up
ping -c3 localnode 2>/dev/null || echo "mesh ping failed"

# Verify ath10k radio state
iw dev wlan0 info

# Check ath10k is still associated
iwinfo wlan0 assoclist 2>/dev/null | head -5
```

**Expected:** Mesh is up, ath10k radio operating normally.

---

## Test 9: Cleanup (Remove Caldata)

```bash
/usr/sbin/rfeye-agent wmac_remove

# Verify caldata removed
ls -la /lib/firmware/ath9k-eeprom-* 2>/dev/null || echo "caldata removed"

# Verify ath9k unloaded
lsmod | grep ath9k || echo "ath9k unloaded"
```

---

## Test 10: Reboot Persistence

```bash
# After provisioning, reboot the node
reboot

# After reboot, check if WMAC is up (caldata should persist in overlay)
/usr/sbin/rfeye-agent wmac_status
```

**Expected:** If `/lib/firmware/` is on overlay, caldata persists across
reboots and the WMAC should initialize automatically.

---

## Troubleshooting

### ath9k can't find firmware
Check `dmesg | grep -i firmware` for the exact path ath9k is looking for.
Common paths:
- `/lib/firmware/ath9k-eeprom-ahb-18100000.wmac.bin`
- `/lib/firmware/ath9k-eeprom-pci-0000:00:00.0.bin`
- Platform-specific path from DTS `compatible` string

If the path is different, update `CALDATA_PATHS` in `rfeye-wmac-provision.sh`
and reinstall.

### WMAC initializes but no spectral
The ath9k debugfs spectral interface requires kernel config `ATH9K_DEBUGFS`
and `ATH_SPECTRAL`. Check:
```bash
zgrep ATH9K_DEBUGFS /proc/config.gz 2>/dev/null
zgrep ATH_SPECTRAL /proc/config.gz 2>/dev/null
```

### Production radio affected
If the ath10k radio loses connectivity during ath9k module reload:
1. Run `wmac_remove` to unload ath9k
2. Check if ath10k needs to be re-associated: `wifi up`
3. Report the issue — this should not happen

### Caldata format mismatch
The reference caldata is from a WA board (QCA956x). If ath9k on the XC
board (QCA955x) rejects it, check `dmesg` for EEPROM parse errors. The
AR9300 template format should be compatible across QCA955x/QCA956x WMAC
silicon, but this is the first cross-SoC test.

---

## Success Criteria

| # | Test | Pass condition |
|---|------|---------------|
| 1 | DTS hooks present | WMAC DTS node visible |
| 2 | Baseline state | No WMAC phy, no caldata file |
| 3 | Install r17 | Package installs cleanly |
| 4 | Pre-provision status | `supported:true`, `initialized:false` |
| 5 | Provisioning | `ok:true`, WMAC phy appears |
| 6 | WMAC initialized | spectral_scan_ctl present |
| 7 | Spectral works | frames > 0 in capture |
| 8 | Production safe | ath10k mesh still up |
| 9 | Cleanup | Caldata removed, ath9k unloaded |
| 10 | Reboot persistence | WMAC survives reboot |

**Minimum pass:** Tests 1–6 and 8 all pass. Tests 7, 9, 10 are valuable
but not blockers for the PR #2730 compliance proof.
