# WA AirView Runtime Probe

> **Date:** 2026-05-29  
> **Target:** PowerBeam 5AC Gen2 (PBE-5AC-Gen2), WA board, AirOS WA.v8.7.11  
> **IP:** 10.108.120.18 (AREDN mesh, via MSE-88 jump host)  
> **Condition:** AirView actively running in the web UI during probe

---

## 1. The AirView Daemon

Stock AirOS runs a proprietary spectral scan daemon at boot:

```
PID 819: /bin/ubntspecd -w -t -a -i wifi1 -j airview1 -m 4 -f
```

| Flag | Likely meaning |
|------|---------------|
| `-w` | Wideband mode |
| `-t` | Timed / continuous |
| `-a` | Auto channel sweep |
| `-i wifi1` | Radio interface (WMAC) |
| `-j airview1` | Monitor VAP |
| `-m 4` | Scan mode (mode 4) |
| `-f` | Foreground |

The daemon is a **boot-time service** — it was running with the same PID
both before and after AirView was opened in the web UI. Opening AirView
in the browser connects to the already-running daemon; it does not start
a new process.

System config confirms:
```
airview.status=enabled
```

---

## 2. The airview1 Interface

```
airview1  IEEE 802.11na  ESSID:"spectral"
          Mode:Monitor  Frequency:4.92 GHz  Access Point: Not-Associated
          Bit Rate:130 Mb/s   Tx-Power=13 dBm
          RTS thr:off   Fragment thr:off
          Center1-Freq: 4.92 GHz
          Encryption key:off
          Power Management:off
          Link Quality=0/94  Signal level=-96 dBm  Noise level=-103 dBm
          Rx invalid nwid:0  Rx invalid crypt:0  Rx invalid frag:0
          Tx excessive retries:0  Invalid misc:0   Missed beacon:0
```

Key observations:

- **4.92 GHz parking frequency** — below any real Wi-Fi band. The WMAC
  sits at this out-of-band frequency when idle. `ubntspecd` controls the
  actual sweep; the iwconfig frequency display does not update during
  scanning.

- **Zero RX counters** — `airview1` shows 0 bytes / 0 packets received
  in `/proc/net/dev`, even while AirView is actively scanning. The
  spectral data path does **not** flow through the normal network stack.

- **Noise floor: -103 dBm** — this is the idle noise floor at the
  parking frequency, not a swept measurement.

---

## 3. No Change Between Idle and Active AirView

| Metric | Before AirView UI | During AirView UI | Changed? |
|--------|-------------------|-------------------|----------|
| `airview1` present | Yes | Yes | No |
| `airview1` frequency | 4.92 GHz | 4.92 GHz | No |
| `ubntspecd` running | Yes (PID 819) | Yes (PID 819) | No |
| `wifi1` RX packets | 32 | 32 | No |
| `airview1` RX packets | 0 | 0 | No |
| Signal level | -96 dBm | -96 dBm | No |
| Noise level | -103 dBm | -103 dBm | No |

The SSH-visible state is **identical** whether or not the AirView web UI
is open. The daemon runs continuously; the web UI is just a client that
reads from it.

---

## 4. Proprietary Driver Stack

The spectral data flow on stock AirOS is completely different from
AREDN/OpenWrt's mainline ath9k:

| Component | Stock AirOS (WA) | AREDN/OpenWrt |
|-----------|-----------------|---------------|
| **Driver** | `ubnthal` + `ath_hal` + `ath_spectral` (proprietary) | `ath9k` (mainline Linux) |
| **Daemon** | `/bin/ubntspecd` | RFeye agent (`rfeye-survey`) |
| **Data path** | Internal driver ioctl / custom API | debugfs `spectral_scan0` |
| **Control** | `ubntspecd` flags | `spectral_scan_ctl` sysfs |
| **Visibility** | Invisible from shell | Readable from debugfs |

Key differences:

- **No debugfs/procfs spectral files** — `find /sys -name "*spectral*"`
  and `find /proc -name "*spectral*"` return nothing. Ubiquiti's driver
  does not expose the standard Linux wireless spectral scan interface.

- **No `/sys/class/net/wifi1/phy80211`** — the Ubiquiti driver stack does
  not use the standard `cfg80211`/`mac80211` sysfs hierarchy.

- **Custom kernel modules** — the driver stack consists of:
  ```
  ubnthal    395766  (9 dependents — core Ubiquiti HAL)
  ath_hal    328906  (Atheros HAL, proprietary)
  ath_dev    221833  (device layer)
  ath_spectral 24777 (spectral scan engine)
  ath_dfs   1188973  (DFS radar detection)
  umac               (upper MAC)
  ath_rate_atheros    (rate control)
  ```

- **`ubntspecd` talks directly to `ubnthal`** — the spectral data never
  touches the network stack, which is why `/proc/net/dev` shows zero
  traffic on `wifi1` and `airview1`.

---

## 5. Implications

### For RFeye

RFeye on AREDN uses the mainline `ath9k` driver with standard Linux
debugfs spectral scan files. This is a completely different code path
from Ubiquiti's `ubntspecd` + `ubnthal`. The two implementations share
the same concept (WMAC monitor interface for spectral scanning) and the
same hardware (QCA9558 on-chip radio), but nothing else.

This means:
- We cannot learn how `ubntspecd` handles caldata by watching it from the shell
- The spectral data format may be different from ath9k's debugfs format
- Ubiquiti may apply their own gain corrections or calibration in `ubntspecd`
  that are invisible to us

### For the Caldata Question

The fact that `ubntspecd` runs at boot with `airview.status=enabled` and
uses the WMAC with factory caldata at 0x1000 confirms:

1. **Ubiquiti considers the WMAC caldata important enough to factory-write it**
   on WA boards — even for a listen-only spectral scanner.

2. **The WA WMAC caldata is used in production** by `ubntspecd`. It's not
   vestigial or test-only data.

3. **The XC omission is deliberate** — if Ubiquiti wanted AirView on XC
   boards, they would have calibrated the WMAC at the factory.

### For XC Stock Firmware Testing

To determine whether XC stock firmware has `ubntspecd` / AirView at all,
check for:
- `/bin/ubntspecd` binary
- `airview.status` in `/tmp/system.cfg`
- `airview1` interface in `iwconfig`
- `radio.2` entries in `/etc/board.info`

If any of these are absent on XC stock firmware, it confirms AirView is a
WA-only feature.
