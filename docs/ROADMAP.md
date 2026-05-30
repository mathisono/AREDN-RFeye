# RFeye Development Roadmap

> **Last updated:** 2026-05-29

---

## Current: r16 (Shipped)

WMAC wideband spectral scanner working on XC boards with bench-only PCI
caldata fallback. Single-channel ath10k spectral on all AREDN ath10k nodes.

---

## Next: r17 — Caldata Self-Provisioning for AREDN PR #2730

### The AREDN Integration Model (PR #2730, merged)

[AREDN PR #2730](https://github.com/aredn/aredn/pull/2730) provides the
hooks for RFeye to operate the WMAC spectrum radio on supported Ubiquiti
AC boards. The design is a clean separation of concerns:

**What AREDN provides (PR #2730):**
- DTS entries that enable the WMAC (`&wmac { status = "okay"; }`) on:
  - Rocket 5AC Lite (`qca9558_ubnt_rocket-5ac-lite.dts`)
  - PowerBeam 5AC 500 (`qca9558_ubnt_powerbeam-5ac-500.dts`)
- `qca,no-eeprom` flag — tells ath9k to look for caldata on the
  **filesystem** instead of in the ART/EEPROM partition
- Hardware config entries for `wlan1` with `"disabled": true`
- The WMAC will **not initialize by default** because no caldata file
  is present — it will fail silently until an app provides one

**What RFeye provides (r17):**
- The correct caldata firmware file for the target board's WMAC
- Installation of the firmware file to the expected filesystem path
- Reload of the ath9k module (or require a reboot) to re-initialize
  the WMAC with the newly-available caldata
- All spectral scanning functionality via the initialized WMAC

### r17 Implementation Tasks

1. **Determine the ath9k firmware file path and format**
   - When `qca,no-eeprom` is set, ath9k looks for caldata at a specific
     path under `/lib/firmware/` (e.g., `ath9k-caldata-<phy>.bin` or
     the path specified in the DTS)
   - Verify the exact path the kernel expects on AREDN builds

2. **Source valid WMAC caldata**
   - XC boards (PBE-5AC-500, R5AC-Lite): ART+0x1000 is blank — no
     factory caldata exists. Options:
     - Use the ath9k `ar9300_default` built-in template (generic but
       correct radio type)
     - Use WA board caldata as a cross-board reference (available in
       `artifacts/eeprom/eeprom_wa_pbe5ac_gen2.bin` at offset 0x1000)
     - Generate a caldata file from runtime NF calibration
   - WA boards (if supported later): extract from ART+0x1000 directly

3. **RFeye agent: caldata provisioning flow**
   ```
   rfeye-agent probe
     → detect board type (XC/WA) and WMAC state
     → if WMAC not initialized and caldata file missing:
       → install appropriate caldata to /lib/firmware/...
       → reload ath9k module (rmmod + modprobe) or flag reboot needed
     → if WMAC initialized:
       → proceed with spectral scanning as r16
   ```

4. **Package the caldata**
   - Include caldata file(s) in the RFeye IPK
   - Or download on first run from a known source
   - Document provenance and limitations (bench-only vs. production)

5. **Safety: do not break the production radio**
   - ath9k module reload must not affect the ath10k PCI radio
   - If ath9k reload fails, RFeye should report the error and continue
     with single-channel ath10k mode only
   - Never write to flash — caldata goes to tmpfs or a persistent
     overlay path that doesn't risk bricking

### r17 Caldata Safety Rules (unchanged)

1. Do not feed blank ART+0x1000 (all 0xFF)
2. Do not feed random bytes
3. Do not use PCI caldata (ART+0x5000) as production WMAC source
4. Find a valid WMAC-calibrated source
5. Treat any substitute caldata as bench-only until confirmed valid

---

## Future: r18+ — Accuracy and Calibration

### Runtime Calibration Improvements
- Read ath9k `dump_nfcal` per channel, apply to spectral power formula
- Cross-radio offset table (ath10k vs ath9k on shared channel)
- Per-channel gain correction from empirical measurements

### Extended Hardware Support
- NanoBeam AC XC (`qca9558_ubnt_nanobeam-ac-xc.dts`) — needs DTS entry
  in AREDN, same caldata model as PR #2730
- WA boards — extract and use factory caldata from ART+0x1000

### Ubiquiti Spectral Pipeline Recovery (Future Project)
- Reverse engineer `ubntspecd` binary and `ubnthal` kernel module
- Recover the proprietary spectral data format and calibration pipeline
- Reimplement as open-source for AREDN
- See `docs/WA_AIRVIEW_RUNTIME_PROBE.md` Section 6

---

## Reference: AREDN Integration History

| PR | Status | Description |
|----|--------|-------------|
| [#2725](https://github.com/aredn/aredn/pull/2725) | Superseded | WMAC DTS enablement with shared PCI caldata at ART+0x5000 |
| [#2730](https://github.com/aredn/aredn/pull/2730) | **Merged** | WMAC DTS hooks with `qca,no-eeprom` — caldata from filesystem, app-provisioned |

PR #2730 supersedes PR #2725. The key change: AREDN provides the DTS hooks,
RFeye provides the caldata. The WMAC does not initialize until an app
(RFeye) installs the appropriate firmware file.

---

## Reference: Research Documents

| Document | Contents |
|----------|----------|
| [`The XC vs. WA Divide`](The%20XC%20vs.%20WA%20Divide.md) | Hardware comparison, EEPROM analysis, caldata presence |
| [`T0_STOCK_PROBE_RESULTS`](T0_STOCK_PROBE_RESULTS.md) | Stock WA firmware probe — AirView, factory caldata |
| [`WA_AIRVIEW_RUNTIME_PROBE`](WA_AIRVIEW_RUNTIME_PROBE.md) | ubntspecd runtime analysis, proprietary driver stack |
| [`WMAC_CALDATA_RESEARCH_AND_TESTING_PLAN`](WMAC_CALDATA_RESEARCH_AND_TESTING_PLAN.md) | Full caldata research, safety rules, testing phases |
| [`ONCHIP_SCANNER_RADIO_RESEARCH`](ONCHIP_SCANNER_RADIO_RESEARCH.md) | QCA9558 WMAC hardware research |
