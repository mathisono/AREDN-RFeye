#!/usr/bin/env python3
import json
import sys
from collections import Counter, defaultdict
from pathlib import Path


def hex_bytes(b):
    return b.hex()


def hist_first(data, n=4096):
    h = Counter(data[:n])
    return {f"{i:02x}": h.get(i, 0) for i in range(256)}


def plausible_bins(data):
    if not data:
        return False, 0.0, 0, 0
    zeros = sum(1 for x in data if x == 0)
    nonzero = len(data) - zeros
    uniq = len(set(data))
    avg = sum(data) / len(data)
    var = sum((x - avg) ** 2 for x in data) / len(data)
    return nonzero > 0 and var > 1.0 and uniq > 3, var, zeros, uniq


def scan_tlv(data, header_size, endian, variant_name):
    n = len(data)
    records = []
    type_counts = Counter()
    type3_offsets = []
    plausible = []

    for off in range(0, n - header_size + 1):
        t = data[off]
        if header_size == 3:
            ln = int.from_bytes(data[off + 1:off + 3], endian)
        else:
            ln = int.from_bytes(data[off + 2:off + 4], endian)
        if ln <= 0 or off + header_size + ln > n:
            continue
        records.append((off, t, ln))
        type_counts[t] += 1
        if t == 3:
            type3_offsets.append(off)
            payload = data[off + header_size:off + header_size + ln]
            ok, var, zeros, uniq = plausible_bins(payload)
            if ok:
                plausible.append({"offset": off, "length": ln, "payload_first_bytes_hex": payload[:16].hex(), "variance": round(var, 3), "unique": uniq, "zeros": zeros})

    first = []
    for off, t, ln in records[:10]:
        payload = data[off + header_size:off + header_size + ln]
        first.append({"offset": off, "type": t, "length": ln, "payload_first_bytes_hex": payload[:16].hex()})

    chain = False
    stride = None
    if len(records) >= 2:
        deltas = [records[i + 1][0] - records[i][0] for i in range(len(records) - 1)]
        c = Counter(deltas)
        stride, cnt = c.most_common(1)[0]
        chain = cnt >= max(2, len(deltas) * 3 // 4)

    best = {
        "variant": variant_name,
        "candidate_count": len(records),
        "plausible_record_count": len(plausible),
        "type_counts": {str(k): v for k, v in type_counts.items()},
        "first_candidate_records": first,
        "type3_offsets": type3_offsets[:50],
        "plausible_records": plausible[:10],
        "sequential_chain": chain,
        "repeat_stride": stride,
        "score": int((len(plausible) * 1000) / (len(records) + 1)) + (100 if chain else 0),
    }
    return best


def scan_tlv4(data, endian, variant_name):
    n = len(data)
    records = []
    type_counts = Counter()
    type3_offsets = []
    plausible = []
    for off in range(0, n - 4 + 1):
        t = data[off]
        tag = data[off + 1]
        ln = int.from_bytes(data[off + 2:off + 4], endian)
        if ln <= 0 or off + 4 + ln > n:
            continue
        records.append((off, t, tag, ln))
        type_counts[t] += 1
        if t == 3:
            type3_offsets.append(off)
            payload = data[off + 4:off + 4 + ln]
            ok, var, zeros, uniq = plausible_bins(payload)
            if ok:
                plausible.append({"offset": off, "type": t, "tag": tag, "length": ln, "payload_first_bytes_hex": payload[:16].hex(), "variance": round(var, 3), "unique": uniq, "zeros": zeros})

    first = []
    for off, t, tag, ln in records[:10]:
        payload = data[off + 4:off + 4 + ln]
        first.append({"offset": off, "type": t, "tag": tag, "length": ln, "payload_first_bytes_hex": payload[:16].hex()})

    chain = False
    stride = None
    if len(records) >= 2:
        deltas = [records[i + 1][0] - records[i][0] for i in range(len(records) - 1)]
        c = Counter(deltas)
        stride, cnt = c.most_common(1)[0]
        chain = cnt >= max(2, len(deltas) * 3 // 4)

    return {
        "variant": variant_name,
        "candidate_count": len(records),
        "plausible_record_count": len(plausible),
        "type_counts": {str(k): v for k, v in type_counts.items()},
        "first_candidate_records": first,
        "type3_offsets": type3_offsets[:50],
        "plausible_records": plausible[:10],
        "sequential_chain": chain,
        "repeat_stride": stride,
        "score": int((len(plausible) * 1000) / (len(records) + 1)) + (100 if chain else 0),
    }


def scan_ff93(data):
    n = len(data)
    offs = [i for i in range(n - 1) if data[i] == 0xff and data[i + 1] == 0x93]
    records = []
    type_counts = Counter()
    plausible = []
    for idx, off in enumerate(offs):
        nxt = offs[idx + 1] if idx + 1 < len(offs) else n
        rec_len = nxt - off
        if rec_len < 32:
            continue
        type_counts[0xff] += 1
        payload = data[off + 4:off + rec_len]
        # Node-detected fixed record format:
        #   4-byte marker/header
        #   12-byte debug metadata (freq-ish fields + tsf)
        #   5 one-byte status fields
        #   72-byte bin payload
        if rec_len >= 93 and off + 93 <= n:
            rec = data[off:off + 93]
            bins = rec[21:93]
            ok, var, zeros, uniq = plausible_bins(bins)
            if ok:
                plausible.append({
                    "offset": off,
                    "length": rec_len,
                    "payload_first_bytes_hex": rec[4:20].hex(),
                    "header_len": 21,
                    "bins_offset": 21,
                    "bins_len": 72,
                    "variance": round(var, 3),
                    "unique": uniq,
                    "zeros": zeros,
                })
        records.append((off, rec_len))

    first = []
    for off, rec_len in records[:10]:
        first.append({"offset": off, "type": 255, "length": rec_len, "payload_first_bytes_hex": data[off + 4:off + 20].hex()})

    chain = False
    stride = None
    if len(records) >= 2:
        deltas = [records[i + 1][0] - records[i][0] for i in range(len(records) - 1)]
        c = Counter(deltas)
        stride, cnt = c.most_common(1)[0]
        chain = cnt >= max(2, len(deltas) * 3 // 4)

    return {
        "variant": "ff93_fixed_stride",
        "candidate_count": len(records),
        "plausible_record_count": len(plausible),
        "type_counts": {str(k): v for k, v in type_counts.items()},
        "first_candidate_records": first,
        "type3_offsets": offs[:50],
        "plausible_records": plausible[:10],
        "sequential_chain": chain,
        "repeat_stride": stride,
        "record_len": 93,
        "header_len": 21,
        "bins_len": 72,
        "leading_padding_bytes": offs[0] if offs else 0,
        "score": int((len(plausible) * 1000) / (len(records) + 1)) + (500 if stride == 93 else 0),
    }


def longest_repeat_guess(data):
    # Lightweight heuristic: look for the dominant repeated offset delta among ff93 markers.
    offs = [i for i in range(len(data) - 1) if data[i] == 0xff and data[i + 1] == 0x93]
    if len(offs) >= 2:
        deltas = [offs[i + 1] - offs[i] for i in range(len(offs) - 1)]
        c = Counter(deltas)
        stride, count = c.most_common(1)[0]
        return {"kind": "ff93_marker_stride", "stride_bytes": stride, "count": count, "markers": len(offs)}
    return {"kind": "none"}


def main():
    if len(sys.argv) != 2:
        print("usage: analyze-spectral-raw.py FILE", file=sys.stderr)
        sys.exit(2)
    data = Path(sys.argv[1]).read_bytes()
    variants = {
        "tlv3_be": scan_tlv(data, 3, "big", "tlv3_be"),
        "tlv3_le": scan_tlv(data, 3, "little", "tlv3_le"),
        "tlv4_be": scan_tlv4(data, "big", "tlv4_be"),
        "tlv4_le": scan_tlv4(data, "little", "tlv4_le"),
        "ff93_fixed_stride": scan_ff93(data),
    }
    best = max(variants.values(), key=lambda v: (v["score"], v["plausible_record_count"], v["candidate_count"]))
    out = {
        "ok": True,
        "file_size": len(data),
        "first_256_hex": data[:256].hex(),
        "first_nonzero_offset": next((i for i, b in enumerate(data) if b != 0), -1),
        "count_zero_bytes": sum(1 for b in data if b == 0),
        "count_nonzero_bytes": sum(1 for b in data if b != 0),
        "likely_binary": True,
        "histogram_first_4096": hist_first(data),
        "variants": variants,
        "best_guess": best,
        "longest_repeated_structure_guess": longest_repeat_guess(data),
    }
    print(json.dumps(out, separators=(",", ":")))


if __name__ == "__main__":
    main()
