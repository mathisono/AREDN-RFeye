#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/* rfeye-wmac-rebind: tiny POSIX helper for r17 WMAC caldata provisioning.
 *
 * This intentionally avoids system(3), shell expansion, and broad ath9k module
 * removal.  It writes directly to the ath9k platform driver's sysfs bind and
 * unbind controls for the QCA9558 WMAC device only.
 */

#define DEFAULT_WMAC_DEV "18100000.wmac"
#define DEFAULT_DRIVER_DIR "/sys/bus/platform/drivers/ath9k"
#define DEFAULT_SOURCE "/usr/lib/rfeye/caldata/ath9k-caldata-wmac-wa-reference.bin"
#define DEFAULT_DEST "/lib/firmware/ath9k-eeprom-ahb-18100000.wmac.bin"
#define COPY_BUFSZ 4096

static const char *wmac_dev = DEFAULT_WMAC_DEV;
static const char *driver_dir = DEFAULT_DRIVER_DIR;
static const char *src_path = DEFAULT_SOURCE;
static const char *dst_path = DEFAULT_DEST;
static int do_install = 0;
static int do_unbind = 0;
static int do_bind = 0;
static int do_status = 0;
static int quiet = 0;

static void json_escape(FILE *out, const char *s) {
  if (!s) return;
  for (; *s; s++) {
    switch (*s) {
      case '\\': fputs("\\\\", out); break;
      case '"': fputs("\\\"", out); break;
      case '\n': fputs("\\n", out); break;
      case '\r': fputs("\\r", out); break;
      case '\t': fputs("\\t", out); break;
      default: fputc(*s, out); break;
    }
  }
}

static void usage(const char *argv0) {
  fprintf(stderr,
          "Usage: %s [--status] [--install] [--unbind] [--bind] [--rebind] [--provision]\n"
          "          [--source FILE] [--dest FILE] [--device ID] [--driver-dir DIR] [--quiet]\n"
          "\n"
          "Defaults:\n"
          "  --device     %s\n"
          "  --driver-dir %s\n"
          "  --source     %s\n"
          "  --dest       %s\n",
          argv0, DEFAULT_WMAC_DEV, DEFAULT_DRIVER_DIR, DEFAULT_SOURCE, DEFAULT_DEST);
}

static int path_join(char *buf, size_t bufsz, const char *a, const char *b) {
  int n = snprintf(buf, bufsz, "%s/%s", a, b);
  return (n > 0 && (size_t)n < bufsz) ? 0 : -1;
}

static bool exists_path(const char *path) {
  struct stat st;
  return stat(path, &st) == 0;
}

static bool writable_path(const char *path) {
  return access(path, W_OK) == 0;
}

static int mkdir_parent(const char *path) {
  char tmp[512];
  const char *slash;
  size_t len;

  slash = strrchr(path, '/');
  if (!slash || slash == path) return 0;
  len = (size_t)(slash - path);
  if (len >= sizeof(tmp)) return -1;
  memcpy(tmp, path, len);
  tmp[len] = '\0';

  if (exists_path(tmp)) return 0;
  if (mkdir_parent(tmp) != 0) return -1;
  if (mkdir(tmp, 0755) == 0 || errno == EEXIST) return 0;
  return -1;
}

static int validate_caldata(const char *path, char *err, size_t errsz) {
  int fd = open(path, O_RDONLY);
  uint8_t buf[COPY_BUFSZ];
  ssize_t n;
  unsigned long total = 0;
  int all_00 = 1;
  int all_ff = 1;

  if (fd < 0) {
    snprintf(err, errsz, "open source failed: %s", strerror(errno));
    return -1;
  }

  while ((n = read(fd, buf, sizeof(buf))) > 0) {
    total += (unsigned long)n;
    for (ssize_t i = 0; i < n; i++) {
      if (buf[i] != 0x00) all_00 = 0;
      if (buf[i] != 0xff) all_ff = 0;
    }
  }

  if (n < 0) {
    snprintf(err, errsz, "read source failed: %s", strerror(errno));
    close(fd);
    return -1;
  }
  close(fd);

  if (total < 1024) {
    snprintf(err, errsz, "caldata too small: %lu bytes", total);
    return -1;
  }
  if (all_00 || all_ff) {
    snprintf(err, errsz, "refusing blank caldata: all_%s", all_00 ? "00" : "ff");
    return -1;
  }

  return 0;
}

static int copy_file_checked(const char *src, const char *dst, char *err, size_t errsz) {
  int in_fd = -1, out_fd = -1;
  uint8_t buf[COPY_BUFSZ];
  ssize_t n;

  if (validate_caldata(src, err, errsz) != 0) return -1;
  if (mkdir_parent(dst) != 0) {
    snprintf(err, errsz, "mkdir parent failed for %s: %s", dst, strerror(errno));
    return -1;
  }

  in_fd = open(src, O_RDONLY);
  if (in_fd < 0) {
    snprintf(err, errsz, "open source failed: %s", strerror(errno));
    return -1;
  }

  out_fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (out_fd < 0) {
    snprintf(err, errsz, "open dest failed: %s", strerror(errno));
    close(in_fd);
    return -1;
  }

  while ((n = read(in_fd, buf, sizeof(buf))) > 0) {
    ssize_t off = 0;
    while (off < n) {
      ssize_t w = write(out_fd, buf + off, (size_t)(n - off));
      if (w < 0) {
        snprintf(err, errsz, "write dest failed: %s", strerror(errno));
        close(in_fd);
        close(out_fd);
        return -1;
      }
      off += w;
    }
  }

  if (n < 0) {
    snprintf(err, errsz, "read source failed: %s", strerror(errno));
    close(in_fd);
    close(out_fd);
    return -1;
  }

  if (fsync(out_fd) != 0 && errno != EINVAL) {
    snprintf(err, errsz, "fsync dest failed: %s", strerror(errno));
    close(in_fd);
    close(out_fd);
    return -1;
  }

  close(in_fd);
  close(out_fd);
  return 0;
}

static int sysfs_trigger(const char *leaf, char *err, size_t errsz) {
  char path[512];
  int fd;
  size_t len = strlen(wmac_dev);
  ssize_t n;

  if (path_join(path, sizeof(path), driver_dir, leaf) != 0) {
    snprintf(err, errsz, "sysfs path too long");
    return -1;
  }

  fd = open(path, O_WRONLY);
  if (fd < 0) {
    snprintf(err, errsz, "open %s failed: %s", path, strerror(errno));
    return -1;
  }

  n = write(fd, wmac_dev, len);
  close(fd);

  if (n != (ssize_t)len) {
    snprintf(err, errsz, "write %s failed: wrote %ld of %lu", path, (long)n, (unsigned long)len);
    return -1;
  }

  if (!quiet) fprintf(stderr, "rfeye-wmac-rebind: wrote %s to %s\n", wmac_dev, path);
  return 0;
}

static void print_status_json(void) {
  char bind_path[512], unbind_path[512], bound_path[512];
  path_join(bind_path, sizeof(bind_path), driver_dir, "bind");
  path_join(unbind_path, sizeof(unbind_path), driver_dir, "unbind");
  path_join(bound_path, sizeof(bound_path), driver_dir, wmac_dev);

  printf("{\"ok\":true");
  printf(",\"device\":\""); json_escape(stdout, wmac_dev); printf("\"");
  printf(",\"driver_dir\":\""); json_escape(stdout, driver_dir); printf("\"");
  printf(",\"bind_writable\":%s", writable_path(bind_path) ? "true" : "false");
  printf(",\"unbind_writable\":%s", writable_path(unbind_path) ? "true" : "false");
  printf(",\"bound\":%s", exists_path(bound_path) ? "true" : "false");
  printf(",\"source_exists\":%s", exists_path(src_path) ? "true" : "false");
  printf(",\"dest_exists\":%s", exists_path(dst_path) ? "true" : "false");
  printf("}\n");
}

static int print_result(int ok, const char *action, const char *err) {
  printf("{\"ok\":%s,\"action\":\"", ok ? "true" : "false");
  json_escape(stdout, action);
  printf("\"");
  if (err && *err) {
    printf(",\"error\":\"");
    json_escape(stdout, err);
    printf("\"");
  }
  printf(",\"device\":\""); json_escape(stdout, wmac_dev); printf("\"");
  printf(",\"dest\":\""); json_escape(stdout, dst_path); printf("\"");
  printf("}\n");
  return ok ? 0 : 1;
}

int main(int argc, char **argv) {
  char err[256] = {0};

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--status") == 0) do_status = 1;
    else if (strcmp(argv[i], "--install") == 0) do_install = 1;
    else if (strcmp(argv[i], "--unbind") == 0) do_unbind = 1;
    else if (strcmp(argv[i], "--bind") == 0) do_bind = 1;
    else if (strcmp(argv[i], "--rebind") == 0) { do_unbind = 1; do_bind = 1; }
    else if (strcmp(argv[i], "--provision") == 0) { do_install = 1; do_unbind = 1; do_bind = 1; }
    else if (strcmp(argv[i], "--quiet") == 0) quiet = 1;
    else if (strcmp(argv[i], "--source") == 0 && i + 1 < argc) src_path = argv[++i];
    else if (strcmp(argv[i], "--dest") == 0 && i + 1 < argc) dst_path = argv[++i];
    else if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) wmac_dev = argv[++i];
    else if (strcmp(argv[i], "--driver-dir") == 0 && i + 1 < argc) driver_dir = argv[++i];
    else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) { usage(argv[0]); return 0; }
    else { usage(argv[0]); return 2; }
  }

  if (!do_status && !do_install && !do_unbind && !do_bind) do_status = 1;

  if (do_status && !do_install && !do_unbind && !do_bind) {
    print_status_json();
    return 0;
  }

  if (do_unbind) {
    if (sysfs_trigger("unbind", err, sizeof(err)) != 0) {
      /* Continue through provisioning if the WMAC simply was not bound yet. */
      if (!quiet) fprintf(stderr, "rfeye-wmac-rebind: unbind warning: %s\n", err);
      err[0] = '\0';
    }
    usleep(100000);
  }

  if (do_install) {
    if (copy_file_checked(src_path, dst_path, err, sizeof(err)) != 0) {
      return print_result(0, "install", err);
    }
  }

  if (do_bind) {
    if (sysfs_trigger("bind", err, sizeof(err)) != 0) {
      return print_result(0, "bind", err);
    }
  }

  if (do_install && do_bind) return print_result(1, "provision", NULL);
  if (do_install) return print_result(1, "install", NULL);
  if (do_unbind && do_bind) return print_result(1, "rebind", NULL);
  if (do_bind) return print_result(1, "bind", NULL);
  if (do_unbind) return print_result(1, "unbind", NULL);

  print_status_json();
  return 0;
}
