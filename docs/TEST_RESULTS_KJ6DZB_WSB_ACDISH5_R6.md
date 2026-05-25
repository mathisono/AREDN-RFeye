# RFeye r6 Test Results — KJ6DZB-WSB-ACdish5

- Date: 2026-05-24 PDT
- Node: `10.188.138.222`
- Jump host: `192.168.3.88` (MSE-88)
- Package: `aredn-rfeye 0.1.0-r6`

## Summary

- `raw_capture_test`: `bytes_read=262144`, `frames_emitted=0`
- Raw file is mostly nonzero: `222160` nonzero bytes vs `39984` zero bytes
- `raw_inspect`: valid JSON
- `parser_probe`: valid JSON
- `--probe`: valid JSON and explains why sequential parsing is not trustworthy yet

## Raw capture

First 64 hex bytes:

`ff93003c0057017d000000008ce6f62a000509390103010302020001010304030400050502020205070300010003050304050204040207060106000300060204`

Hexdump excerpt:

```text
00000000  ff 93 00 3c 00 57 01 7d  00 00 00 00 8c e6 f6 2a
00000010  00 05 09 39 01 03 01 03  02 02 00 01 01 03 04 03
00000020  04 00 05 05 02 02 02 05  07 03 00 01 00 03 05 03
00000030  04 05 02 04 04 02 07 06  01 06 00 03 00 06 02 04
```

## Parser probe

- `candidate_records`: `247957`
- `type3_candidate_records`: `38040`
- `current_layout_matches`: `35076`
- `compact_layout_matches`: `0`
- `type3_offsets`: present
- Other TLV types also appear (`type_counts` includes many values, not just 3)

## Interpretation

- The capture buffer is not empty and is not mostly zero.
- The parser can now probe and resync, but the file still looks like a layout/framing mismatch.
- The current parser is finding many candidate headers at arbitrary offsets, so the real issue is likely TLV boundary detection / sample layout alignment rather than a dead capture path.

## Next recommended parser fix

Tighten record boundary detection around the real ath10k stream framing, then restrict decode to true type-3 records only after the header format is verified. The probe output suggests the current 3-byte TLV assumption is still matching payload bytes as headers.
