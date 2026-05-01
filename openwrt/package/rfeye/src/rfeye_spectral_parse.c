#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Based on linux drivers/net/wireless/ath/spectral_common.h */

static uint16_t be16(const uint8_t *p) {
  return (uint16_t)((p[0] << 8) | p[1]);
}

static uint64_t be64(const uint8_t *p) {
  uint64_t v = 0;
  for (int i = 0; i < 8; i++) v = (v << 8) | p[i];
  return v;
}

static void usage(const char *argv0) {
  fprintf(stderr,
          "Usage: %s [--input FILE] [--phy NAME] [--limit N] [--bins N]\n"
          "  --input FILE : TLV stream file (default: stdin)\n"
          "  --phy NAME   : phy label in JSON (default: phy0)\n"
          "  --limit N    : max frames to emit (default: unlimited)\n"
          "  --bins N     : max bins to emit per frame (default: all)\n",
          argv0);
}

int main(int argc, char **argv) {
  const char *input = NULL;
  const char *phy = "phy0";
  long limit = -1;
  long bins_limit = -1;

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--input") && i + 1 < argc) {
      input = argv[++i];
    } else if (!strcmp(argv[i], "--phy") && i + 1 < argc) {
      phy = argv[++i];
    } else if (!strcmp(argv[i], "--limit") && i + 1 < argc) {
      limit = strtol(argv[++i], NULL, 10);
    } else if (!strcmp(argv[i], "--bins") && i + 1 < argc) {
      bins_limit = strtol(argv[++i], NULL, 10);
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
      fprintf(stderr, "open %s failed: %s\n", input, strerror(errno));
      return 1;
    }
  }

  long emitted = 0;
  for (;;) {
    uint8_t tlv[3];
    size_t n = fread(tlv, 1, 3, fp);
    if (n == 0) break;
    if (n < 3) {
      fprintf(stderr, "truncated TLV header\n");
      break;
    }

    uint8_t type = tlv[0];
    uint16_t len = be16(&tlv[1]);
    uint8_t *buf = (uint8_t *)malloc(len);
    if (!buf) {
      fprintf(stderr, "oom\n");
      break;
    }

    if (fread(buf, 1, len, fp) != len) {
      fprintf(stderr, "truncated TLV payload\n");
      free(buf);
      break;
    }

    /* ATH_FFT_SAMPLE_ATH10K == 3 */
    if (type == 3 && len >= 26) {
      uint8_t chan_width_mhz = buf[0];
      uint16_t freq1 = be16(&buf[1]);
      uint16_t freq2 = be16(&buf[3]);
      int16_t noise = (int16_t)be16(&buf[5]);
      uint16_t max_magnitude = be16(&buf[7]);
      /* total_gain_db/base_pwr_db currently ignored in JSON */
      uint64_t tsf = be64(&buf[13]);
      int8_t max_index = (int8_t)buf[21];
      uint8_t rssi = buf[22];

      size_t bins_len = (size_t)(len - 26);
      size_t out_bins = bins_len;
      if (bins_limit >= 0 && (size_t)bins_limit < out_bins) out_bins = (size_t)bins_limit;

      printf("{\"phy\":\"%s\",\"freq1_mhz\":%u,\"freq2_mhz\":%u,\"width_mhz\":%u,\"noise\":%d,\"rssi\":%u,\"max_index\":%d,\"max_magnitude\":%u,\"tsf\":%llu,\"bins\":[",
             phy,
             freq1,
             freq2,
             chan_width_mhz,
             (int)noise,
             (unsigned)rssi,
             (int)max_index,
             (unsigned)max_magnitude,
             (unsigned long long)tsf);
      for (size_t i = 0; i < out_bins; i++) {
        if (i) putchar(',');
        printf("%u", (unsigned)buf[26 + i]);
      }
      printf("]}\n");

      emitted++;
      if (limit >= 0 && emitted >= limit) {
        free(buf);
        break;
      }
    }

    free(buf);
  }

  if (fp != stdin) fclose(fp);
  return 0;
}

