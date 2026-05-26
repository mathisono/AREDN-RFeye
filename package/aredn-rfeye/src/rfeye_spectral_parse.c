#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* RFeye spectral raw parser/probe.
 *
 * Supports:
 * - legacy ath10k-style sample fixtures (type-3 TLV-ish)
 * - the node's actual ff93-framed fixed-stride raw capture stream
 */

typedef struct {
  int valid;
  int layout_id;
  size_t header_size;
  uint8_t width_raw;
  uint16_t width_mhz;
  uint16_t freq1_mhz;
  uint16_t freq2_mhz;
  int noise_dbm;
  uint16_t max_magnitude;
  uint64_t tsf;
  int max_index;
  int rssi;
  int relpwr_db;
  int avgpwr_db;
  uint8_t max_exp;
  size_t bins_offset;
  size_t bins_len;
  int freq_valid;
  int bins_plausible;
  int score;
} frame_t;

static uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint16_t le16(const uint8_t *p) { return (uint16_t)((p[1] << 8) | p[0]); }

static uint64_t be64(const uint8_t *p) {
  uint64_t v = 0;
  for (int i = 0; i < 8; i++) v = (v << 8) | p[i];
  return v;
}

static uint16_t map_width(uint8_t raw) {
  switch (raw) {
    case 20:
    case 40:
    case 80:
    case 160:
      return raw;
    case 1:
      return 20;
    case 2:
      return 40;
    case 4:
      return 80;
    case 8:
      return 160;
    default:
      return 0;
  }
}

static int plausible_freq(uint16_t freq1) { return freq1 >= 2300 && freq1 <= 7100; }

static int plausible_bins(size_t bins_len) {
  return bins_len == 64 || bins_len == 128 || bins_len == 256 || bins_len == 512 ||
         (bins_len >= 32 && bins_len <= 2048);
}

static int bins_score(const uint8_t *bins, size_t len) {
  if (!bins || !len) return -1000;
  size_t nonzero = 0;
  uint8_t minv = 255, maxv = 0;
  double sum = 0.0, sumsq = 0.0;
  for (size_t i = 0; i < len; i++) {
    uint8_t v = bins[i];
    if (v) nonzero++;
    if (v < minv) minv = v;
    if (v > maxv) maxv = v;
    sum += (double)v;
    sumsq += (double)v * (double)v;
  }
  double mean = sum / (double)len;
  double var = (sumsq / (double)len) - (mean * mean);
  int s = 0;
  if (nonzero > 0) s += 2;
  if (nonzero > len / 8) s += 2;
  if (maxv > minv) s += 2;
  if (var > 1.0) s += 2;
  if (var > 10.0) s += 1;
  return s;
}

static int score_frame(const frame_t *f) {
  int s = 0;
  if (!f->valid) return -1000;
  if (f->width_mhz == 20 || f->width_mhz == 40 || f->width_mhz == 80 || f->width_mhz == 160)
    s += 3;
  if (plausible_freq(f->freq1_mhz)) s += 3;
  if (f->bins_len == 64 || f->bins_len == 128 || f->bins_len == 256 || f->bins_len == 512)
    s += 3;
  else if (f->bins_len >= 32 && f->bins_len <= 1024)
    s += 1;
  if (f->rssi >= 0 && f->rssi <= 255) s += 1;
  if (f->max_index >= -128 && f->max_index <= 127) s += 1;
  return s;
}

static void init_frame(frame_t *f, int layout_id) {
  memset(f, 0, sizeof(*f));
  f->layout_id = layout_id;
}

static int decode_current_layout(const uint8_t *buf, size_t len, frame_t *f) {
  if (len < 26) return 0;
  init_frame(f, 1);
  f->header_size = 26;
  f->width_raw = buf[0];
  f->width_mhz = map_width(f->width_raw);
  f->freq1_mhz = be16(&buf[1]);
  f->freq2_mhz = be16(&buf[3]);
  f->noise_dbm = (int16_t)be16(&buf[5]);
  f->max_magnitude = be16(&buf[7]);
  f->tsf = be64(&buf[13]);
  f->max_index = (int8_t)buf[21];
  f->rssi = buf[22];
  f->relpwr_db = (int8_t)buf[23];
  f->avgpwr_db = (int8_t)buf[24];
  f->max_exp = buf[25];
  f->bins_offset = 26;
  f->bins_len = len - 26;
  f->freq_valid = (f->width_mhz != 0) && plausible_freq(f->freq1_mhz);
  f->bins_plausible = plausible_bins(f->bins_len);
  f->valid = f->bins_plausible;
  f->score = score_frame(f) + bins_score(&buf[f->bins_offset], f->bins_len);
  return f->valid;
}

static int decode_compact_layout(const uint8_t *buf, size_t len, frame_t *f) {
  if (len < 23) return 0;
  init_frame(f, 2);
  f->header_size = 23;
  f->width_raw = buf[0];
  f->width_mhz = map_width(f->width_raw);
  f->freq1_mhz = be16(&buf[1]);
  f->freq2_mhz = be16(&buf[3]);
  f->noise_dbm = (int8_t)buf[5];
  f->max_magnitude = be16(&buf[6]);
  f->tsf = be64(&buf[10]);
  f->max_index = (int8_t)buf[18];
  f->rssi = buf[19];
  f->relpwr_db = (int8_t)buf[20];
  f->avgpwr_db = (int8_t)buf[21];
  f->max_exp = buf[22];
  f->bins_offset = 23;
  f->bins_len = len - 23;
  f->freq_valid = (f->width_mhz != 0) && plausible_freq(f->freq1_mhz);
  f->bins_plausible = plausible_bins(f->bins_len);
  f->valid = f->bins_plausible;
  f->score = score_frame(f) + bins_score(&buf[f->bins_offset], f->bins_len);
  return f->valid;
}

static int decode_type3(const uint8_t *buf, size_t len, frame_t *out) {
  frame_t c1, c2;
  int ok1 = decode_current_layout(buf, len, &c1);
  int ok2 = decode_compact_layout(buf, len, &c2);
  if (!ok1 && !ok2) return 0;
  if (ok1 && ok2) {
    *out = (c2.score > c1.score) ? c2 : c1;
  } else if (ok1) {
    *out = c1;
  } else {
    *out = c2;
  }
  return 1;
}

static int decode_ff93_record(const uint8_t *rec, size_t rec_len, frame_t *out) {
  /* Node-detected format: 0xff 0x93 marker, 93-byte fixed stride.
   * Payload layout observed on this node:
   *   u16 field_a
   *   u16 field_b
   *   u64 tsf
   *   u8  max_index
   *   u8  rssi
   *   s8  relpwr_db
   *   s8  avgpwr_db
   *   u8  max_exp
   *   u8  bins[72]
   * The first two fields are stable but do not map cleanly to MHz, so we treat
   * them as debug metadata rather than rejecting the frame when they are implausible.
   */
  if (rec_len < 93) return 0;
  if (rec[0] != 0xff || rec[1] != 0x93) return 0;
  init_frame(out, 3);
  out->header_size = 21;
  out->width_raw = 0;
  out->width_mhz = 0;
  out->freq1_mhz = be16(&rec[4]);
  out->freq2_mhz = be16(&rec[6]);
  out->noise_dbm = 0;
  out->max_magnitude = 0;
  out->tsf = be64(&rec[8]);
  out->max_index = (int8_t)rec[16];
  out->rssi = rec[17];
  out->relpwr_db = (int8_t)rec[18];
  out->avgpwr_db = (int8_t)rec[19];
  out->max_exp = rec[20];
  out->bins_offset = 21;
  out->bins_len = rec_len - out->bins_offset;
  out->bins_plausible = plausible_bins(out->bins_len);
  out->freq_valid = 0;
  out->valid = out->bins_plausible;
  for (size_t i = 0; i < out->bins_len; i++) {
    uint8_t v = rec[out->bins_offset + i];
    if (v > out->max_magnitude) {
      out->max_magnitude = v;
      out->max_index = (int)i;
    }
  }
  out->score = 0;
  out->score += bins_score(&rec[out->bins_offset], out->bins_len);
  if (out->bins_len == 72) out->score += 4;
  if (out->tsf != 0) out->score += 2;
  if (out->max_index >= 0 && out->max_index < (int)out->bins_len) out->score += 2;
  if (out->rssi <= 127) out->score += 1;
  return out->valid;
}

static const char *layout_name(int layout_id) {
  switch (layout_id) {
    case 2: return "compact";
    case 3: return "ff93_fixed";
    default: return "current";
  }
}

static void usage(const char *argv0) {
  fprintf(stderr,
          "Usage: %s [--input FILE] [--phy NAME] [--limit N] [--bins N] [--debug] [--stats] [--probe] [--resync]\n"
          "  --input FILE : raw capture file (default: stdin)\n"
          "  --phy NAME   : phy label in JSON (default: phy0)\n"
          "  --limit N    : max frames to emit (default: unlimited)\n"
          "  --bins N     : max bins to emit per frame (default: all)\n"
          "  --debug      : print parser warnings to stderr\n"
          "  --stats      : print parse stats JSON to stderr\n"
          "  --probe      : scan file for candidate layouts and print JSON\n"
          "  --resync     : scan forward for plausible frame boundaries\n",
          argv0);
}

static void hex_bytes(const uint8_t *p, size_t n) {
  for (size_t i = 0; i < n; i++) printf("%02x", p[i]);
}

static void print_frame_json(const char *phy, const frame_t *f, const uint8_t *payload,
                             size_t payload_len, long bins_limit) {
  size_t out_bins = payload_len;
  if (bins_limit >= 0 && (size_t)bins_limit < out_bins) out_bins = (size_t)bins_limit;
  printf("{\"phy\":\"%s\",\"layout\":\"%s\",\"freq1_mhz\":%u,\"freq2_mhz\":%u,",
         phy, layout_name(f->layout_id), f->freq1_mhz, f->freq2_mhz);
  printf("\"width_mhz\":%u,\"freq_valid\":%s,\"noise\":%d,\"rssi\":%d,",
         f->width_mhz, f->freq_valid ? "true" : "false", f->noise_dbm, f->rssi);
  printf("\"max_index\":%d,\"max_magnitude\":%u,\"tsf\":%llu,\"bins\":[",
         f->max_index, f->max_magnitude, (unsigned long long)f->tsf);
  for (size_t i = 0; i < out_bins; i++) {
    if (i) putchar(',');
    printf("%u", (unsigned)payload[i]);
  }
  printf("]}\n");
}

typedef struct {
  size_t offset;
  unsigned type;
  unsigned tag;
  unsigned length;
  char payload_hex[33];
} candidate_t;

typedef struct {
  const char *name;
  size_t candidate_count;
  size_t plausible_count;
  size_t type_counts[256];
  size_t type3_offsets[50];
  size_t type3_count;
  candidate_t first[10];
  size_t first_count;
  candidate_t plausible[10];
  size_t plausible_count_emitted;
  int sequential_chain;
  size_t repeat_stride;
  int score;
} scan_summary_t;

static void candidate_snippet(char *dst, size_t dst_len, const uint8_t *p, size_t n) {
  size_t lim = n < 16 ? n : 16;
  size_t pos = 0;
  if (dst_len == 0) return;
  for (size_t i = 0; i < lim && pos + 2 < dst_len; i++) {
    int wrote = snprintf(dst + pos, dst_len - pos, "%02x", p[i]);
    if (wrote < 0) break;
    pos += (size_t)wrote;
  }
  dst[(pos < dst_len) ? pos : dst_len - 1] = '\0';
}

static void scan_init(scan_summary_t *s, const char *name) { memset(s, 0, sizeof(*s)); s->name = name; }

static void scan_add_first(scan_summary_t *s, size_t off, unsigned type, unsigned tag, unsigned len,
                           const uint8_t *payload, size_t payload_len) {
  if (s->first_count >= 10) return;
  candidate_t *c = &s->first[s->first_count++];
  c->offset = off; c->type = type; c->tag = tag; c->length = len;
  candidate_snippet(c->payload_hex, sizeof(c->payload_hex), payload, payload_len);
}

static void scan_add_plausible(scan_summary_t *s, size_t off, unsigned type, unsigned tag, unsigned len,
                               const uint8_t *payload, size_t payload_len) {
  if (s->plausible_count_emitted >= 10) return;
  candidate_t *c = &s->plausible[s->plausible_count_emitted++];
  c->offset = off; c->type = type; c->tag = tag; c->length = len;
  candidate_snippet(c->payload_hex, sizeof(c->payload_hex), payload, payload_len);
}

static void scan_finalize(scan_summary_t *s, const size_t *offs, size_t count) {
  if (count < 2) return;
  size_t best_stride = 0, best_hits = 0;
  for (size_t i = 1; i < count; i++) {
    size_t delta = offs[i] - offs[i - 1];
    size_t hits = 1;
    for (size_t j = i + 1; j < count; j++) if (offs[j] - offs[j - 1] == delta) hits++;
    if (hits > best_hits) { best_hits = hits; best_stride = delta; }
  }
  s->repeat_stride = best_stride;
  s->sequential_chain = best_hits >= (count > 2 ? (count - 1) * 3 / 4 : 1);
}

static void scan_tlv3_variant(const uint8_t *data, size_t size, const char *name, int little,
                              scan_summary_t *out) {
  scan_init(out, name);
  size_t offs[1024]; size_t off_count = 0;
  for (size_t off = 0; off + 3 <= size; off++) {
    unsigned type = data[off];
    unsigned len = little ? (unsigned)le16(&data[off + 1]) : (unsigned)be16(&data[off + 1]);
    if (!len || off + 3 + len > size) continue;
    if (off_count < 1024) offs[off_count++] = off;
    out->candidate_count++; out->type_counts[type]++;
    scan_add_first(out, off, type, 0, len, &data[off + 3], len);
    if (type == 3) {
      if (out->type3_count < 50) out->type3_offsets[out->type3_count++] = off;
      if (len >= 23 && len <= 1024) {
        frame_t f;
        if (decode_type3(&data[off + 3], len, &f)) {
          out->plausible_count++;
          scan_add_plausible(out, off, type, 0, len, &data[off + 3], len);
        }
      }
    }
  }
  scan_finalize(out, offs, off_count);
  out->score = (int)((out->plausible_count * 1000) / (out->candidate_count + 1) +
                     (out->sequential_chain ? 100 : 0));
}

static void scan_tlv4_variant(const uint8_t *data, size_t size, const char *name, int little,
                              scan_summary_t *out) {
  scan_init(out, name);
  size_t offs[1024]; size_t off_count = 0;
  for (size_t off = 0; off + 4 <= size; off++) {
    unsigned type = data[off];
    unsigned tag = data[off + 1];
    unsigned len = little ? (unsigned)le16(&data[off + 2]) : (unsigned)be16(&data[off + 2]);
    if (!len || off + 4 + len > size) continue;
    if (off_count < 1024) offs[off_count++] = off;
    out->candidate_count++; out->type_counts[type]++;
    scan_add_first(out, off, type, tag, len, &data[off + 4], len);
    if (type == 3 || (type == 0xff && tag == 0x93)) {
      if (type == 3 && out->type3_count < 50) out->type3_offsets[out->type3_count++] = off;
      if (len >= 23 && len <= 1024) {
        frame_t f;
        if (decode_type3(&data[off + 4], len, &f)) {
          out->plausible_count++;
          scan_add_plausible(out, off, type, tag, len, &data[off + 4], len);
        }
      }
    }
  }
  scan_finalize(out, offs, off_count);
  out->score = (int)((out->plausible_count * 1000) / (out->candidate_count + 1) +
                     (out->sequential_chain ? 100 : 0));
}

static void scan_ff93_variant(const uint8_t *data, size_t size, scan_summary_t *out) {
  scan_init(out, "ff93_fixed_stride");
  size_t offs[1024]; size_t off_count = 0;
  for (size_t off = 0; off + 1 < size; off++) {
    if (data[off] != 0xff || data[off + 1] != 0x93) continue;
    size_t next = size;
    for (size_t j = off + 2; j + 1 < size; j++) {
      if (data[j] == 0xff && data[j + 1] == 0x93) { next = j; break; }
    }
    size_t len = next - off;
    if (off_count < 1024) offs[off_count++] = off;
    out->candidate_count++;
    out->type_counts[255]++;
    scan_add_first(out, off, 255, 0x93, (unsigned)len, &data[off + 4], len > 4 ? len - 4 : 0);
    if (len >= 93 && off + 93 <= size) {
      frame_t f;
      if (decode_ff93_record(&data[off], 93, &f)) {
        out->plausible_count++;
        if (out->type3_count < 50) out->type3_offsets[out->type3_count++] = off;
        scan_add_plausible(out, off, 255, 0x93, 93, &data[off + 4], 16);
      }
    }
  }
  scan_finalize(out, offs, off_count);
  out->score = (int)((out->plausible_count * 1000) / (out->candidate_count + 1) +
                     (out->sequential_chain ? 500 : 0) + (out->repeat_stride == 93 ? 500 : 0));
}

static void print_scan_summary(const scan_summary_t *s) {
  printf("\"%s\":{", s->name);
  printf("\"candidate_count\":%zu,\"plausible_record_count\":%zu,", s->candidate_count, s->plausible_count);
  printf("\"type_counts\":{");
  size_t first = 1;
  for (int i = 0; i < 256; i++) {
    if (!s->type_counts[i]) continue;
    printf("%s\"%d\":%zu", first ? "" : ",", i, s->type_counts[i]);
    first = 0;
  }
  printf("},\"first_candidate_records\":[");
  for (size_t i = 0; i < s->first_count; i++) {
    const candidate_t *c = &s->first[i];
    printf("%s{\"offset\":%zu,\"type\":%u,\"tag\":%u,\"length\":%u,\"payload_first_bytes_hex\":\"%s\"}",
           i ? "," : "", c->offset, c->type, c->tag, c->length, c->payload_hex);
  }
  printf("],\"type3_offsets\":[");
  for (size_t i = 0; i < s->type3_count; i++) printf("%s%zu", i ? "," : "", s->type3_offsets[i]);
  printf("],\"plausible_records\":[");
  for (size_t i = 0; i < s->plausible_count_emitted; i++) {
    const candidate_t *c = &s->plausible[i];
    printf("%s{\"offset\":%zu,\"type\":%u,\"tag\":%u,\"length\":%u,\"payload_first_bytes_hex\":\"%s\"}",
           i ? "," : "", c->offset, c->type, c->tag, c->length, c->payload_hex);
  }
  printf("],\"sequential_chain\":%s,\"repeat_stride\":%zu,\"score\":%d}",
         s->sequential_chain ? "true" : "false", s->repeat_stride, s->score);
}

static void emit_probe_json(const uint8_t *data, size_t size) {
  const size_t probe_window_max = 65536;
  size_t scan_size = size > probe_window_max ? probe_window_max : size;
  size_t zero_bytes = 0, nonzero_bytes = 0;
  size_t first_nonzero_offset = (size_t)-1;
  size_t hist[256];
  memset(hist, 0, sizeof(hist));
  for (size_t i = 0; i < size; i++) {
    if (data[i] == 0) zero_bytes++; else nonzero_bytes++;
    if (first_nonzero_offset == (size_t)-1 && data[i] != 0) first_nonzero_offset = i;
  }
  for (size_t i = 0; i < (size < 4096 ? size : 4096); i++) hist[data[i]]++;

  scan_summary_t v3be, v3le, v4be, v4le, ff93;
  scan_tlv3_variant(data, scan_size, "tlv3_be", 0, &v3be);
  scan_tlv3_variant(data, scan_size, "tlv3_le", 1, &v3le);
  scan_tlv4_variant(data, scan_size, "tlv4_be", 0, &v4be);
  scan_tlv4_variant(data, scan_size, "tlv4_le", 1, &v4le);
  scan_ff93_variant(data, scan_size, &ff93);

  const scan_summary_t *best = &v3be;
  if (v3le.score > best->score) best = &v3le;
  if (v4be.score > best->score) best = &v4be;
  if (v4le.score > best->score) best = &v4le;
  if (ff93.score > best->score) best = &ff93;

  printf("{\"ok\":true,\"file_size\":%zu,\"probe_scan_window\":%zu,\"first_bytes\":\"", size, scan_size);
  hex_bytes(data, size < 64 ? size : 64);
  printf("\",\"candidate_records\":%zu,\"type3_candidate_records\":%zu,", best->candidate_count, best->type3_count);
  printf("\"variants\":{");
  print_scan_summary(&v3be); printf(",");
  print_scan_summary(&v3le); printf(",");
  print_scan_summary(&v4be); printf(",");
  print_scan_summary(&v4le); printf(",");
  print_scan_summary(&ff93);
  printf("},\"best_scoring_variant\":\"%s\",\"best_guess\":\"%s\",", best->name, best->name);
  printf("\"likely_binary\":%s,", (zero_bytes > 0 || nonzero_bytes > 0) ? "true" : "false");
  printf("\"count_zero_bytes\":%zu,\"count_nonzero_bytes\":%zu,", zero_bytes, nonzero_bytes);
  if (first_nonzero_offset == (size_t)-1) printf("\"first_nonzero_offset\":-1,"); else printf("\"first_nonzero_offset\":%zu,", first_nonzero_offset);
  printf("\"histogram_first_4096\":{");
  for (int i = 0; i < 256; i++) printf("%s\"%02x\":%zu", i ? "," : "", i, hist[i]);
  printf("},\"longest_repeated_structure_guess\":{\"kind\":\"ff93_marker_stride\",\"stride_bytes\":%zu,\"markers\":%zu}}\n",
         ff93.repeat_stride ? ff93.repeat_stride : 157, ff93.candidate_count);
}

static int probe_mode(const uint8_t *data, size_t size) {
  emit_probe_json(data, size);
  return 0;
}

static size_t count_ff93_markers(const uint8_t *data, size_t size) {
  size_t c = 0;
  for (size_t i = 0; i + 1 < size; i++) if (data[i] == 0xff && data[i + 1] == 0x93) c++;
  return c;
}

static int parse_ff93_stream(const uint8_t *data, size_t size, const char *phy, long limit, long bins_limit,
                             int debug, int stats, size_t *skipped_out) {
  const size_t record_len = 93;
  size_t emitted = 0;
  size_t skipped = 0;
  for (size_t pos = 0; pos + record_len <= size; ) {
    if (data[pos] != 0xff || data[pos + 1] != 0x93) {
      pos++;
      skipped++;
      continue;
    }
    frame_t f;
    if (!decode_ff93_record(&data[pos], record_len, &f)) {
      if (debug) fprintf(stderr, "ff93 candidate at %zu rejected\n", pos);
      pos++;
      skipped++;
      continue;
    }
    if (debug) {
      fprintf(stderr, "matched ff93 record at offset %zu len=%zu bins=%zu score=%d field_a=%u field_b=%u tsf=%llu\n",
              pos, record_len, f.bins_len, f.score, f.freq1_mhz, f.freq2_mhz,
              (unsigned long long)f.tsf);
    }
    print_frame_json(phy, &f, &data[pos + f.bins_offset], f.bins_len, bins_limit);
    emitted++;
    pos += record_len;
    if (limit >= 0 && (long)emitted >= limit) break;
  }
  if (stats) fprintf(stderr, "{\"frames_emitted\":%zu,\"skipped_bytes\":%zu}\n", emitted, skipped);
  if (skipped_out) *skipped_out = skipped;
  return emitted > 0 ? 0 : 1;
}

static int parse_type3_stream(const uint8_t *data, size_t size, const char *phy, long limit, long bins_limit,
                              int debug, int stats, int resync, size_t *skipped_out) {
  size_t emitted = 0;
  size_t skipped = 0;
  size_t pos = 0;
  while (pos + 3 <= size) {
    size_t found = (size_t)-1;
    uint16_t found_len = 0;
    frame_t found_frame;
    memset(&found_frame, 0, sizeof(found_frame));
    for (size_t off = pos; off + 3 <= size; off++) {
      uint8_t type = data[off];
      uint16_t len = be16(&data[off + 1]);
      if (!len || off + 3 + (size_t)len > size) continue;
      if (type != 3) continue;
      frame_t f;
      if (!decode_type3(&data[off + 3], len, &f)) continue;
      found = off;
      found_len = len;
      found_frame = f;
      break;
    }
    if (found == (size_t)-1) break;
    if (debug && found > pos) fprintf(stderr, "resync from offset %zu to %zu\n", pos, found);
    if (debug) fprintf(stderr, "matched %s layout at offset %zu len=%u bins=%zu score=%d\n",
                       layout_name(found_frame.layout_id), found, found_len, found_frame.bins_len,
                       found_frame.score);
    print_frame_json(phy, &found_frame, &data[found + found_frame.bins_offset], found_frame.bins_len,
                     bins_limit);
    emitted++;
    if (found > pos) skipped += found - pos;
    pos = found + 3 + found_len;
    if (!resync) break;
    if (limit >= 0 && (long)emitted >= limit) break;
  }
  if (stats) fprintf(stderr, "{\"frames_emitted\":%zu,\"skipped_bytes\":%zu}\n", emitted, skipped);
  if (skipped_out) *skipped_out = skipped;
  return emitted > 0 ? 0 : 1;
}

static int parse_mode(const uint8_t *data, size_t size, const char *phy, long limit, long bins_limit,
                      int debug, int stats, int resync) {
  size_t ff93_markers = count_ff93_markers(data, size);
  if (ff93_markers >= 3) {
    int rc = parse_ff93_stream(data, size, phy, limit, bins_limit, debug, stats, NULL);
    if (rc == 0) return 0;
    if (!resync) return rc;
  }
  return parse_type3_stream(data, size, phy, limit, bins_limit, debug, stats, resync, NULL);
}

int main(int argc, char **argv) {
  const char *input = NULL;
  const char *phy = "phy0";
  long limit = -1;
  long bins_limit = -1;
  int debug = 0;
  int stats = 0;
  int probe = 0;
  int resync = 0;

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--input") && i + 1 < argc) {
      input = argv[++i];
    } else if (!strcmp(argv[i], "--phy") && i + 1 < argc) {
      phy = argv[++i];
    } else if (!strcmp(argv[i], "--limit") && i + 1 < argc) {
      limit = strtol(argv[++i], NULL, 10);
    } else if (!strcmp(argv[i], "--bins") && i + 1 < argc) {
      bins_limit = strtol(argv[++i], NULL, 10);
    } else if (!strcmp(argv[i], "--debug")) {
      debug = 1;
    } else if (!strcmp(argv[i], "--stats")) {
      stats = 1;
    } else if (!strcmp(argv[i], "--probe")) {
      probe = 1;
    } else if (!strcmp(argv[i], "--resync")) {
      resync = 1;
    } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
      usage(argv[0]);
      return 0;
    } else {
      usage(argv[0]);
      return 2;
    }
  }

  FILE *fp = stdin;
  if (input) {
    fp = fopen(input, "rb");
    if (!fp) {
      printf("{\"ok\":false,\"error\":\"open %s failed: %s\"}\n", input, strerror(errno));
      return 1;
    }
  }

  if (fseek(fp, 0, SEEK_END) != 0) {
    if (fp != stdin) fclose(fp);
    printf("{\"ok\":false,\"error\":\"seek failed\"}\n");
    return 1;
  }
  long flen = ftell(fp);
  if (flen < 0) {
    if (fp != stdin) fclose(fp);
    printf("{\"ok\":false,\"error\":\"tell failed\"}\n");
    return 1;
  }
  if (fseek(fp, 0, SEEK_SET) != 0) {
    if (fp != stdin) fclose(fp);
    printf("{\"ok\":false,\"error\":\"rewind failed\"}\n");
    return 1;
  }

  size_t size = (size_t)flen;
  uint8_t *data = NULL;
  if (size > 0) {
    data = (uint8_t *)malloc(size);
    if (!data) {
      if (fp != stdin) fclose(fp);
      printf("{\"ok\":false,\"error\":\"oom\"}\n");
      return 1;
    }
    if (fread(data, 1, size, fp) != size) {
      free(data);
      if (fp != stdin) fclose(fp);
      printf("{\"ok\":false,\"error\":\"short read\"}\n");
      return 1;
    }
  }
  if (fp != stdin) fclose(fp);

  if (probe) {
    int rc = probe_mode(data, size);
    free(data);
    return rc;
  }

  int rc = parse_mode(data, size, phy, limit, bins_limit, debug, stats, resync);
  free(data);
  return rc;
}
