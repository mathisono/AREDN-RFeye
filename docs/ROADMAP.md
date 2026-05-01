# AREDN RFeye Roadmap

## Phase 1 (done/in progress)

- [x] Package scaffold
- [x] Node probe (`rfeye-probe`)
- [x] Parser MVP for ath10k TLV (`ATH_FFT_SAMPLE_ATH10K`)
- [x] Admin snapshot CGI endpoint

## Phase 2 (node service hardening)

- [ ] `rfeye-agent` / `spectrald` daemon skeleton
- [ ] `/etc/config/rfeye` defaults and limits
- [ ] time-limited capture windows
- [ ] RAM-only ring buffer
- [ ] rate throttling and memory guardrails
- [ ] graceful unsupported/fail-closed behavior

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
