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
SURVEY="/usr/sbin/rfeye-survey"

case "$ACTION" in
  status)
    exec "$AGENT" status
    ;;
  capture_status)
    exec "$AGENT" capture_status
    ;;
  radio_info)
    exec "$AGENT" radio_info
    ;;
  ui_state)
    exec "$AGENT" ui_state
    ;;
  waveform)
    exec "$AGENT" waveform
    ;;
  waterfall)
    exec "$AGENT" waterfall
    ;;
  ambient)
    exec "$AGENT" ambient
    ;;
  heatmap_bundle)
    exec "$AGENT" heatmap_bundle
    ;;
  pipeline_status)
    exec "$AGENT" pipeline_status
    ;;
  pipeline_test)
    exec "$AGENT" pipeline_test "${BINS_ARG:-128}" "${PHY_ARG:-phy0}"
    ;;
  acquisition_debug)
    exec "$AGENT" acquisition_debug
    ;;
  raw_inspect)
    exec "$AGENT" raw_inspect
    ;;
  parser_probe)
    exec "$AGENT" parser_probe
    ;;
  raw_capture_test)
    exec "$AGENT" raw_capture_test "${SECONDS_ARG:-10}" "${BINS_ARG:-128}" "${PHY_ARG:-phy0}"
    ;;
  reset)
    exec "$AGENT" reset "${PHY_ARG:-phy0}"
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
  survey)
    exec "$SURVEY" survey "${PHY_ARG:-}"
    ;;
  survey_raw|raw)
    exec "$SURVEY" raw "${PHY_ARG:-}"
    ;;
  utilization|survey_delta)
    exec "$SURVEY" utilization "${PHY_ARG:-}"
    ;;
  *)
    echo '{"ok":false,"error":"unknown action"}'
    ;;
esac

