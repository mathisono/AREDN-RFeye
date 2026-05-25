#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Robust ath10k spectral TLV parser/probe.
 *
 * - --probe scans a file for candidate TLV records and layout matches.
 * - normal mode emits the first plausible type-3 spectral frames found.
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

static int plausible_bins(size_t bins_len) {
  return (bins_len >= 32 && bins_len <= 2048) || bins_len == 64 || bins_len == 128 ||
         bins_len == 256 || bins_len == 512;
}

static int plausible_freq(uint16_t freq1) { return freq1 >= 2300 && freq1 <= 7100; }

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
  f->score = score_frame(f);
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
  f->score = score_frame(f);
  return f->valid;
}

static int decode_type3(const uint8_t *buf, size_t len, frame_t *out) {
  frame_t c1, c2;
  int ok1 = decode_current_layout(buf, len, &c1);
  int ok2 = decode_compact_layout(buf, len, &c2);

  if (!ok1 && !ok2) return 0;

  if (ok1 && ok2) {
    if (c2.score > c1.score) {
      *out = c2;
    } else {
      *out = c1;
    }
  } else if (ok1) {
    *out = c1;
  } else {
    *out = c2;
  }

  return 1;
}

static void usage(const char *argv0) {
  fprintf(stderr,
          "Usage: %s [--input FILE] [--phy NAME] [--limit N] [--bins N] [--debug] [--stats] [--probe]\n"
          "  --input FILE : TLV stream file (default: stdin)\n"
          "  --phy NAME   : phy label in JSON (default: phy0)\n"
          "  --limit N    : max frames to emit (default: unlimited)\n"
          "  --bins N     : max bins to emit per frame (default: all)\n"
          "  --debug      : print parser warnings to stderr\n"
          "  --stats      : print parse stats JSON to stderr\n"
          "  --probe      : scan file for candidate TLVs/layouts and print JSON\n",
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
         phy, f->layout_id == 2 ? "compact" : "current", f->freq1_mhz, f->freq2_mhz);
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

static void emit_probe_json(const uint8_t *data, size_t size) {
  size_t type_counts[256];
  size_t candidate_records = 0;
  size_t type3_count = 0;
  size_t plausible_count = 0;
  size_t current_matches = 0;
  size_t compact_matches = 0;
  size_t first_nonzero_offset = (size_t)-1;
  size_t zero_bytes = 0;
  size_t nonzero_bytes = 0;
  size_t hist[256];
  size_t first_candidate_count = 0;

  memset(type_counts, 0, sizeof(type_counts));
  memset(hist, 0, sizeof(hist));

  size_t first4096 = size < 4096 ? size : 4096;
  for (size_t i = 0; i < size; i++) {
    if (data[i] == 0) zero_bytes++; else nonzero_bytes++;
    if (first_nonzero_offset == (size_t)-1 && data[i] != 0) first_nonzero_offset = i;
  }
  for (size_t i = 0; i < first4096; i++) hist[data[i]]++;

  printf("{\"ok\":true,\"file_size\":%zu,", size);
  printf("\"first_bytes\":\"");
  hex_bytes(data, size < 64 ? size : 64);
  printf("\",");

  printf("\"candidate_records\":");

  /* First pass counts/records. */
  for (size_t off = 0; off + 3 <= size; off++) {
    uint8_t type = data[off];
    uint16_t len = be16(&data[off + 1]);
    if (len == 0 || off + 3 + (size_t)len > size) continue;

    candidate_records++;
    type_counts[type]++;
    if (type == 3) type3_count++;

    frame_t f;
    if (type == 3 && decode_type3(&data[off + 3], len, &f)) {
      plausible_count++;
      if (f.layout_id == 1) current_matches++; else compact_matches++;
    }
  }

  /* Re-scan for plausible records array. */
  printf("%zu,\"type3_candidate_records\":%zu,\"first_candidate_records\":[", candidate_records, type3_count);
  first_candidate_count = 0;
  for (size_t off = 0; off + 3 <= size && first_candidate_count < 10; off++) {
    uint8_t type = data[off];
    uint16_t len = be16(&data[off + 1]);
    if (len == 0 || off + 3 + (size_t)len > size) continue;
    printf("%s{\"offset\":%zu,\"type\":%u,\"length\":%u,\"payload_first_bytes_hex\":\"",
           first_candidate_count ? "," : "", off, type, len);
    hex_bytes(&data[off + 3], len < 16 ? len : 16);
    printf("\"}");
    first_candidate_count++;
  }
  printf("]");

  printf(",\"plausible_records\":[");
  size_t plausible_emitted = 0;
  for (size_t off = 0; off + 3 <= size && plausible_emitted < 10; off++) {
    uint8_t type = data[off];
    uint16_t len = be16(&data[off + 1]);
    if (type != 3 || len == 0 || off + 3 + (size_t)len > size) continue;
    frame_t f;
    if (!decode_type3(&data[off + 3], len, &f)) continue;
    printf("%s{\"offset\":%zu,\"layout\":\"%s\",\"score\":%d,\"length\":%u,\"bins_len\":%zu,\"freq1_mhz\":%u,\"width_mhz\":%u}",
           plausible_emitted ? "," : "", off, f.layout_id == 2 ? "compact" : "current", f.score, len,
           f.bins_len, f.freq1_mhz, f.width_mhz);
    plausible_emitted++;
  }
  printf("]");

  printf(",\"type3_offsets\":[");
  size_t type3_emitted = 0;
  for (size_t off = 0; off + 3 <= size; off++) {
    uint8_t type = data[off];
    uint16_t len = be16(&data[off + 1]);
    if (type != 3 || len == 0 || off + 3 + (size_t)len > size) continue;
    printf("%s%zu", type3_emitted ? "," : "", off);
    type3_emitted++;
    if (type3_emitted >= 50) break;
  }
  printf("]");

  printf(",\"type_counts\":{");
  size_t first = 1;
  for (int t = 0; t < 256; t++) {
    if (!type_counts[t]) continue;
    printf("%s\"%d\":%zu", first ? "" : ",", t, type_counts[t]);
    first = 0;
  }
  printf("}");

  printf(",\"plausible_record_count\":%zu,\"current_layout_matches\":%zu,\"compact_layout_matches\":%zu,",
         plausible_count, current_matches, compact_matches);
  printf("\"likely_binary\":%s,", (zero_bytes > 0 || nonzero_bytes > 0) ? "true" : "false");
  printf("\"count_zero_bytes\":%zu,\"count_nonzero_bytes\":%zu,", zero_bytes, nonzero_bytes);
  if (first_nonzero_offset == (size_t)-1)
    printf("\"first_nonzero_offset\":-1,");
  else
    printf("\"first_nonzero_offset\":%zu,", first_nonzero_offset);

  printf("\"histogram_first_4096\":{");
  first = 1;
  for (int i = 0; i < 256; i++) {
    printf("%s\"%02x\":%zu", first ? "" : ",", i, hist[i]);
    first = 0;
  }
  printf("}");

  printf("}\n");
}

static int probe_mode(const uint8_t *data, size_t size) {
  emit_probe_json(data, size);
  return 0;
}

static int parse_mode(const uint8_t *data, size_t size, const char *phy, long limit, long bins_limit,
                      int debug, int stats) {
  size_t emitted = 0;
  int trailing_truncated = 0;

  size_t pos = 0;
  while (pos + 3 <= size) {
    size_t found = (size_t)-1;
    uint8_t found_type = 0;
    uint16_t found_len = 0;
    frame_t found_frame;
    memset(&found_frame, 0, sizeof(found_frame));

    for (size_t off = pos; off + 3 <= size; off++) {
      uint8_t type = data[off];
      uint16_t len = be16(&data[off + 1]);
      if (len == 0 || off + 3 + (size_t)len > size) continue;
      if (type != 3) continue;
      frame_t f;
      if (!decode_type3(&data[off + 3], len, &f)) continue;
      found = off;
      found_type = type;
      found_len = len;
      found_frame = f;
      break;
    }

    if (found == (size_t)-1) break;

    if (debug && found > pos) {
      fprintf(stderr, "resync from offset %zu to %zu\n", pos, found);
    }
    if (debug) {
      fprintf(stderr, "matched %s layout at offset %zu len=%u bins=%zu score=%d\n",
              found_frame.layout_id == 2 ? "compact" : "current", found, found_len,
              found_frame.bins_len, found_frame.score);
    }

    print_frame_json(phy, &found_frame, &data[found + 3], found_frame.bins_len, bins_limit);
    emitted++;
    pos = found + 3 + found_len;
    (void)found_type;
    if (limit >= 0 && (long)emitted >= limit) break;
  }

  if (pos < size && pos + 3 > size) trailing_truncated = 1;
  if (stats) {
    fprintf(stderr, "{\"frames_emitted\":%zu,\"trailing_truncated\":%s}\n", emitted,
            trailing_truncated ? "true" : "false");
  }
  if (emitted > 0) return 0;
  return trailing_truncated ? 1 : 0;
}

int main(int argc, char **argv) {
  const char *input = NULL;
  const char *phy = "phy0";
  long limit = -1;
  long bins_limit = -1;
  int debug = 0;
  int stats = 0;
  int probe = 0;

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

  long rc = parse_mode(data, size, phy, limit, bins_limit, debug, stats);
  free(data);
  return (int)rc;
}
