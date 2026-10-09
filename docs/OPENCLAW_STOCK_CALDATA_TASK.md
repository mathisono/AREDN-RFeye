# OpenClaw Prompt — Stock WMAC Caldata Reconnaissance

Use this prompt when handing the next RFeye caldata-research milestone to OpenClaw.

```text
You are working in the GitHub repository mathisono/AREDN-RFeye.

Mission:
Implement the stock-firmware WMAC caldata reconnaissance milestone for RFeye. The goal is to prove, from same-device stock Ubiquiti firmware evidence, whether a target board has transferable WMAC caldata, uses a stock HAL/default-template fallback, or has no valid stock WMAC calibration path. Do not treat random data or wrong-radio PCI data as production caldata.

Current project context:
- RFeye is an AREDN/OpenWrt node-side RF spectrum visibility app.
- Current focus is safe WMAC caldata provisioning, ath9k spectral parsing, and live browser rendering.
- AREDN PR #2730 enables the QCA9558 WMAC with qca,no-eeprom, so Linux ath9k expects a firmware-loader file at /lib/firmware/ath9k-eeprom-ahb-18100000.wmac.bin.
- Existing RFeye safety policy says never write ART/EEPROM flash, never feed blank caldata, never feed random bytes, and never use PCI radio ART+0x5000 as the production WMAC source.
- The repo already has a C helper rfeye-wmac-rebind and a WA-reference caldata path for bench testing, but the provenance/validation story needs to be tightened.

Primary deliverables:
1. Add or refine a read-only stock probe script.
   - Path: scripts/rfeye-stock-caldata-probe.sh
   - BusyBox/ash compatible.
   - Must run on stock Ubiquiti AirOS and OpenWrt/AREDN lab nodes.
   - Must be read-only: no flash writes, no caldata install, no radio rebind, no channel changes.
   - Must capture before-state metadata and optionally after-AirView metadata.
   - Must dump the selected EEPROM/ART MTD partition into /tmp only.
   - Must extract at least:
     - full first 64 KiB EEPROM/ART dump
     - wmac_0x1000_1024.bin
     - wmac_0x1000_4096.bin
     - pci_0x5000_4096.bin
   - Must hash dumped binaries when sha256sum or md5sum is available.
   - Must classify extracted slices as one of:
     - blank_ff
     - blank_00
     - ar9300_template_0202_candidate
     - ar9300_compressed_4408_candidate
     - unknown
   - Must produce a tar.gz bundle under /tmp unless --no-tar is set.
   - Must warn users not to publish full EEPROM/ART bundles publicly.

2. Add documentation.
   - Path: docs/STOCK_CALDATA_RECON_TEST_PROGRAM.md
   - Explain the purpose, safety rules, target test matrix, how to run the script, what files are captured, classification rules, decision tree, and AREDN bench validation path.
   - Make clear that the goal is not just to find bytes that make ath9k load; the goal is to prove stock same-device behavior.
   - Explicitly separate same-device stock WMAC data, stock default-template fallback, WA-reference bench data, and wrong-radio PCI 0x5000 bench-only data.

3. Add or update an OpenClaw task prompt document.
   - Path: docs/OPENCLAW_STOCK_CALDATA_TASK.md
   - Include this prompt or a cleaner version of it so the milestone can be repeated.

4. Consider follow-up implementation notes for RFeye proper.
   - Recommend a future rfeye-caldata-validate command that emits JSON.
   - Recommend atomic firmware-file install using temp file + fsync + rename.
   - Recommend explicit caldata modes in /etc/config/rfeye:
     - stock_same_device
     - stock_default_template
     - wa_reference_bench
     - pci_0x5000_bench_wrong_radio
     - disabled
   - Require a bench-only override before allowing wrong-radio PCI 0x5000 data.

Acceptance criteria:
- sh -n scripts/rfeye-stock-caldata-probe.sh passes.
- The script is POSIX/BusyBox ash friendly; avoid bash arrays, process substitution, jq, Python, Perl, or GNU-only assumptions.
- The script writes only under /tmp or the user-specified output directory.
- The script does not call mtd write, flashcp, dd with an output device under /dev/mtd*, ubus config changes, uci changes, ifconfig down/up, iw channel changes, modprobe/rmmod, or platform driver bind/unbind.
- The docs clearly state that full EEPROM/ART bundles may contain identifiers and should not be posted publicly without review.
- The docs give a decision tree for these four outcomes:
  1. same-device stock 0x1000 has real WMAC data;
  2. same-device stock 0x1000 is blank but AirView works;
  3. stock has no AirView path and 0x1000 is blank;
  4. EEPROM changes after AirView starts.
- Do not modify unrelated RFeye UI/parser code in this milestone.
- Do not change network configuration, AREDN node configuration, or repository release packaging unless necessary for the new script/documentation.

Validation commands to run locally:
sh -n scripts/rfeye-stock-caldata-probe.sh
grep -n "mtd write\|flashcp\|/dev/mtd.*of=\|rmmod\|modprobe\|bind\|unbind\|uci set\|iw .* set channel" scripts/rfeye-stock-caldata-probe.sh && echo "review unsafe command matches" || true

Expected final summary:
- List files changed.
- Confirm sh -n passed.
- Confirm the script is read-only by inspection.
- Explain any intentional limitations, especially that structural classification is not RF calibration validation.
```
