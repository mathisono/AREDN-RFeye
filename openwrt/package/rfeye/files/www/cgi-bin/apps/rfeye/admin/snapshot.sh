#!/bin/sh

echo "Content-Type: application/json"
echo "Cache-Control: no-store"
echo

PHY="${QUERY_STRING#*phy=}"
if [ "$PHY" = "$QUERY_STRING" ] || [ -z "$PHY" ]; then
  PHY="phy0"
else
  PHY="${PHY%%&*}"
fi

BINS="${QUERY_STRING#*bins=}"
if [ "$BINS" = "$QUERY_STRING" ] || [ -z "$BINS" ]; then
  BINS=64
else
  BINS="${BINS%%&*}"
fi

ATH_DIR="/sys/kernel/debug/ieee80211/$PHY/ath10k"
CTL="$ATH_DIR/spectral_scan_ctl"
SCAN0="$ATH_DIR/spectral_scan0"
PARSER="/usr/lib/rfeye/rfeye-spectral-parse"

if [ ! -f "$SCAN0" ]; then
  echo "{\"ok\":false,\"error\":\"spectral_scan0 missing\",\"phy\":\"$PHY\"}"
  exit 0
fi

if [ ! -x "$PARSER" ]; then
  echo "{\"ok\":false,\"error\":\"parser missing\",\"path\":\"$PARSER\"}"
  exit 0
fi

IFACE=""
for n in /sys/class/ieee80211/$PHY/device/net/*; do
  [ -e "$n" ] || continue
  IFACE="$(basename "$n")"
  break
done

if [ -f "$CTL" ]; then
  echo background > "$CTL" 2>/dev/null || true
  echo trigger > "$CTL" 2>/dev/null || true
fi

if [ -n "$IFACE" ] && command -v iw >/dev/null 2>&1; then
  iw dev "$IFACE" scan >/dev/null 2>&1 || true
fi

TMP="/tmp/rfeye-snap-$$.bin"
if command -v timeout >/dev/null 2>&1; then
  timeout 2 dd if="$SCAN0" of="$TMP" bs=1 count=4096 2>/dev/null || true
else
  dd if="$SCAN0" of="$TMP" bs=1 count=4096 2>/dev/null || true
fi

FRAME="$($PARSER --input "$TMP" --phy "$PHY" --limit 1 --bins "$BINS" 2>/dev/null | head -n1)"
rm -f "$TMP"

if [ -z "$FRAME" ]; then
  echo "{\"ok\":false,\"error\":\"no frame available\",\"phy\":\"$PHY\"}"
  exit 0
fi

echo "{\"ok\":true,\"frame\":$FRAME}"

