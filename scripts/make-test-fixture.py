#!/usr/bin/env python3
"""Emit a tiny synthetic ath10k spectral TLV fixture for parser smoke tests."""

from __future__ import annotations

import argparse
import struct


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="fixtures/sample-ath10k.bin")
    ap.add_argument("--bins", type=int, default=16)
    args = ap.parse_args()

    bins = bytes([(i * 7) % 255 for i in range(args.bins)])

    # struct fft_sample_ath10k (payload only, TLV header excluded)
    payload = bytearray()
    payload += struct.pack("!B", 80)          # chan_width_mhz
    payload += struct.pack("!H", 5745)        # freq1
    payload += struct.pack("!H", 0)           # freq2
    payload += struct.pack("!H", 0xFFA0)      # noise = -96 (u16)
    payload += struct.pack("!H", 900)         # max_magnitude
    payload += struct.pack("!H", 50)          # total_gain_db
    payload += struct.pack("!H", 20)          # base_pwr_db
    payload += struct.pack("!Q", 123456789)   # tsf
    payload += struct.pack("!b", 115)         # max_index
    payload += struct.pack("!B", 42)          # rssi
    payload += struct.pack("!B", 10)          # relpwr_db
    payload += struct.pack("!B", 9)           # avgpwr_db
    payload += struct.pack("!B", 3)           # max_exp
    payload += bins                            # data[] bins

    tlv = bytearray()
    tlv += struct.pack("!B", 3)               # ATH_FFT_SAMPLE_ATH10K
    tlv += struct.pack("!H", len(payload))
    tlv += payload

    import pathlib

    out = pathlib.Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(tlv)
    print(out)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

