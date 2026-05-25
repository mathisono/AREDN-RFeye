# RFeye IPK Build Notes

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
