# OpenClaw Task: Build and Commit RFeye IPK for Node Testing

## Mission

Build a testable `aredn-rfeye` OpenWrt/AREDN `.ipk`, commit only the package artifact to this repo, and document the exact build target used.

This is for early bench-node testing only. Keep the node side small and safe.

## Guardrails

- Do not commit a full OpenWrt/AREDN build tree.
- Do not commit `bin/`, `build_dir/`, `staging_dir/`, `tmp/`, or toolchains.
- Commit only the final `.ipk` under `artifacts/ipk/` plus a short build note if needed.
- Do not modify radio/channel behavior beyond current-channel/background spectral testing.
- Do not add automatic channel hopping.
- Keep captures in `/tmp` on the node.

## Inputs

Assume these local paths unless the operator overrides them:

```sh
RFeye=$HOME/src/AREDN-RFeye
AREDN=$HOME/src/aredn
```

## Steps

### 1. Run local parser smoke test

```sh
cd "$RFeye"
scripts/test-parser-smoke.sh
```

Stop if this fails.

### 2. Copy package into AREDN/OpenWrt tree

```sh
rsync -av --delete "$RFeye/package/aredn-rfeye/" \
  "$AREDN/package/aredn-rfeye/"
```

### 3. Build only the RFeye package

```sh
cd "$AREDN"
make package/aredn-rfeye/clean V=s
make package/aredn-rfeye/compile V=s
```

### 4. Locate the IPK

```sh
find "$AREDN/bin" -name 'aredn-rfeye*.ipk' -print
```

There should be one current package for the selected target architecture.

### 5. Copy artifact back into repo

```sh
cd "$RFeye"
mkdir -p artifacts/ipk
cp "$(find "$AREDN/bin" -name 'aredn-rfeye*.ipk' | head -n1)" artifacts/ipk/
```

### 6. Record build notes

Create or update:

```text
artifacts/ipk/BUILD_NOTES.md
```

Include:

- Date/time
- AREDN/OpenWrt tree path
- Target profile or architecture if known
- Package filename
- Build command used
- Any warnings/errors

### 7. Commit artifact only

```sh
git status --short
git add artifacts/ipk/*.ipk artifacts/ipk/BUILD_NOTES.md
git commit -m "Add RFeye test IPK artifact"
git push
```

## Bench-node install test

Copy to node:

```sh
scp artifacts/ipk/aredn-rfeye_*.ipk root@localnode.local.mesh:/tmp/
```

Install:

```sh
ssh root@localnode.local.mesh
opkg install /tmp/aredn-rfeye_*.ipk
```

Run checks:

```sh
/usr/sbin/rfeye-agent status
/usr/sbin/rfeye-survey survey
/usr/sbin/rfeye-survey utilization
/usr/sbin/rfeye-agent snapshot
```

Open GUI:

```text
http://localnode.local.mesh/cgi-bin/apps/rfeye/user
```

## Acceptance

- Local parser smoke test passes.
- `.ipk` builds without package errors.
- `.ipk` is committed under `artifacts/ipk/` only.
- Node install succeeds with `opkg install`.
- GUI loads and can start/stop a time-limited data pull.
- `status`, `survey`, `utilization`, and `snapshot` return valid JSON or clear unsupported JSON errors.
