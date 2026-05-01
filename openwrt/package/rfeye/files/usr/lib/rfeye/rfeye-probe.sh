#!/bin/sh
# rfeye-probe.sh - Milestone 1 support probe for AREDN RFeye

set -u

CAPTURE_BYTES=0
CAPTURE_DIR="/tmp"

while [ $# -gt 0 ]; do
  case "$1" in
    --capture-bytes)
      CAPTURE_BYTES="$2"
      shift 2
      ;;
    --capture-dir)
      CAPTURE_DIR="$2"
      shift 2
      ;;
    *)
      echo "Usage: $0 [--capture-bytes N] [--capture-dir DIR]" >&2
      exit 2
      ;;
  esac
done

json_escape() {
  echo "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'
}

is_mounted_debugfs=0
if grep -q " /sys/kernel/debug " /proc/mounts 2>/dev/null; then
  is_mounted_debugfs=1
fi

echo "{"
echo "  \"probe\": \"rfeye-probe\"," 
echo "  \"debugfs_mounted\": $is_mounted_debugfs,"
echo "  \"phys\": ["

first_phy=1
for p in /sys/kernel/debug/ieee80211/phy*; do
  [ -d "$p" ] || continue

  phy="$(basename "$p")"
  ath="$p/ath10k"

  [ $first_phy -eq 1 ] || echo "    ,"
  first_phy=0

  ctl=0; scan0=0; bins=0; count=0
  [ -f "$ath/spectral_scan_ctl" ] && ctl=1
  [ -f "$ath/spectral_scan0" ] && scan0=1
  [ -f "$ath/spectral_bins" ] && bins=1
  [ -f "$ath/spectral_count" ] && count=1

  iface=""
  net_glob="/sys/class/ieee80211/$phy/device/net/*"
  for n in $net_glob; do
    [ -e "$n" ] || continue
    iface="$(basename "$n")"
    break
  done

  has_active=0; has_busy=0; has_tx=0; has_rx=0; has_noise=0
  if [ -n "$iface" ] && command -v iw >/dev/null 2>&1; then
    SURVEY="$(iw dev "$iface" survey dump 2>/dev/null || true)"
    echo "$SURVEY" | grep -qi "active time" && has_active=1
    echo "$SURVEY" | grep -qi "busy time" && has_busy=1
    echo "$SURVEY" | grep -qi "transmit time" && has_tx=1
    echo "$SURVEY" | grep -qi "receive time" && has_rx=1
    echo "$SURVEY" | grep -qi "noise" && has_noise=1
  fi

  capture_file=""
  capture_bytes_written=0
  if [ "$CAPTURE_BYTES" -gt 0 ] 2>/dev/null && [ $scan0 -eq 1 ]; then
    mkdir -p "$CAPTURE_DIR" 2>/dev/null || true
    capture_file="$CAPTURE_DIR/rfeye-sample-$phy.bin"
    dd if="$ath/spectral_scan0" of="$capture_file" bs=1 count="$CAPTURE_BYTES" 2>/dev/null || true
    if [ -f "$capture_file" ]; then
      capture_bytes_written="$(wc -c < "$capture_file" 2>/dev/null || echo 0)"
    fi
  fi

  echo "    {"
  echo "      \"phy\": \"$(json_escape "$phy")\"," 
  echo "      \"ath10k_dir\": \"$(json_escape "$ath")\"," 
  echo "      \"spectral\": {"
  echo "        \"spectral_scan_ctl\": $ctl,"
  echo "        \"spectral_scan0\": $scan0,"
  echo "        \"spectral_bins\": $bins,"
  echo "        \"spectral_count\": $count"
  echo "      },"
  echo "      \"iface\": \"$(json_escape "$iface")\"," 
  echo "      \"survey_fields\": {"
  echo "        \"active\": $has_active,"
  echo "        \"busy\": $has_busy,"
  echo "        \"tx\": $has_tx,"
  echo "        \"rx\": $has_rx,"
  echo "        \"noise\": $has_noise"
  echo "      },"
  echo "      \"capture\": {"
  echo "        \"requested_bytes\": $CAPTURE_BYTES,"
  echo "        \"file\": \"$(json_escape "$capture_file")\"," 
  echo "        \"written_bytes\": $capture_bytes_written"
  echo "      }"
  echo -n "    }"
done

echo
echo "  ]"
echo "}"
