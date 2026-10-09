#!/bin/sh
# RFeye stock firmware caldata reconnaissance probe.
# BusyBox/ash compatible and read-only: no flash writes, no radio rebinds,
# no caldata install, and no mesh configuration changes.

set -u
VERSION="2026-10-09-r1"
OUTBASE="${RFEYE_OUTBASE:-/tmp}"
DUMP_BYTES="${RFEYE_DUMP_BYTES:-65536}"
MTD_NAME="${RFEYE_MTD_NAME:-}"
AIRVIEW_WAIT="0"
NO_TAR="0"

usage() {
  cat <<USAGE
Usage: $0 [--outdir DIR] [--mtd-name NAME] [--dump-bytes N] [--airview-wait SEC] [--no-tar]

Examples:
  sh $0
  sh $0 --airview-wait 60
  RFEYE_MTD_NAME=EEPROM sh $0 --airview-wait 90

This probe is read-only. It captures metadata and EEPROM/ART bytes, but it
never writes ART/EEPROM, never installs caldata, and never rebinds a radio.
Do not publish full EEPROM/ART bundles unless you understand what identifiers
are inside.
USAGE
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --outdir) shift; OUTBASE="$1" ;;
    --mtd-name) shift; MTD_NAME="$1" ;;
    --dump-bytes) shift; DUMP_BYTES="$1" ;;
    --airview-wait) shift; AIRVIEW_WAIT="$1" ;;
    --no-tar) NO_TAR="1" ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
  esac
  shift
done

TS="$(date +%Y%m%d-%H%M%S 2>/dev/null || date)"
HOST="$(hostname 2>/dev/null || echo node)"
SAFE_HOST="$(echo "$HOST" | tr -c 'A-Za-z0-9_.-' '_')"
OUTDIR="$OUTBASE/rfeye-stock-caldata-probe-$SAFE_HOST-$TS"
mkdir -p "$OUTDIR" || exit 1
LOG="$OUTDIR/probe.log"

log() { echo "$*" | tee -a "$LOG" >/dev/null; }
run() { name="$1"; shift; { echo "# $*"; "$@"; } > "$OUTDIR/$name" 2>&1 || true; }
copy_if() { [ -e "$1" ] && cat "$1" > "$OUTDIR/$2" 2>/dev/null || true; }

hash_file() {
  if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" 2>/dev/null || true;
  elif command -v md5sum >/dev/null 2>&1; then md5sum "$1" 2>/dev/null || true;
  else wc -c "$1" 2>/dev/null || true; fi
}

classify() {
  f="$1"; label="$2"; [ -f "$f" ] || return
  size="$(wc -c < "$f" 2>/dev/null | tr -d ' ')"
  hdr4="$(od -An -N4 -tx1 "$f" 2>/dev/null | tr -d ' \n')"
  hdr2="$(echo "$hdr4" | cut -c1-4)"
  nonff="$(od -An -v -tx1 "$f" 2>/dev/null | tr ' ' '\n' | awk 'NF && $1 != "ff" {c++} END {print c+0}')"
  non00="$(od -An -v -tx1 "$f" 2>/dev/null | tr ' ' '\n' | awk 'NF && $1 != "00" {c++} END {print c+0}')"
  class="unknown"
  [ "$nonff" = "0" ] && class="blank_ff"
  [ "$non00" = "0" ] && class="blank_00"
  [ "$hdr2" = "0202" ] && class="ar9300_template_0202_candidate"
  [ "$hdr2" = "4408" ] && class="ar9300_compressed_4408_candidate"
  {
    echo "label=$label"
    echo "file=$(basename "$f")"
    echo "size=$size"
    echo "header4=$hdr4"
    echo "classification=$class"
    echo "non_ff_bytes=$nonff"
    echo "non_00_bytes=$non00"
    echo "hash=$(hash_file "$f")"
  } > "$OUTDIR/$label.classification.txt"
  log "CLASSIFY $label $class header=$hdr4 size=$size non_ff=$nonff non_00=$non00"
}

find_mtd_line() {
  if [ -n "$MTD_NAME" ]; then grep -i "\"$MTD_NAME\"" /proc/mtd 2>/dev/null | head -n 1; return; fi
  grep -iE '"(eeprom|art|cal|caldata|radio)' /proc/mtd 2>/dev/null | head -n 1
}

capture_phase() {
  phase="$1"; log "=== capture $phase ==="
  run "date.$phase.txt" date
  run "uname.$phase.txt" uname -a
  run "hostname.$phase.txt" hostname
  run "proc_mtd.$phase.txt" cat /proc/mtd
  run "mounts.$phase.txt" cat /proc/mounts
  run "ifconfig-a.$phase.txt" ifconfig -a
  run "iwconfig.$phase.txt" iwconfig
  run "iw-dev.$phase.txt" iw dev
  run "lsmod.$phase.txt" lsmod
  run "ps.$phase.txt" ps w
  run "dmesg.$phase.txt" dmesg
  run "lib_firmware_tree.$phase.txt" ls -laR /lib/firmware
  run "proc_device_tree.$phase.txt" ls -laR /proc/device-tree
  run "sys_platform_devices.$phase.txt" ls -la /sys/bus/platform/devices
  run "debugfs_ieee80211.$phase.txt" ls -laR /sys/kernel/debug/ieee80211
  run "find_airview_spectral_cal.$phase.txt" find /tmp /var /etc /lib /usr -iname '*airview*' -o -iname '*spectral*' -o -iname '*cal*' -o -iname '*eeprom*'
  copy_if /etc/version "etc_version.$phase.txt"
  copy_if /etc/board.info "etc_board_info.$phase.txt"
  copy_if /etc/board.json "etc_board_json.$phase.txt"
  copy_if /tmp/sysinfo/model "tmp_sysinfo_model.$phase.txt"
  copy_if /tmp/sysinfo/board_name "tmp_sysinfo_board_name.$phase.txt"
}

dump_eeprom() {
  phase="$1"
  line="$(find_mtd_line)"
  if [ -z "$line" ]; then echo "No EEPROM/ART/caldata MTD found; set --mtd-name." > "$OUTDIR/eeprom.$phase.error.txt"; return 1; fi
  mtd="${line%%:*}"; idx="$(echo "$mtd" | sed 's/^mtd//')"
  echo "$line" > "$OUTDIR/eeprom_mtd_line.$phase.txt"; log "MTD $phase $line"
  out="$OUTDIR/eeprom.full.$phase.bin"
  if [ -e "/dev/mtdblock$idx" ]; then dd if="/dev/mtdblock$idx" of="$out" bs="$DUMP_BYTES" count=1 > "$OUTDIR/eeprom.full.$phase.dd.log" 2>&1 || true;
  elif [ -e "/dev/mtd$idx" ]; then dd if="/dev/mtd$idx" of="$out" bs="$DUMP_BYTES" count=1 > "$OUTDIR/eeprom.full.$phase.dd.log" 2>&1 || true;
  else echo "No /dev/mtdblock$idx or /dev/mtd$idx" > "$OUTDIR/eeprom.$phase.error.txt"; return 1; fi
  [ -s "$out" ] || return 1
  hash_file "$out" > "$OUTDIR/eeprom.full.$phase.hash.txt"
  hexdump -C "$out" | head -120 > "$OUTDIR/eeprom.full.$phase.hexdump.head.txt" 2>/dev/null || true
  dd if="$out" of="$OUTDIR/wmac_0x1000_1024.$phase.bin" bs=1 skip=4096 count=1024 > "$OUTDIR/wmac_0x1000_1024.$phase.dd.log" 2>&1 || true
  dd if="$out" of="$OUTDIR/wmac_0x1000_4096.$phase.bin" bs=1 skip=4096 count=4096 > "$OUTDIR/wmac_0x1000_4096.$phase.dd.log" 2>&1 || true
  dd if="$out" of="$OUTDIR/pci_0x5000_4096.$phase.bin" bs=1 skip=20480 count=4096 > "$OUTDIR/pci_0x5000_4096.$phase.dd.log" 2>&1 || true
  classify "$OUTDIR/wmac_0x1000_1024.$phase.bin" "wmac_0x1000_1024.$phase"
  classify "$OUTDIR/wmac_0x1000_4096.$phase.bin" "wmac_0x1000_4096.$phase"
  classify "$OUTDIR/pci_0x5000_4096.$phase.bin" "pci_0x5000_4096.$phase"
}

cat > "$OUTDIR/manifest.txt" <<EOF
rfeye_stock_caldata_probe_version=$VERSION
host=$HOST
timestamp=$TS
airview_wait_seconds=$AIRVIEW_WAIT
dump_bytes=$DUMP_BYTES
forced_mtd_name=$MTD_NAME
safety=no_flash_writes_no_caldata_install_no_radio_rebind
sharing_warning=full_eeprom_art_dumps_may_contain_identifiers
EOF

log "RFeye stock caldata probe $VERSION output=$OUTDIR"
capture_phase before
dump_eeprom before || true

if [ "$AIRVIEW_WAIT" != "0" ]; then
  log "Waiting $AIRVIEW_WAIT seconds; start AirView on the stock UI now if applicable."
  sleep "$AIRVIEW_WAIT"
  capture_phase after_airview
  dump_eeprom after_airview || true
  if [ -f "$OUTDIR/eeprom.full.before.bin" ] && [ -f "$OUTDIR/eeprom.full.after_airview.bin" ]; then
    cmp -l "$OUTDIR/eeprom.full.before.bin" "$OUTDIR/eeprom.full.after_airview.bin" > "$OUTDIR/eeprom.before_vs_after_airview.cmp.txt" 2>&1 || true
    [ -s "$OUTDIR/eeprom.before_vs_after_airview.cmp.txt" ] || echo no_byte_differences > "$OUTDIR/eeprom.before_vs_after_airview.cmp.txt"
  fi
fi

if [ "$NO_TAR" != "1" ]; then
  parent="$(dirname "$OUTDIR")"; base="$(basename "$OUTDIR")"; tarball="$OUTDIR.tar.gz"
  (cd "$parent" && tar -czf "$tarball" "$base") > "$OUTDIR/tar.log" 2>&1 || true
  [ -f "$tarball" ] && log "Bundle $tarball"
fi

log "Done. Review classification files before using any caldata candidate."
echo "$OUTDIR"
