# AREDN RFeye

AREDN RFeye is an AREDN-native RF spectrum visibility tool for ath10k-based 802.11ac nodes. The goal is to provide an AirView-like experience that fits inside an AREDN node, uses the AREDN app menu/UI conventions, and relies on OpenWrt `PACKAGE_ATH_SPECTRAL` instead of external SDR hardware.

This repository starts as a development scaffold. The first milestone is to prove that the target node exposes ath10k spectral scan controls and can safely display live RF/survey state from inside the node UI.

## Goals

- Fit inside an AREDN firmware image as a small OpenWrt package.
- Appear in the AREDN Apps menu using `/www/cgi-bin/apps/<app>/user`, `/www/cgi-bin/apps/<app>/admin`, and `/www/apps/<app>/icon.svg`.
- Use the same visual language as the AREDN node UI by loading `/a/css/theme.css`, `/a/css/user.css`, and `/a/css/mobile.css`.
- Probe ath10k spectral support through debugfs.
- Collect channel survey data through `iw` / nl80211.
- Add a live FFT/waterfall view after binary TLV parsing is implemented.
- Add practical interference classification for mesh troubleshooting.

## Non-goals

- This is not a calibrated lab spectrum analyzer.
- This is not a regulatory DFS/radar detector.
- This should not write continuous high-rate RF data to node flash.
- This should not channel-hop a production mesh radio without an explicit warning and operator action.

## Repository layout

```text
package/aredn-rfeye/        OpenWrt/AREDN package skeleton
package/aredn-rfeye/files/  Files installed into the node image
docs/                       Development approach, AREDN integration notes, OpenClaw brief
```

## Initial package contents

The first package scaffold installs:

- `rfeye-probe`: a small shell probe for debugfs, ath10k spectral controls, and survey counters.
- `rfeye` AREDN app entry under `/www/cgi-bin/apps/rfeye/`.
- `rfeye` icon under `/www/apps/rfeye/icon.svg`.
- `/etc/config/rfeye` default configuration.

## Build concept

In an AREDN/OpenWrt build tree, this package should eventually be included as a feed package and selected for supported ath10k AC targets only.

Required kernel/build features for real spectral capture:

```text
CONFIG_PACKAGE_ATH_DEBUG=y
CONFIG_PACKAGE_ATH_SPECTRAL=y
CONFIG_KERNEL_DEBUG_FS=y
CONFIG_KERNEL_RELAY=y
```

For ath10k AC devices, the node must also have the appropriate `kmod-ath10k` or `kmod-ath10k-ct` driver and firmware package.

## Current status

Scaffold only. The UI can probe support and display survey/debugfs state. The binary `spectral_scan0` TLV parser and live waterfall renderer are planned next.
