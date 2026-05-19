# RFeye IPK Build Notes

- Date/time: 2026-05-19 16:28:34 UTC
- Source repo: https://github.com/mathisono/AREDN-RFeye
- Source commit before local build changes: c956cae Add OpenClaw ipk build task
- Build tree: /home/bill/src/build-sdk/openwrt-sdk-ath79-generic
- Build SDK: OpenWrt 24.10.0 ath79/generic SDK, gcc 13.3.0, musl
- Target architecture: mips_24kc
- Package filename: aredn-rfeye_0.1.0-r1_mips_24kc.ipk
- Package size: 10441 bytes
- SHA256: f775d31bf0589ad738e2d090b89b39d594bf183dd9970ccf9c488a8f863a8cac
- Parser smoke test: passed via `bash scripts/test-parser-smoke.sh`
- Build commands:
  - `rsync -a --delete "/home/bill/src/AREDN-RFeye/package/aredn-rfeye/" "/home/bill/src/build-sdk/openwrt-sdk-ath79-generic/package/aredn-rfeye/"`
  - `make package/aredn-rfeye/clean V=s`
  - `make package/aredn-rfeye/compile V=s`
- Notes/warnings:
  - Local host had mawk only; GNU awk was extracted under `/home/bill/src/local-gawk` and prepended to PATH for the SDK build.
  - The OpenWrt SDK emitted dependency metadata warnings for `iw` and `uhttpd` because the local SDK feed metadata did not include those packages. The built package control still declares `Depends: libc, iw, uhttpd`.
  - Package Makefile was adjusted to skip copying optional `files/www/apps` when that directory is absent; this avoided packaging a missing optional app-assets directory.
