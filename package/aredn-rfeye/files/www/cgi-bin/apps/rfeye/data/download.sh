#!/bin/sh
# Safe RFeye downloads from /tmp/rfeye only.

TYPE="${QUERY_STRING#*type=}"
[ "$TYPE" = "$QUERY_STRING" ] && TYPE="tlv" || TYPE="${TYPE%%&*}"
AGENT="/usr/sbin/rfeye-agent"

case "$TYPE" in
  tlv)
    FILE="/tmp/rfeye/latest.tlv"
    [ -s "$FILE" ] || FILE="/tmp/rfeye/raw-test.tlv"
    if [ ! -s "$FILE" ]; then
      echo "Content-Type: application/json"
      echo "Cache-Control: no-store"
      echo
      echo '{"ok":false,"error":"no TLV capture available"}'
      exit 1
    fi
    echo "Content-Type: application/octet-stream"
    echo "Content-Disposition: attachment; filename=rfeye-latest.tlv"
    echo "Cache-Control: no-store"
    echo
    exec cat "$FILE"
    ;;
  jsonl)
    echo "Content-Type: application/x-ndjson"
    echo "Content-Disposition: attachment; filename=rfeye-frames.jsonl"
    echo "Cache-Control: no-store"
    echo
    exec "$AGENT" export_jsonl
    ;;
  *)
    echo "Content-Type: application/json"
    echo "Cache-Control: no-store"
    echo
    echo '{"ok":false,"error":"unknown download type"}'
    exit 1
    ;;
esac
