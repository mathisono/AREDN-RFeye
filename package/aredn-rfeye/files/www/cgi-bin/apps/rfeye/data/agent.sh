#!/bin/sh

echo "Content-Type: application/json"
echo "Cache-Control: no-store"
echo

ACTION="${QUERY_STRING#*action=}"
[ "$ACTION" = "$QUERY_STRING" ] && ACTION="status" || ACTION="${ACTION%%&*}"

SECONDS_ARG="${QUERY_STRING#*seconds=}"
[ "$SECONDS_ARG" = "$QUERY_STRING" ] && SECONDS_ARG="" || SECONDS_ARG="${SECONDS_ARG%%&*}"

BINS_ARG="${QUERY_STRING#*bins=}"
[ "$BINS_ARG" = "$QUERY_STRING" ] && BINS_ARG="" || BINS_ARG="${BINS_ARG%%&*}"

PHY_ARG="${QUERY_STRING#*phy=}"
[ "$PHY_ARG" = "$QUERY_STRING" ] && PHY_ARG="" || PHY_ARG="${PHY_ARG%%&*}"

AGENT="/usr/sbin/rfeye-agent"

case "$ACTION" in
  status)
    exec "$AGENT" status
    ;;
  start)
    exec "$AGENT" start "${SECONDS_ARG:-10}" "${BINS_ARG:-128}" "${PHY_ARG:-phy0}"
    ;;
  stop)
    exec "$AGENT" stop
    ;;
  snapshot)
    exec "$AGENT" snapshot
    ;;
  *)
    echo '{"ok":false,"error":"unknown action"}'
    ;;
esac

