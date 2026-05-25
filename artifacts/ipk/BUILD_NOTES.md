# RFeye IPK Build Notes

## r9 build (2026-05-25 UTC)

- Commit: `841640a` (`rfeye: improve live capture cadence and export diagnostics`)
- Package: `aredn-rfeye 0.1.0-r9`
- IPK filename: `aredn-rfeye_0.1.0-r9_mips_24kc.ipk`
- SHA256: `ec25e05fab06640e96514642391058986681ce533879e73ccad9e130a90f7638`
- Build tree path: `/home/bill/src/build-sdk/openwrt-sdk-ath79-generic`
- Build commands:
  - `rsync -a --delete /home/bill/src/AREDN-RFeye/package/aredn-rfeye/ /home/bill/src/build-sdk/openwrt-sdk-ath79-generic/package/aredn-rfeye/`
  - `rsync -a --delete /home/bill/src/AREDN-RFeye/src/ /home/bill/src/build-sdk/openwrt-sdk-ath79-generic/package/aredn-rfeye/src/`
  - `LD_LIBRARY_PATH=/home/bill/src/local-gawk/lib make package/aredn-rfeye/clean V=s`
  - `LD_LIBRARY_PATH=/home/bill/src/local-gawk/lib make package/aredn-rfeye/compile V=s`
- Validation results before build:
  - `sh -n package/aredn-rfeye/files/usr/sbin/rfeye-agent` ✅
  - `sh -n package/aredn-rfeye/files/usr/sbin/rfeye-survey` ✅
  - `sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/data/agent.sh` ✅
  - `sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/data/download.sh` ✅
  - `sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/user` ✅
  - `cc -Wall -Wextra -o /tmp/rfeye-spectral-parse src/rfeye_spectral_parse.c` ✅
  - `sh scripts/test-parser-smoke.sh` ✅
  - `sh scripts/test-hardware-fixture-probe.sh` ✅
- Build notes:
  - r9 improves live capture cadence/status, preserves the resync parser path, and adds CLI/CGI export diagnostics.
- Node retest: PASS through MSE-88 on KJ6DZB-WSB-ACdish5. `raw_capture_test` emitted 1 frame; `pipeline_test` passed all stages; timed 10-second capture produced 8 frames in final quick retest (`capture_status.frames_captured=8`, `pipeline_status.waterfall_rows=7`); export/download endpoints returned valid data/headers; final `spectral_scan_ctl=disable`.

## r8 build (2026-05-25 UTC)

- Commit: pending local commit
- Package: `aredn-rfeye 0.1.0-r8`
- IPK filename: `aredn-rfeye_0.1.0-r8_mips_24kc.ipk`
- SHA256: `43b6fb8f5728905c206ebc09df8cfb549c1b0ffce5d77eb1aad41e4a43baa0e6`
- Build tree path: `/home/bill/src/build-sdk/openwrt-sdk-ath79-generic`
- Build commands:
  - `rsync -a --delete /home/bill/src/AREDN-RFeye/package/aredn-rfeye/ /home/bill/src/build-sdk/openwrt-sdk-ath79-generic/package/aredn-rfeye/`
  - `rsync -a --delete /home/bill/src/AREDN-RFeye/src/ /home/bill/src/build-sdk/openwrt-sdk-ath79-generic/package/aredn-rfeye/src/`
  - `LD_LIBRARY_PATH=/home/bill/src/local-gawk/lib make package/aredn-rfeye/clean V=s`
  - `LD_LIBRARY_PATH=/home/bill/src/local-gawk/lib make package/aredn-rfeye/compile V=s`
- Validation results before build:
  - `sh -n package/aredn-rfeye/files/usr/sbin/rfeye-agent` ✅
  - `sh -n package/aredn-rfeye/files/usr/sbin/rfeye-survey` ✅
  - `sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/data/agent.sh` ✅
  - `sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/user` ✅
  - `cc -Wall -Wextra -o /tmp/rfeye-spectral-parse src/rfeye_spectral_parse.c` ✅
  - `sh scripts/test-parser-smoke.sh` ✅
  - `sh scripts/test-hardware-fixture-probe.sh` ✅
- Build notes:
  - r8 changes are focused on using the `--resync` parser path in the live capture pipeline and writing pipeline/product diagnostics.

## r7 build (2026-05-24 UTC)

- Commit: pending local commit
- Package: `aredn-rfeye 0.1.0-r7`
- IPK filename: `aredn-rfeye_0.1.0-r7_mips_24kc.ipk`
- SHA256: `6832ffceaf88e0e15d10b2739f6809cb2aaf10cc05973fa2bc04455707e9419d`
- Build tree path: `/home/bill/src/build-sdk/openwrt-sdk-ath79-generic`
- Build commands:
  - `rsync -av --delete "/home/bill/src/AREDN-RFeye/package/aredn-rfeye/" "/home/bill/src/build-sdk/openwrt-sdk-ath79-generic/package/aredn-rfeye/"`
  - `rsync -av --delete "/home/bill/src/AREDN-RFeye/src/" "/home/bill/src/build-sdk/openwrt-sdk-ath79-generic/package/aredn-rfeye/src/"`
  - `LD_LIBRARY_PATH=/home/bill/src/local-gawk/lib make package/aredn-rfeye/clean V=s`
  - `LD_LIBRARY_PATH=/home/bill/src/local-gawk/lib make package/aredn-rfeye/compile V=s`
- Validation results before build:
  - `sh -n package/aredn-rfeye/files/usr/sbin/rfeye-agent` ✅
  - `sh -n package/aredn-rfeye/files/usr/sbin/rfeye-survey` ✅
  - `sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/data/agent.sh` ✅
  - `sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/user` ✅
  - `cc -Wall -Wextra -o /tmp/rfeye-spectral-parse src/rfeye_spectral_parse.c` ✅
  - `sh scripts/test-parser-smoke.sh` ✅
  - `PARSER=/tmp/rfeye-spectral-parse sh scripts/test-hardware-fixture-probe.sh` ✅
- Build notes:
  - `--probe` now scans a bounded 64 KiB window for speed on node hardware.
  - Parser probe/resync JSON remains valid on 262144-byte node captures.

## r6 build (2026-05-24 UTC)

- Commit: pending local commit
- Package: `aredn-rfeye 0.1.0-r6`
- IPK filename: `aredn-rfeye_0.1.0-r6_mips_24kc.ipk`
- SHA256: `4d04b2c722a21bc4a37b96c149e3df3169e348055cd458e10744f2b61f8da51a`
- Build tree path: `/home/bill/src/build-sdk/openwrt-sdk-24.10.0-ath79-generic_gcc-13.3.0_musl.Linux-x86_64`
- Build commands:
  - `rsync -av --delete "/home/bill/src/AREDN-RFeye/package/aredn-rfeye/" "/home/bill/src/build-sdk/openwrt-sdk-24.10.0-ath79-generic_gcc-13.3.0_musl.Linux-x86_64/package/aredn-rfeye/"`
  - `rsync -av --delete "/home/bill/src/AREDN-RFeye/src/" "/home/bill/src/build-sdk/openwrt-sdk-24.10.0-ath79-generic_gcc-13.3.0_musl.Linux-x86_64/package/aredn-rfeye/src/"`
  - `LD_LIBRARY_PATH=/home/bill/src/local-gawk/lib make package/aredn-rfeye/clean V=s`
  - `LD_LIBRARY_PATH=/home/bill/src/local-gawk/lib make package/aredn-rfeye/compile V=s`
- Validation results before build:
  - `sh -n package/aredn-rfeye/files/usr/sbin/rfeye-agent` ✅
  - `sh -n package/aredn-rfeye/files/usr/sbin/rfeye-survey` ✅
  - `sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/data/agent.sh` ✅
  - `sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/user` ✅
  - `cc -Wall -Wextra -o /tmp/rfeye-spectral-parse src/rfeye_spectral_parse.c` ✅
  - `sh scripts/test-parser-smoke.sh` ✅
- Build notes:
  - `rfeye-spectral-parse` in the final package includes `--probe` support.

## r5 build (2026-05-25 UTC)

- Commit: `8fbc87f` (`rfeye: add acquisition diagnostics for r5`)
- Package: `aredn-rfeye 0.1.0-r5`
- IPK filename: `aredn-rfeye_0.1.0-r5_mips_24kc.ipk`
- SHA256: `aa035d679660c027e8d0ea2e69d718feec73514743035a8a09e91633930eb124`
- Build tree path: `/home/bill/src/build-sdk/openwrt-sdk-ath79-generic`
- Build commands:
  - `rsync -av --delete "/home/bill/src/AREDN-RFeye/package/aredn-rfeye/" "/home/bill/src/build-sdk/openwrt-sdk-ath79-generic/package/aredn-rfeye/"`
  - `LD_LIBRARY_PATH=/home/bill/src/local-gawk/lib make package/aredn-rfeye/clean V=s`
  - `LD_LIBRARY_PATH=/home/bill/src/local-gawk/lib make package/aredn-rfeye/compile V=s`
- Validation results before build:
  - `sh -n package/aredn-rfeye/files/usr/sbin/rfeye-agent` ✅
  - `sh -n package/aredn-rfeye/files/usr/sbin/rfeye-survey` ✅
  - `sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/data/agent.sh` ✅
  - `sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/user` ✅
  - `cc -Wall -Wextra -o /tmp/rfeye-spectral-parse src/rfeye_spectral_parse.c` ✅
  - `sh scripts/test-parser-smoke.sh` ✅
- Build issue diagnosed/fixed:
  - SDK host `awk` symlink/runtime path mismatch caused metadata generation problems; using `LD_LIBRARY_PATH=/home/bill/src/local-gawk/lib` for SDK make calls restored build target generation.
- Node retest: performed through jump host MSE-88, but r5 capture loop still returned zero parsed frames; diagnostics endpoints worked.

## r4 build (2026-05-25 UTC)

- Commit: `511c9ab` (`rfeye: add reliable capture loop and heatmap bundle UI`)
- Package: `aredn-rfeye 0.1.0-r4`
- IPK filename: `aredn-rfeye_0.1.0-r4_mips_24kc.ipk`
- SHA256: `2489b5f37a23ae084770eb9cf11559617fabf218af3948625b8e26734a0aa577`
- Build tree path: `/home/bill/src/build-sdk/openwrt-sdk-ath79-generic`
- Build commands:
  - `rsync -av --delete "/home/bill/src/AREDN-RFeye/package/aredn-rfeye/" "/home/bill/src/build-sdk/openwrt-sdk-ath79-generic/package/aredn-rfeye/"`
  - `LD_LIBRARY_PATH=/home/bill/src/local-gawk/lib make package/aredn-rfeye/clean V=s`
  - `LD_LIBRARY_PATH=/home/bill/src/local-gawk/lib make package/aredn-rfeye/compile V=s`
- Validation results before build:
  - `sh -n package/aredn-rfeye/files/usr/sbin/rfeye-agent` ✅
  - `sh -n package/aredn-rfeye/files/usr/sbin/rfeye-survey` ✅
  - `sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/data/agent.sh` ✅
  - `sh -n package/aredn-rfeye/files/www/cgi-bin/apps/rfeye/user` ✅
  - `cc -Wall -Wextra -o /tmp/rfeye-spectral-parse src/rfeye_spectral_parse.c` ✅
  - `sh scripts/test-parser-smoke.sh` ✅
- Build issue diagnosed/fixed:
  - SDK host `awk` symlink was pointing to a local gawk without runtime library path, causing metadata generation failures and then `No rule to make target ...`.
  - Setting `LD_LIBRARY_PATH=/home/bill/src/local-gawk/lib` for SDK make calls restored package metadata scanning and target generation.
- Node retest completed: **Yes (PARTIAL PASS)** via jump host MSE-88 (`192.168.3.88`). Package installed and endpoints validated; captures still produced zero frames in this run.

## Prior artifacts

- `aredn-rfeye_0.1.0-r1_mips_24kc.ipk`
- `aredn-rfeye_0.1.0-r2_mips_24kc.ipk`
- `aredn-rfeye_0.1.0-r3_mips_24kc.ipk`
