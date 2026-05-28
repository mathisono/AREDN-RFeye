# RFeye Node Compatibility

This matrix tracks node/driver compatibility for RFeye spectral capture. Tests must not change channels or enable channel hopping.

| Node | IP/host | AREDN version | Kernel | chipset/driver | spectral files present | survey counters | raw_capture_test frames | pipeline_test | heatmap_bundle | GUI | result | notes |
|---|---|---|---|---|---|---|---:|---|---|---|---|---|
| KJ6DZB-WSB-ACdish5 | 10.188.138.222 | AREDN 4.26.1.0 | 6.6.119 | ath10k / phy0 / wlan0 | ath10k `spectral_scan_ctl`, `spectral_scan0`, `spectral_bins` | survey exists; earlier utilization counters not reliable | 1 in r8/r9 raw test | r9 PASS | r9 PASS, waveform/waterfall/ambient populated; 10s timed capture reached 8 frames / 7 waterfall rows | CGI PASS; GUI status fields added, manual browser visual pending | r9 PASS | Channel 141 / 5705 MHz / 20 MHz; no channel changes. |
| KJ6DZB-WSB-ACdish5 (WMAC) | 10.189.138.222 | AREDN+PR2725 | 6.12.87 | ath9k / phy1 (no iface) | ath9k `spectral_scan_ctl`, `spectral_scan0`, `spectral_count`, `spectral_period`, `spectral_fft_period`, `spectral_short_repeat` | N/A | 51KB per trigger | N/A | N/A | N/A | PASS | Template EEPROM fallback. No factory caldata. chanscan mode works. Channel 36 HT20 verified. AR9550 Rev:0 3x3 MIMO. |
