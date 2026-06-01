#!/bin/sh
# rfeye-wmac-provision.sh — WMAC caldata provisioning for AREDN PR #2730
#
# PR #2730 enables the QCA9558 WMAC with qca,no-eeprom in the DTS.
# The WMAC will NOT initialize until a caldata firmware file is provided.
#
# r17 safety model:
#   - Never writes to ART/EEPROM flash
#   - Installs only an ath9k firmware caldata file under /lib/firmware
#   - Prefers the compiled WMAC-only helper over shell sysfs writes
#   - Falls back to WMAC-only sysfs platform unbind/bind if needed
#   - Does not disturb the production ath10k PCI radio
#   - Treats substitute/reference caldata as bench-only until validated

set -u

# --- Configuration ---

WMAC_PLATFORM_ID="18100000.wmac"
ATH9K_PLATFORM_DRIVER="/sys/bus/platform/drivers/ath9k"
WMAC_PLATFORM_DEVICE="/sys/bus/platform/devices/$WMAC_PLATFORM_ID"
RFEYE_WMAC_REBIND="/usr/lib/rfeye/rfeye-wmac-rebind"
PRIMARY_CALDATA_PATH="/lib/firmware/ath9k-eeprom-ahb-18100000.wmac.bin"

CALDATA_PATHS="
$PRIMARY_CALDATA_PATH
/lib/firmware/ath9k-eeprom-pci-0000:00:00.0.bin
"

REFERENCE_CALDATA="/usr/lib/rfeye/caldata/ath9k-caldata-wmac-wa-reference.bin"
SUPPORTED_SYSIDS="0xe3d5 0xe1f5"

# --- Helpers ---

json_escape() { printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g'; }
log() { logger -t rfeye-wmac "$@" 2>/dev/null; echo "$@" >&2; }

get_board_sysid() {
    if [ -f /etc/board.json ]; then
        local model
        model="$(cat /etc/board.json 2>/dev/null | sed -n 's/.*"id":[[:space:]]*"\([^"]*\)".*/\1/p' | head -1)"
        case "$model" in
            *powerbeam-5ac-500*) echo "0xe3d5"; return 0 ;;
            *rocket-5ac-lite*)   echo "0xe1f5"; return 0 ;;
        esac
    fi

    if [ -f /proc/mtd ]; then
        local art_num art_dev sysid_hex
        art_num="$(grep -i '"art"' /proc/mtd 2>/dev/null | head -1 | sed 's/mtd\([0-9]*\):.*/\1/')"
        [ -n "${art_num:-}" ] && art_dev="/dev/mtdblock${art_num}"
        if [ -n "${art_dev:-}" ] && [ -e "$art_dev" ]; then
            sysid_hex="$(dd if="$art_dev" bs=1 skip=12 count=2 2>/dev/null | hexdump -v -e '1/1 "%02x"' 2>/dev/null)"
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

ath9k_loaded() { lsmod 2>/dev/null | grep -q '^ath9k '; }
helper_available() { [ -x "$RFEYE_WMAC_REBIND" ]; }
platform_device_present() { [ -e "$WMAC_PLATFORM_DEVICE" ]; }
platform_bind_available() { [ -w "$ATH9K_PLATFORM_DRIVER/bind" ]; }
platform_unbind_available() { [ -w "$ATH9K_PLATFORM_DRIVER/unbind" ]; }
platform_bound() { [ -e "$ATH9K_PLATFORM_DRIVER/$WMAC_PLATFORM_ID" ] || [ -L "$ATH9K_PLATFORM_DRIVER/$WMAC_PLATFORM_ID" ]; }

wait_for_wmac() {
    local tries=0 max="${1:-10}"
    while [ "$tries" -lt "$max" ]; do
        wmac_phy_exists && return 0
        tries=$((tries + 1))
        sleep 1
    done
    return 1
}

create_monitor_iface() {
    local phy="$1"
    [ -n "$phy" ] || return 0
    if ! iw dev 2>/dev/null | grep -q 'mon1\|rfeye-mon'; then
        iw phy "$phy" interface add rfeye-mon type monitor 2>/dev/null || true
        ip link set rfeye-mon up 2>/dev/null || true
        log "created monitor interface rfeye-mon on $phy"
    fi
}

helper_install_caldata() {
    helper_available || return 1
    "$RFEYE_WMAC_REBIND" --install --quiet \
        --source "$REFERENCE_CALDATA" \
        --dest "$PRIMARY_CALDATA_PATH"
}

helper_rebind_wmac() {
    helper_available || return 1
    "$RFEYE_WMAC_REBIND" --rebind --quiet \
        --source "$REFERENCE_CALDATA" \
        --dest "$PRIMARY_CALDATA_PATH"
}

helper_unbind_wmac() {
    helper_available || return 1
    "$RFEYE_WMAC_REBIND" --unbind --quiet
}

fallback_install_caldata() {
    local caldata_src="$REFERENCE_CALDATA" caldata_dst installed_path=""

    [ -f "$caldata_src" ] || {
        echo "reference caldata not found: $caldata_src" >&2
        return 1
    }

    for caldata_dst in $CALDATA_PATHS; do
        local dir
        dir="$(dirname "$caldata_dst")"
        mkdir -p "$dir" 2>/dev/null || continue
        cp "$caldata_src" "$caldata_dst" 2>/dev/null || continue
        log "installed caldata: $caldata_dst (from $caldata_src)"
        installed_path="$caldata_dst"
        break
    done

    [ -n "$installed_path" ] || {
        echo "failed to install caldata to any firmware path" >&2
        return 1
    }
    echo "$installed_path"
}

fallback_rebind_wmac_platform() {
    platform_device_present || {
        echo "WMAC platform device not present: $WMAC_PLATFORM_DEVICE" >&2
        return 1
    }

    if ! ath9k_loaded; then
        modprobe ath9k 2>/dev/null || {
            echo "modprobe ath9k failed" >&2
            return 1
        }
        sleep 1
    fi

    platform_bind_available || {
        echo "ath9k platform bind control unavailable" >&2
        return 1
    }

    if platform_bound; then
        platform_unbind_available || {
            echo "ath9k platform unbind control unavailable" >&2
            return 1
        }
        log "unbinding ath9k WMAC platform device $WMAC_PLATFORM_ID"
        echo "$WMAC_PLATFORM_ID" > "$ATH9K_PLATFORM_DRIVER/unbind" 2>/dev/null || {
            echo "failed to unbind $WMAC_PLATFORM_ID" >&2
            return 1
        }
        sleep 1
    fi

    log "binding ath9k WMAC platform device $WMAC_PLATFORM_ID"
    echo "$WMAC_PLATFORM_ID" > "$ATH9K_PLATFORM_DRIVER/bind" 2>/dev/null || {
        echo "failed to bind $WMAC_PLATFORM_ID" >&2
        return 1
    }

    wait_for_wmac 10
}

legacy_module_reload() {
    [ "${RFEYE_ALLOW_RMMOD:-0}" = "1" ] || return 1
    log "RFEYE_ALLOW_RMMOD=1 set; attempting legacy ath9k module reload"
    if ath9k_loaded; then
        rmmod ath9k 2>/dev/null || true
        sleep 1
    fi
    modprobe ath9k 2>/dev/null || return 1
    wait_for_wmac 10
}

# --- Commands ---

status_cmd() {
    local sysid model supported="false" wmac="false" spectral="false"
    local caldata="false" ath9k="false" phy="" pdev="false" pbind="false" pbound="false" helper="false"

    sysid="$(get_board_sysid)"
    model="$(get_board_model)"
    is_supported_board "$sysid" && supported="true"
    wmac_phy_exists && wmac="true"
    wmac_spectral_ready && spectral="true"
    caldata_installed && caldata="true"
    ath9k_loaded && ath9k="true"
    platform_device_present && pdev="true"
    platform_bind_available && pbind="true"
    platform_bound && pbound="true"
    helper_available && helper="true"
    phy="$(wmac_phy_name)"

    cat <<JSON
{"ok":true,"board":{"sysid":"$(json_escape "$sysid")","model":"$(json_escape "$model")","supported":$supported},"wmac":{"platform_id":"$WMAC_PLATFORM_ID","platform_device_present":$pdev,"platform_bound":$pbound,"phy":"$(json_escape "$phy")","initialized":$wmac,"spectral_ready":$spectral},"caldata":{"installed":$caldata,"reference":"$REFERENCE_CALDATA","primary_path":"$PRIMARY_CALDATA_PATH"},"ath9k":{"loaded":$ath9k,"platform_bind_available":$pbind},"helper":{"available":$helper,"path":"$RFEYE_WMAC_REBIND"}}
JSON
}

install_cmd() {
    local sysid model helper_result fallback_result

    sysid="$(get_board_sysid)"
    model="$(get_board_model)"

    if ! is_supported_board "$sysid"; then
        echo "{\"ok\":false,\"error\":\"unsupported board: $(json_escape "$model") ($sysid)\"}"
        return 1
    fi

    if wmac_spectral_ready; then
        local phy
        phy="$(wmac_phy_name)"
        echo "{\"ok\":true,\"action\":\"already_ready\",\"phy\":\"$(json_escape "$phy")\",\"message\":\"WMAC already initialized and spectral-ready\"}"
        return 0
    fi

    if helper_available; then
        if helper_result="$(helper_install_caldata 2>&1)"; then
            log "installed caldata with C helper: $PRIMARY_CALDATA_PATH"
            echo "{\"ok\":true,\"action\":\"installed\",\"method\":\"c_helper\",\"caldata_path\":\"$(json_escape "$PRIMARY_CALDATA_PATH")\",\"source\":\"$(json_escape "$REFERENCE_CALDATA")\",\"message\":\"caldata installed with rfeye-wmac-rebind; run reload to initialize WMAC\"}"
            return 0
        fi
        echo "{\"ok\":false,\"error\":\"C helper caldata install failed\",\"detail\":\"$(json_escape "$helper_result")\"}"
        return 1
    fi

    if fallback_result="$(fallback_install_caldata 2>&1)"; then
        echo "{\"ok\":true,\"action\":\"installed\",\"method\":\"shell_fallback\",\"caldata_path\":\"$(json_escape "$fallback_result")\",\"source\":\"$(json_escape "$REFERENCE_CALDATA")\",\"message\":\"caldata installed; run reload to initialize WMAC\"}"
        return 0
    fi

    echo "{\"ok\":false,\"error\":\"failed to install caldata\",\"detail\":\"$(json_escape "$fallback_result")\"}"
    return 1
}

reload_cmd() {
    local sysid model phy rebind_err="" method="shell_fallback"

    sysid="$(get_board_sysid)"
    model="$(get_board_model)"

    if ! is_supported_board "$sysid"; then
        echo "{\"ok\":false,\"error\":\"unsupported board: $(json_escape "$model") ($sysid)\"}"
        return 1
    fi

    if ! caldata_installed; then
        echo "{\"ok\":false,\"error\":\"no caldata installed, run install first\"}"
        return 1
    fi

    if helper_available; then
        method="c_helper"
        if rebind_err="$(helper_rebind_wmac 2>&1)"; then
            phy="$(wmac_phy_name)"
            create_monitor_iface "$phy"
            echo "{\"ok\":true,\"action\":\"platform_rebound\",\"method\":\"$method\",\"phy\":\"$(json_escape "$phy")\",\"spectral_ready\":$(wmac_spectral_ready && echo true || echo false),\"message\":\"WMAC rebound with rfeye-wmac-rebind\"}"
            return 0
        fi
        log "C helper rebind failed: $rebind_err"
    fi

    method="shell_fallback"
    if rebind_err="$(fallback_rebind_wmac_platform 2>&1)"; then
        phy="$(wmac_phy_name)"
        create_monitor_iface "$phy"
        echo "{\"ok\":true,\"action\":\"platform_rebound\",\"method\":\"$method\",\"phy\":\"$(json_escape "$phy")\",\"spectral_ready\":$(wmac_spectral_ready && echo true || echo false),\"message\":\"WMAC rebound with shell sysfs fallback\"}"
        return 0
    fi

    log "platform rebind failed: $rebind_err"

    if legacy_module_reload; then
        phy="$(wmac_phy_name)"
        create_monitor_iface "$phy"
        echo "{\"ok\":true,\"action\":\"legacy_module_reload\",\"phy\":\"$(json_escape "$phy")\",\"spectral_ready\":$(wmac_spectral_ready && echo true || echo false),\"message\":\"WMAC initialized by legacy module reload\"}"
        return 0
    fi

    echo "{\"ok\":false,\"error\":\"WMAC sysfs rebind failed; not running rmmod unless RFEYE_ALLOW_RMMOD=1\",\"detail\":\"$(json_escape "$rebind_err")\"}"
    return 1
}

provision_cmd() {
    local result action

    result="$(install_cmd)"
    action="$(printf '%s' "$result" | sed -n 's/.*"action":"\([^"]*\)".*/\1/p')"

    case "$action" in
        already_ready)
            echo "$result"
            return 0
            ;;
        installed)
            ;;
        *)
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

    if helper_available; then
        helper_unbind_wmac >/dev/null 2>&1 || true
    elif platform_bound && platform_unbind_available; then
        echo "$WMAC_PLATFORM_ID" > "$ATH9K_PLATFORM_DRIVER/unbind" 2>/dev/null || true
        log "unbound ath9k WMAC platform device $WMAC_PLATFORM_ID"
    fi

    echo "{\"ok\":true,\"action\":\"removed\",\"files_removed\":$removed,\"message\":\"caldata removed; ath9k module left loaded\"}"
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
