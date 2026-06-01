# Spectral Power Formula Audit Report

> **Date:** 2026-05-31 23:07 PDT  
> **Auditor:** builder_bob  
> **Scope:** Section 8 of `WMAC_CALDATA_RESEARCH_AND_TESTING_PLAN.md` — Spectral Data Format Reference  
> **Sources verified against:**
> - [Linux kernel wireless docs](https://wireless.docs.kernel.org/en/latest/en/users/drivers/ath9k/spectral_scan.html)
> - [`spectral_common.h`](https://github.com/torvalds/linux/blob/master/drivers/net/wireless/ath/spectral_common.h) (torvalds/linux master)
> - [`FFT_eval`](https://github.com/simonwunderlich/FFT_eval) reference implementation (`fft_eval_sdl.c`)

---

## 1. Core Formula — ✅ CORRECT

### Our document stated:

$$P_i = \text{NF} + \text{RSSI} + 20 \log_{10}(b_i) - \text{BinSum}$$

where $\text{BinSum} = 10 \log_{10}\left(\sum_{k=0}^{N-1} b_k^2\right)$

### Upstream kernel docs state:

```
power(i) = nf + RSSI + 10*log(b(i)^2) - bin_sum
where:
  RSSI is computed on control chain 0
  b(i) is the magnitude in each bin, unscaled by max_exp
  bin_sum = 10*log(sum[i=1..56](b(i)^2))
```

### Equivalence proof:

```
10 · log₁₀(b²) = 10 · 2 · log₁₀(b) = 20 · log₁₀(b)
```

The two forms are **mathematically identical**. ✅

### FFT_eval reference implementation confirms:

```c
// fft_eval_sdl.c, plot_datapoint():
signal = noise + rssi + 20 * log10f(data) - log10f(datasquaresum) * 10;

// where:
//   data = raw_bin[i] << max_exp        (unscaled bin value)
//   datasquaresum = sum((raw[i] << max_exp)^2)   (energy normalization)
```

All three sources agree on the formula. ✅

---

## 2. Variables — Corrections Required

### 2.1 RSSI — ❌ Was wrong, now fixed

| | Before audit | After audit | Source |
|--|---|---|---|
| Description | "Combined RSSI from the sample header" | **"RSSI, control chain 0"** | Kernel docs: "RSSI is computed on control chain 0" |

The `fft_sample_ht20` struct has a single `s8 rssi` field. The upstream docs explicitly state this is from control chain 0, not a combined/averaged value. For HT40 mode, there are separate `lower_rssi` and `upper_rssi` fields.

### 2.2 b_i (bin value) — ❌ Was incomplete, now fixed

| | Before audit | After audit | Source |
|--|---|---|---|
| Description | "Raw 8-bit bin magnitude (0–255)" | **"`data[i] << max_exp` — bin magnitude unscaled by max_exp"** | Kernel docs: "b(i) is the magnitude in each bin, unscaled by max_exp" |

The raw `data[i]` bytes from the TLV stream have been right-shifted by the hardware. The `max_exp` field records the shift amount. The true magnitude is `data[i] << max_exp`. This shift must be applied **before** computing `log₁₀`.

### 2.3 BinSum — ✅ Was correct

`BinSum = 10·log₁₀(Σ b_k²)` matches upstream `bin_sum = 10*log(sum(b(i)^2))`. Both use the unscaled values.

### 2.4 NF — ✅ Was correct

The `noise` field from the TLV header, in dBm. Kernel docs note the default assumption is -96 dBm but actual values may differ.

---

## 3. max_exp Cancellation Property

A key insight discovered during the audit: `max_exp` mathematically cancels in the power formula. Proof:

```
Let s = max_exp (shift amount)
Let r_i = data[i] (raw byte), b_i = r_i << s = r_i · 2^s

20·log₁₀(b_i) = 20·log₁₀(r_i · 2^s)
               = 20·log₁₀(r_i) + 20·s·log₁₀(2)

BinSum = 10·log₁₀(Σ(r_k · 2^s)²)
       = 10·log₁₀(2^(2s) · Σ r_k²)
       = 10·log₁₀(Σ r_k²) + 20·s·log₁₀(2)

P_i = NF + RSSI + [20·log₁₀(r_i) + 20·s·log₁₀(2)]
                 - [10·log₁₀(Σ r_k²) + 20·s·log₁₀(2)]
    = NF + RSSI + 20·log₁₀(r_i) - 10·log₁₀(Σ r_k²)
```

The `20·s·log₁₀(2)` terms cancel. The final result is the same regardless of `max_exp`.

**However**, you must still apply the shift before `log₁₀()` to avoid `log₁₀(0)` on bins where hardware truncation set `data[i] = 0` but the true magnitude was nonzero. The FFT_eval reference implementation always applies the shift.

---

## 4. TLV Structure — ❌ Six Errors Fixed

### 4.1 Bogus 0x1756 "magic signature" — REMOVED

The document claimed a `0x1756` signature appears in the TLV stream. **This is false.** The relayfs TLV stream has no magic bytes — it's a concatenation of `fft_sample_tlv` headers (type byte + 2-byte big-endian length) followed by type-specific payloads.

The value `0x1756` appears in some AR9003 raw PHY error report documentation as an internal hardware tag, but it is never present in the userspace-facing relayfs output.

### 4.2 Nonexistent `fft_sample_ath9k` struct — REPLACED

The document defined a struct called `fft_sample_ath9k`. **This struct does not exist** in `spectral_common.h`. The actual ath9k types are:

| Type | Enum | Struct | Bins |
|------|------|--------|------|
| 1 | `ATH_FFT_SAMPLE_HT20` | `fft_sample_ht20` | 56 |
| 2 | `ATH_FFT_SAMPLE_HT20_40` | `fft_sample_ht20_40` | 128 |
| 3 | `ATH_FFT_SAMPLE_ATH10K` | `fft_sample_ath10k` | 64/128/256 |
| 4 | `ATH_FFT_SAMPLE_ATH11K` | `fft_sample_ath11k` | 16–512 |

Types 3 and 4 are **different drivers** (ath10k, ath11k), not ath9k variants.

### 4.3 Nonexistent `rssi_ctl[3]` and `rssi_ext[3]` fields — REMOVED

The document listed per-chain RSSI arrays in the payload header. These fields **do not exist** in either `fft_sample_ht20` or `fft_sample_ht20_40`. The HT20 struct has a single `s8 rssi`. The HT40 struct has `s8 lower_rssi` and `s8 upper_rssi`.

### 4.4 Type 3 mislabeled — CORRECTED

The document said type 3 was "HT40 (64 bins, AR9003+)". Type 3 is actually `ATH_FFT_SAMPLE_ATH10K` — a completely different driver for QCA988x/QCA99x0 chipsets, with its own struct layout.

### 4.5 Nonexistent "VHT20 = 64 bins" mode — REMOVED

The document listed a "VHT20" mode with 64 bins for "AR9003+ native". **ath9k has no VHT mode.** The 64-bin count appears only in ath10k. ath9k supports HT20 (56 bins) and HT40 (128 bins) only.

### 4.6 HT40 formula note — ADDED

For HT40 mode, the power formula must be applied separately for each half-channel:
- Bins 0–63 (lower): use `lower_rssi`, `lower_noise`, compute `BinSum` over bins 0–63 only
- Bins 64–127 (upper): use `upper_rssi`, `upper_noise`, compute `BinSum` over bins 64–127 only

This matches the FFT_eval `draw_sample_ht20_40()` implementation which computes `datasquaresum_lower` and `datasquaresum_upper` separately.

---

## 5. Python Reference Implementation — Updated

The `bins_to_dbm()` function was updated to:
1. Accept `max_exp` parameter
2. Apply `<< max_exp` before logarithm computation
3. Document that RSSI is from control chain 0
4. Reference the upstream kernel docs and FFT_eval

---

## 6. Summary

| Item | Status | Detail |
|------|--------|--------|
| Core formula `P_i = NF + RSSI + 20·log₁₀(b_i) − BinSum` | ✅ Correct | Matches upstream kernel docs and FFT_eval |
| BinSum = `10·log₁₀(Σ b_k²)` | ✅ Correct | Matches upstream |
| RSSI source | ❌→✅ Fixed | Was "combined", corrected to "control chain 0" |
| b_i definition | ❌→✅ Fixed | Was raw byte, corrected to `data[i] << max_exp` |
| 0x1756 signature | ❌→✅ Removed | Does not exist in relayfs TLV |
| Struct name `fft_sample_ath9k` | ❌→✅ Fixed | Replaced with real `fft_sample_ht20` + `fft_sample_ht20_40` |
| `rssi_ctl[3]`, `rssi_ext[3]` fields | ❌→✅ Removed | Don't exist in upstream structs |
| Type 3 = "HT40 AR9003+" | ❌→✅ Fixed | Type 3 = ATH_FFT_SAMPLE_ATH10K |
| "VHT20 = 64 bins" | ❌→✅ Removed | ath9k has no VHT mode |
| HT40 separate RSSI/NF/BinSum | ❌→✅ Added | Per upstream struct and FFT_eval |
| Python `bins_to_dbm()` | ❌→✅ Fixed | Now takes `max_exp`, applies shift |
| max_exp cancellation | ✅ Added | Proof + worked examples |

**Bottom line:** The math was right. The data format documentation around it had six structural errors that could have caused incorrect parsing or misinterpreted fields. All fixed and verified against three independent sources.

---

*Commit: `30826c4` — all corrections applied to `WMAC_CALDATA_RESEARCH_AND_TESTING_PLAN.md`*
