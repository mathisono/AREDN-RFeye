# RFeye UI Notes (r11)

## Page layout

The node UI is a single scrollable page (`/cgi-bin/apps/rfeye/user`) with compact cards at the top and three visualization panels below.

- Global page scrolling is enabled for desktop/mobile (`html, body { min-height:100%; overflow-y:auto; }`).
- Panels do not lock viewport scrolling.
- Raw JSON is collapsed by default under `<details>`.

## Top structure

Top section is split into three compact cards:

1. **Controls**
   - Start / Stop / Reset
   - PHY, Bins, Duration selectors
   - Capture state badge
   - Elapsed/remaining, frame rate, last-frame age

2. **Radio (trusted)**
   - Channel, frequency, width, mode/IBSS
   - Interface/PHY
   - Source of trusted radio data

3. **Diagnostics**
   - Frames captured, no-frame count
   - Waterfall rows, ambient rows
   - Storage usage, parser mode
   - Collapsible raw JSON/details

## Waveform meaning

Waveform is a spectrum-style trace of current normalized bins.

- Y-axis labels show display min/mid/max dBm range.
- X-axis labels use trusted radio frequency + width when available.
- Center-frequency marker is shown.
- Channel edges are indicated by left/right plot boundaries.
- No-data message is shown when no bins are available.

## Waterfall meaning

Waterfall is a rolling row-based heat map of recent rows.

- Newest row is rendered at the **bottom** (explicitly labeled).
- Color legend uses dark blue/black → blue → green → yellow/orange → red/white.
- Min/mid/max legend labels track active display scale.
- Visible row count is shown.
- No-data state is shown when empty.

## Ambient meaning

Ambient panel is labeled **Ambient / slower noise history**.

- Uses minute-peak style history with current in-progress row support.
- Row count is shown.
- No-data state is shown when empty.
- Does not require waiting a full minute before any visual appears when pending row exists.

## Display scaling (relative power)

Display scaling is for operator contrast tuning, not lab calibration.

Controls:

- **Auto scale**: 5th/95th percentile over visible bins, minimum 20 dB span.
- **Manual min/max**: direct dBm entry.
- **Reset scale**: restores backend/default scale.

Default fixed range is typically `-120 .. -60 dBm`.

## Known limitations

- Display values are approximate/relative dB from normalized spectral bins.
- RFeye remains current-channel only; no channel hopping/changing is performed.
- Ambient remains lightweight minute-peak history, not long-term calibrated RF analytics.
