# AREDN RFeye Roadmap

## Phase 1 (done/in progress)

- [x] Package scaffold
- [x] Node probe (`rfeye-probe`)
- [x] Parser MVP for ath10k TLV (`ATH_FFT_SAMPLE_ATH10K`)
- [x] Admin snapshot CGI endpoint

## Phase 2 (node service hardening)

- [x] `rfeye-agent` daemon skeleton (`status/start/stop/snapshot`)
- [x] `/etc/config/rfeye` defaults and strict limits
- [x] time-limited capture windows
- [ ] RAM-only ring buffer
- [x] memory guardrails (hard capture byte caps)
- [ ] graceful unsupported/fail-closed behavior (baseline checks added)

## Phase 2.5: WMAC wideband scanner (ath9k)

Enabled by AREDN PR #2725 device tree patch on Ubiquiti XC boards (PowerBeam 5AC 500, Rocket 5AC Lite, NanoBeam AC XC). The QCA9558 on-chip WMAC initializes via ath9k template EEPROM fallback (no factory caldata), providing a dedicated spectral scanner radio independent of the ath10k mesh radio.

- [x] Verify WMAC initialization via template EEPROM fallback
- [x] Confirm ath9k spectral scan produces valid FFT data (51 KB per trigger)
- [x] Confirm ath10k mesh radio unaffected during WMAC spectral scan
- [x] Add ath9k TLV parsing (type 1 HT20 / type 2 HT40) to parser
- [x] Update probe to detect ath9k spectral capability
- [x] Update agent driver detection (ath_dir auto-detect)
- [ ] Wideband sweep via chanscan mode
- [ ] Multi-channel waterfall display
- [ ] Channel-hopping scan schedule (WMAC only, never ath10k)

## Phase 3 (data APIs)

- [ ] low-rate JSON snapshot endpoint (1–4 FPS friendly)
- [ ] timed capture export endpoints (`tlv`, `jsonl`)
- [ ] optional ubus API mapping (`spectrald.*`)

## Phase 4 (node UI)

- [ ] support status panel
- [ ] FFT trace panel
- [ ] lightweight waterfall panel
- [ ] utilization + external busy estimate
- [ ] noise trend + event list
- [ ] explicit warnings and limitations text

## Phase 5 (Linux Workbench)

- [ ] capture pull + archive format
- [ ] replay engine
- [ ] full waterfall/max-hold/average
- [ ] classifier tuning workflows
- [ ] reports/screenshots export
