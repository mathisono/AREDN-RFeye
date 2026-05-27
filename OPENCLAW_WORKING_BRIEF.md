# OpenClaw Working Brief

## Project

AREDN-RFeye

RFeye is a node-side AREDN/OpenWrt RF spectrum visibility prototype for ath10k-based 802.11ac radios. The project goal is an AirView-like node test view, with heavier analysis and reporting deferred to a future Linux Workbench.

## Start here

Do **not** add new RF features yet.

The next OpenClaw session should focus on the intermittent capture/feed stall. r12 already fixed the stale packaged-parser issue, so do not spend time re-litigating the r11 parser mismatch unless the node parser again lacks `--probe` / `--resync`.

Immediate next action:

1. Confirm parser source sync.
2. Confirm installed parser supports `--probe` and `--resync`.
3. Verify the spectrum flow path end-to-end.
4. Reproduce or catch the intermittent stall.
5. Save a triage report.
6. Only then decide whether to add a minimal watchdog/re-prime fix.

## Active milestone

r12 complete; intermittent capture/feed stall triage in progress.

## Guardrails

- No classifier work yet
- No channel hopping
- No channel changes
- Captures remain under `/tmp/rfeye`
- No continuous capture writes to flash
- End state after stop/reset must be `spectral_scan_ctl=disable`
- Do not add new features until the intermittent feed stall is understood

## Known-good state

### r10 — last fully proven stability milestone

Results on KJ6DZB-WSB-ACdish5:

- 5-minute manual run: PASS
- `frames_captured=80`
- `no_frame_count=0`
- `waterfall_rows=80`
- `/tmp/rfeye` about 772 KiB
- `soak_test 300 128 phy0`: PASS
- `soak_test frames_captured=100`
- `state_dir_bytes=823296`
- final `spectral_scan_ctl=disable`
- no channel hopping or channel changes

### r11 — GUI/display work

r11 added:

- page scrolling
- compact Controls / Radio / Diagnostics cards
- collapsible raw JSON
- waveform labels and trusted radio frequency context
- waterfall and ambient legends / no-data states
- auto/manual display scaling controls
- improved bundle metadata

r11 source/build validation passed, but live acceptance was intermittent.

## Next minimal code-change target (not yet applied)
1. Synchronize parser source used by package build (ensure compiled source includes `--resync/--probe`).
2. Rebuild IPK and verify parser `--help` on node includes `--resync/--probe`.
3. Re-run backend + GUI live acceptance.

## AirView Architecture Finding (2026-05-26)

Behavioral observation of a Rocket 5AC Lite confirmed that Ubiquiti AirView uses
a **dedicated second on-chip radio** (wifi1/AHB) for spectral scanning while the
main PCI radio (wifi0) continues serving the AP. The scanner sweeps ~5100–5900 MHz
and stitches per-channel FFT slices into a wideband display.

Implications for RFeye:

- **RFeye production mode remains current-channel only.** Single-radio AREDN
  devices (PBE-5AC-500, etc.) cannot replicate AirView wideband without dropping
  the AP link.
- **AirView research mode is separate and bench-only.** Any retune-and-stitch
  experimentation is restricted to lab hardware not serving live mesh traffic.
- **WMI knobs remain useful** for improving per-channel FFT quality.
- **PBE-5AC-500 confirmed single-radio** (2026-05-26) — QCA9558 SoC + QCA988x on
  PCIe, one phy, no second scanner radio exposed. AirView-like wideband is not
  feasible without AP disruption.

See: `docs/AIRVIEW_ARCHITECTURE_FINDINGS.md` and `docs/WIDEBAND_SPECTRAL_WORKING_BRIEF.md`
