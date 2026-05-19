# RFeye Testing README

Use this page as the stable testing link for early RFeye bench-node tests.

## Current test package

The first testable OpenWrt/AREDN package artifact is committed under:

```text
artifacts/ipk/aredn-rfeye_0.1.0-r1_mips_24kc.ipk
```

Direct download link:

- [aredn-rfeye_0.1.0-r1_mips_24kc.ipk](../artifacts/ipk/aredn-rfeye_0.1.0-r1_mips_24kc.ipk)

Build details and warnings are recorded in:

```text
artifacts/ipk/BUILD_NOTES.md
```

## Detailed test guide

For full install and bench-node test steps, see:

- [Build and Node Test Instructions](BUILD_AND_NODE_TEST.md)
- [Node Test Report Template](NODE_TEST_REPORT_TEMPLATE.md)

## Quick bench-node test

Copy the package to a bench node:

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
/usr/sbin/rfeye-agent snapshot
/usr/sbin/rfeye-survey survey
/usr/sbin/rfeye-survey utilization
```

GUI test:

```text
http://localnode.local.mesh/cgi-bin/apps/rfeye/user
```

## Please report

- Node model and AREDN version
- Install result
- Output from `status`, `snapshot`, `survey`, and `utilization`
- Whether the GUI loaded
- Any errors or warnings

## Safety notes

- Bench-node testing only for first installs.
- No automatic channel hopping.
- Captures stay in `/tmp`.
- Runtime and capture size are capped by the node-side scripts.
- Unsupported hardware should return clear JSON errors instead of crashing.
