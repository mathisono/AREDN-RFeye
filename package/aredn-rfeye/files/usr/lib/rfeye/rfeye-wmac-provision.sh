#!/bin/sh
# rfeye-wmac-provision.sh — WMAC caldata provisioning for AREDN PR #2730
#
# PR #2730 enables the QCA9558 WMAC with qca,no-eeprom in the DTS.
# The WMAC will NOT initialize until a caldata firmware file is provided.
# This script installs the caldata and reloads ath9k to bring up the WMAC.
#
# Safety rules:
#   - Never writes to flash (ART/EEPROM partition)
#   - Caldata file goes to /lib/firmware (overlay or tmpfs)
#   - If ath9k reload fails, ath10k production radio is unaffected
#   - Idempotent: safe to run multiple times

set -u

# --- Configuration ---

# The firmware path ath9k expects when qca,no-eeprom is set.
# On ath79 QCA9558, the WMAC AHB address is 18100000.
# OpenWrt ath9k tries: /lib/firmware/ath9k-eeprom-ahb-18100000.wmac.bin
# Also tries: /lib/firmware/ath9k/caldata.bin and platform-specific paths.
# We try multiple paths; the first one that works wins.
CALDATA_PATHS="
/lib/firmware/ath9k-eeprom-ahb-18100000.wmac.bin
/lib/firmware/ath9k-eeprom-pci-0000:00:00.0.bin
"

# Shipped reference caldata (WA board WMAC caldata, AR9300 template format)
REFERENCE_CALDATA="/usr/lib/rfeye/caldata/ath9k-caldata-wmac-wa-reference.bin"

# Supported board sysids
# 0xe3d5 = PowerBeam 5AC 500 (XC)
# 0xe1f5 = Rocket 5AC Lite (XC)
SUPPORTED_SYSIDS="0xe3d5 0xe1f5"

# --- Helpers ---

json_escape() { printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'; }

log() { logger -t rfeye-wmac "$@" 2>/dev/null; echo "$@" >&2; }

get_board_sysid() {
    # Read sysid from board.json or DTS
    if [ -f /etc/board.json ]; then
        local model
        model="$(cat /etc/board.json 2>/dev/null | \
            sed -n 's/.*"id":[[:space:]]*"\([^"]*\)".*/\1/p' | head -1)"
        case "$model" in
            *powerbeam-5ac-500*) echo "0xe3d5"; return 0 ;;
            *rocket-5ac-lite*)   echo "0xe1f5"; return 0 ;;
        esac
    fi
    # Fallback: read from EEPROM header
    if [ -f /proc/mtd ]; then
        local art_num art_dev sysid_hex
        art_num="$(grep -i '"art"' /proc/mtd 2>/dev/null | head -1 | \
            sed 's/mtd\([0-9]*\):.*/\1/')"
        [ -n "$art_num" ] && art_dev="/dev/mtdblock${art_num}"
        if [ -n "$art_dev" ] && [ -e "$art_dev" ]; then
            sysid_hex="$(dd if="$art_dev" bs=1 skip=12 count=2 2>/dev/null | \
                hexdump -v -e '1/1 "%02x"' 2>/dev/null)"
            [ -n "$sysid_hex" ] && echo "0x$sysid_hex" && return 0
        fi
    fi
    echo "unknown"
}

get_board_model() {
    if [ -f /tmp/sysinfo/model ]; then
        cat /tmp/sysinfo/model 2>/dev/null
        return 0
    fi
    if [ -f /etc/board.json ]; then
        sed -n 's/.*"name":[[:space:]]*"\([^"]*\)".*/\1/p' /etc/board.json 2>/dev/null | head -1
        return 0
    fi
    echo "unknown"
}

is_supported_board() {
    local sysid="$1" s
    for s in $SUPPORTED_SYSIDS; do
        [ "$sysid" = "$s" ] && return 0
    done
    return 1
}

wmac_phy_exists() {
    # Check if a WMAC phy (ath9k on AHB) exists
    for p in /sys/kernel/debug/ieee80211/phy*; do
        [ -d "$p/ath9k" ] && return 0
    done
    return 1
}

wmac_phy_name() {
    for p in /sys/kernel/debug/ieee80211/phy*; do
        [ -d "$p/ath9k" ] && { basename "$p"; return 0; }
    done
    echo ""
}

wmac_spectral_ready() {
    local phy
    phy="$(wmac_phy_name)"
    [ -n "$phy" ] || return 1
    [ -f "/sys/kernel/debug/ieee80211/$phy/ath9k/spectral_scan_ctl" ] && return 0
    return 1
}

caldata_installed() {
    local p
    for p in $CALDATA_PATHS; do
        [ -f "$p" ] && return 0
    done
    return 1
}

ath9k_loaded() {
    lsmod 2>/dev/null | grep -q "^ath9k " && return 0
    return 1
}

# --- Commands ---

status_cmd() {
    local sysid model supported="false" wmac="false" spectral="false"
    local caldata="false" ath9k="false" phy=""

    sysid="$(get_board_sysid)"
    model="$(get_board_model)"
    is_supported_board "$sysid" && supported="true"
    wmac_phy_exists && wmac="true"
    wmac_spectral_ready && spectral="true"
    caldata_installed && caldata="true"
    ath9k_loaded && ath9k="true"
    phy="$(wmac_phy_name)"

    cat <<JSON
{"ok":true,"board":{"sysid":"$(json_escape "$sysid")","model":"$(json_escape "$model")","supported":$supported},"wmac":{"phy":"$(json_escape "$phy")","initialized":$wmac,"spectral_ready":$spectral},"caldata":{"installed":$caldata,"reference":"$REFERENCE_CALDATA"},"ath9k_loaded":$ath9k}
JSON
}

install_cmd() {
    local sysid model caldata_src caldata_dst installed_path=""

    sysid="$(get_board_sysid)"
    model="$(get_board_model)"

    if ! is_supported_board "$sysid"; then
        echo "{\"ok\":false,\"error\":\"unsupported board: $model ($sysid)\"}"
        return 1
    fi

    if wmac_spectral_ready; then
        local phy
        phy="$(wmac_phy_name)"
        echo "{\"ok\":true,\"action\":\"already_ready\",\"phy\":\"$(json_escape "$phy")\",\"message\":\"WMAC already initialized and spectral-ready\"}"
        return 0
    fi

    # Check reference caldata exists
    caldata_src="$REFERENCE_CALDATA"
    if [ ! -f "$caldata_src" ]; then
        echo "{\"ok\":false,\"error\":\"reference caldata not found: $caldata_src\"}"
        return 1
    fi

    # Install caldata to firmware path(s)
    for caldata_dst in $CALDATA_PATHS; do
        local dir
        dir="$(dirname "$caldata_dst")"
        mkdir -p "$dir" 2>/dev/null || continue
        cp "$caldata_src" "$caldata_dst" 2>/dev/null || continue
        log "installed caldata: $caldata_dst (from $caldata_src)"
        installed_path="$caldata_dst"
        break
    done

    if [ -z "$installed_path" ]; then
        echo "{\"ok\":false,\"error\":\"failed to install caldata to any firmware path\"}"
        return 1
    fi

    echo "{\"ok\":true,\"action\":\"installed\",\"caldata_path\":\"$(json_escape "$installed_path")\",\"source\":\"$(json_escape "$caldata_src")\",\"message\":\"caldata installed, run 'reload' to initialize WMAC\"}"
}

reload_cmd() {
    local sysid model

    sysid="$(get_board_sysid)"
    model="$(get_board_model)"

    if ! is_supported_board "$sysid"; then
        echo "{\"ok\":false,\"error\":\"unsupported board: $model ($sysid)\"}"
        return 1
    fi

    if ! caldata_installed; then
        echo "{\"ok\":false,\"error\":\"no caldata installed, run 'install' first\"}"
        return 1
    fi

    # Reload ath9k module
    log "reloading ath9k module..."

    if ath9k_loaded; then
        rmmod ath9k 2>/dev/null || true
        sleep 1
    fi

    modprobe ath9k 2>/dev/null || {
        echo "{\"ok\":false,\"error\":\"modprobe ath9k failed\"}"
        return 1
    }

    # Wait for WMAC to initialize
    local tries=0
    while [ "$tries" -lt 10 ]; do
        if wmac_phy_exists; then
            break
        fi
        tries=$((tries + 1))
        sleep 1
    done

    if ! wmac_phy_exists; then
        echo "{\"ok\":false,\"error\":\"ath9k loaded but WMAC phy did not appear after ${tries}s\"}"
        return 1
    fi

    local phy
    phy="$(wmac_phy_name)"

    # Create monitor interface for spectral scanning
    if ! iw dev 2>/dev/null | grep -q "mon1\|rfeye-mon"; then
        iw phy "$phy" interface add rfeye-mon type monitor 2>/dev/null || true
        ip link set rfeye-mon up 2>/dev/null || true
        log "created monitor interface rfeye-mon on $phy"
    fi

    # Verify spectral scan is available
    if wmac_spectral_ready; then
        log "WMAC spectral scanning ready on $phy"
        echo "{\"ok\":true,\"action\":\"reloaded\",\"phy\":\"$(json_escape "$phy")\",\"spectral_ready\":true,\"message\":\"WMAC initialized, spectral scanning available\"}"
        return 0
    fi

    echo "{\"ok\":true,\"action\":\"reloaded\",\"phy\":\"$(json_escape "$phy")\",\"spectral_ready\":false,\"message\":\"WMAC initialized but spectral_scan_ctl not found — may need reboot\"}"
}

provision_cmd() {
    # Combined install + reload in one step
    local result

    result="$(install_cmd)"
    local action
    action="$(printf '%s' "$result" | sed -n 's/.*"action":"\([^"]*\)".*/\1/p')"

    case "$action" in
        already_ready)
            echo "$result"
            return 0
            ;;
        installed)
            # Proceed to reload
            ;;
        *)
            # install failed
            echo "$result"
            return 1
            ;;
    esac

    reload_cmd
}

remove_cmd() {
    local removed=0 p
    for p in $CALDATA_PATHS; do
        if [ -f "$p" ]; then
            rm -f "$p"
            log "removed caldata: $p"
            removed=$((removed + 1))
        fi
    done

    # Unload ath9k if loaded (WMAC only — ath10k stays)
    if ath9k_loaded; then
        rmmod ath9k 2>/dev/null || true
        log "unloaded ath9k"
    fi

    echo "{\"ok\":true,\"action\":\"removed\",\"files_removed\":$removed}"
}

# --- Main ---

cmd="${1:-status}"
shift 2>/dev/null || true

case "$cmd" in
    status)    status_cmd ;;
    install)   install_cmd ;;
    reload)    reload_cmd ;;
    provision) provision_cmd ;;
    remove)    remove_cmd ;;
    *)
        echo "{\"ok\":false,\"error\":\"usage: rfeye-wmac-provision {status|install|reload|provision|remove}\"}"
        exit 2
        ;;
esac
