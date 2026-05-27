# AirView Dual-Radio Spectral Architecture — Findings

> **Date:** 2026-05-26
> **Device under observation:** Ubiquiti Rocket 5AC Lite (R5AC-Lite)
> **Method:** Clean-room behavioral observation via SSH — no code was copied or decompiled.

## Summary

AirView on the Rocket 5AC Lite uses a **dedicated second radio** for spectral scanning.
The main AP radio is never taken off-channel. A separate on-chip radio sweeps the
entire 5 GHz band, collects per-channel FFT snapshots through a hardware spectral
engine, and stitches the results into a wideband power display covering roughly
5100–5900 MHz.

---

## Dual-Radio Architecture

The R5AC-Lite contains two independent radios:

| Radio | Bus | Role | Interface | Mode |
|-------|-----|------|-----------|------|
| `wifi0` | PCI (Ubiquiti vendor 0x0777) | Main AP — airMAX/802.11ac | `ath0` | Master, on operating channel (5.745 GHz observed) |
| `wifi1` | AHB (on-chip, QCA9560 SoC) | Spectral scanner only | `airview1` | Monitor, sweeps 5095–5904 MHz |

Key observations:

- `ath0` remains UP/RUNNING/BROADCAST on the production channel at all times.
- `airview1` is created as `IEEE80211_M_MONITOR` with ESSID `"spectral"`.
- The AP link does **not** drop while AirView runs — scanning is on a separate radio.

---

## Scanner Daemon: `ubntspecd`

```
/bin/ubntspecd -w -t -a -i wifi1 -j airview1 -m 4 -f 20 -s 200
```

Likely parameter roles (inferred from observation, not from source):

| Flag | Observed value | Probable role |
|------|---------------|---------------|
| `-i` | `wifi1` | Target radio |
| `-j` | `airview1` | Monitor VAP interface |
| `-m` | `4` | Scan/sweep mode |
| `-f` | `20` | FFT size or related param |
| `-s` | `200` | Sweep interval or step count |
| `-w` | — | Possibly WebSocket enable |
| `-t` | — | Possibly timed/continuous mode |
| `-a` | — | Possibly auto/all-channel mode |

`ubntspecd` (PID 525) file descriptors:

| FD | Target |
|----|--------|
| 6 | `/dev/uph_wifi0` — chardev major 190, `ubnt_poll_host` kernel module |
| 3, 4 | Sockets (netlink to ath_spectral, control) |
| 7 | TCP listener socket (WebSocket server) |

---

## Private Driver Path

### `/dev/uph_wifi0` and `ubnt_poll_host`

- Char device, major 190, registered by `ubnt_poll_host` kernel module.
- `ubnt_poll_host` also manages airMAX TDMA polling for the production radio.
- `ubntspecd` opens this device for radio control — likely channel-set commands
  and spectral scan trigger/readback.
- This is **not** a standard Linux wireless interface — it is a proprietary
  Ubiquiti kernel driver path.

### `ath_spectral` Module

- Kernel module `ath_spectral` v2.0.0, loaded at boot.
- Attaches to both radios; active scanning observed on `wifi1`.
- Registers GPIO interrupt `ath-spectral-filter` (IRQ 64, ATH GPIO).
- Interrupt rate: ~1,348/sec during active scanning.
- Uses netlink for spectral data delivery to userspace (`spectral_init_netlink`
  observed in dmesg).
- Module parameters: `maxholdintvl`, `nfrssi`, `nobeacons` (all 0).
- dmesg notes: `HAL_CAP_SPECTRAL_SCAN : Capable` and
  `SPECTRAL : No ADVANCED SPECTRAL SUPPORT` (older/simpler spectral mode).

### Data Path

```
QCA9560 HW FFT engine
  → ath-spectral-filter GPIO IRQ (~1348/sec)
  → ath_spectral kernel module
  → netlink
  → ubntspecd (userspace)
  → JSON file (/tmp/airview/data)
  → WebSocket (127.0.0.1:50841)
  → lighttpd proxy (/ws/airview)
  → browser UI
```

---

## JSON Data Products

`/tmp/airview/data` (~134 KB, updated continuously):

### Frequency Grids

| Key | Points | Range | Spacing |
|-----|--------|-------|---------|
| `stFreqGridLabels` | 648 | 5095.6 – 5904.4 MHz | 12.5 kHz |
| `mtFreqGridLabels` | 161 | 5097.5 – 5897.5 MHz | ~50 MHz |
| `ltFreqGridLabels` | 161 | 5097.5 – 5897.5 MHz | ~50 MHz |

(Frequency labels are encoded as integers in units of 10 kHz, e.g. `50956250` = 5095.625 MHz.)

### Power and Histogram Data

| Key | Shape | Description |
|-----|-------|-------------|
| `latestPower[648]` | 1D | Most recent power per frequency bin |
| `instantPower[8][648]` | 2D | Last 8 sweep frames |
| `pwrHistogram[64][648]` | 2D | 64 power levels × 648 freq bins |
| `totalSamples[648]` | 1D | Cumulative sample count per bin |

### Waterfall / Time-Series

| Key | Shape | Description |
|-----|-------|-------------|
| `mediumAnze[24][161]` | 2D | Medium-term waterfall, 60 s update interval |
| `latestAnze[24][161]` | 2D | Long-term waterfall, 2560 s update interval |

### Scalar Metadata

| Key | Observed value | Description |
|-----|---------------|-------------|
| `stFreqGridNumPts` | 648 | Short-term bin count |
| `stPowrMin` | -125 | Minimum power level (dBm or relative) |
| `stPowrNum` | 64 | Number of power histogram levels |
| `stPowrDel` | 2 | Power level step size |
| `mtUpdateInterval` | 60 | Medium-term update interval (seconds) |
| `ltUpdateInterval` | 2560 | Long-term update interval (seconds) |
| `instantPowerNumFrames` | 8 | Frames in instantPower ring |
| `mtTimeNum` | 24 | Medium-term waterfall time slots |
| `ltTimeNum` | 24 | Long-term waterfall time slots |

---

## Sweep Behavior Observations

- All 648 frequency bins from 5095–5904 MHz contain data after a few seconds of operation.
- ~1,728 new FFT bin samples per second across the 648-bin grid.
- 540 of 648 `latestPower` bins changed within a 3-second window.
- `iwconfig airview1` reports a static 4.92 GHz — channel changes happen faster
  than shell polling can observe.
- `airview1` channel list spans 1,026 channels from 2.377 GHz to 6.1 GHz
  (vendor-unlocked range, far beyond regulatory allocation).

---

## Conclusion

AirView on the Rocket 5AC Lite achieves wideband spectral display by
**sweep-and-stitch on a dedicated scanner radio**:

1. The QCA9560 SoC on-chip radio (`wifi1`) runs a monitor-mode VAP (`airview1`).
2. `ubntspecd` tunes this radio through each 5 GHz channel in rapid succession.
3. The `ath_spectral` hardware FFT engine captures spectral samples on each channel.
4. Samples are delivered via netlink to `ubntspecd`, which stitches them into a
   648-bin frequency grid covering ~810 MHz of bandwidth.
5. The main PCI radio (`wifi0` / `ath0`) continues serving the AP with zero interruption.

This is **not** an instantaneous 800 MHz capture. It is a time-interleaved sweep
that produces the illusion of wideband coverage by rapidly cycling through channels.

---

## Implications for RFeye

- An AirView-like wideband display on AREDN hardware would require either a
  dedicated scanner radio or deliberate retune-and-stitch on the production radio
  (which disrupts AP service).
- The PBE-5AC-500 and similar single-radio devices likely cannot replicate this
  architecture without taking the AP off-channel.
- RFeye's current-channel-only production mode remains the correct default.
- Wideband research/bench modes should be kept separate from production.
- WMI knobs (spectral bins, FFT size, scan count) improve each per-channel FFT
  slice but do not provide cross-channel sweep by themselves.

---

## Dual-Radio Fingerprint (Reference for Comparison)

The following signatures identify the R5AC-Lite as a dual-radio device.
Use these as a baseline when checking other hardware (e.g. PBE-5AC-500).

### board.info
```
board.phycount=1          # misleading — counts only the "primary" radio
radio.1.bus=pci            # wifi0 — main AP radio
radio.2.bus=ahb            # wifi1 — on-chip scanner radio
```

### /sys/class/net
```
wifi0 -> ../../devices/pci0000:00/0000:00:00.0/net/wifi0   # PCI radio
wifi1 -> ../../devices/virtual/net/wifi1                    # AHB/on-chip radio
ath0  -> ../../devices/virtual/net/ath0                     # AP VAP on wifi0
airview1 -> ../../devices/virtual/net/airview1              # Monitor VAP on wifi1
```

### PCI bus
```
0000  077711ac  ...  ath_pci       # only one PCI device — wifi0
```
(wifi1 is AHB-attached, not on PCI.)

### Kernel modules
```
ath_spectral  24777  3  umac,ath_dev   # spectral FFT module
ubnt_poll_host 153289 2                # airMAX polling + chardev control
ath_pci                                # PCI radio driver (wifi0 only)
```

### dmesg signatures
```
ath_pci_probe                          # PCI radio probe
wifi1: Atheros ???: mem=0xb8100000, irq=2   # AHB radio at fixed MMIO
ath_spectral: Version 2.0.0
IRQ request for SPECTRAL-XMIT-FILTER successful   # on wifi1
VAP device airview1 created            # monitor VAP on wifi1
```

### /proc/interrupts
```
64:  xxxxxxx  ATH GPIO  ath-spectral-filter   # continuous, ~1300/sec
75:  xxxxxxx  ATH PCI   wifi0                 # main radio
```

### What a single-radio device would look like
- Only `wifi0` in `/sys/class/net`, no `wifi1`
- No `radio.2` section in `board.info`
- No `ath-spectral-filter` GPIO interrupt
- No `airview1` interface
- Only one PCI device in `/proc/bus/pci/devices`
- No AHB radio in dmesg (`wifi1: Atheros ...` absent)

---

## Legal and Ethical Note

All findings in this document are from behavioral observation of a running device
via standard SSH shell commands (`ps`, `iwconfig`, `dmesg`, `cat`, `hexdump`,
`ls`, `/proc` filesystem). No proprietary Ubiquiti source code or firmware binaries
were copied, decompiled, or reverse-engineered. String fragments visible in hexdump
output are limited to format-string identifiers used to understand data structure
naming, consistent with clean-room interoperability research.
