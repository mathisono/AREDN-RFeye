# RFeye r4 Node Test Report — KJ6DZB-WSB-ACdish5

Date: 2026-05-25 (PDT/UTC)

## Node info

- Target host: `10.188.138.222`
- SSH: `root@10.188.138.222 -p 2222`
- Web: `http://10.188.138.222:8080`
- Expected node: `KJ6DZB-WSB-ACdish5`

## Package artifact under test

- File: `artifacts/ipk/aredn-rfeye_0.1.0-r4_mips_24kc.ipk`
- SHA256: `2489b5f37a23ae084770eb9cf11559617fabf218af3948625b8e26734a0aa577`
- Source commit: `511c9ab`

## Install result

- **Not completed** (node unreachable).

## Reachability checks

Command run:

```sh
ssh -p 2222 -o ConnectTimeout=8 -o StrictHostKeyChecking=no root@10.188.138.222 'hostname; uptime'
```

Result:

```text
ssh: connect to host 10.188.138.222 port 2222: Connection timed out
```

HTTP probe run:

```sh
curl -I --max-time 8 http://10.188.138.222:8080/
```

Result:

```text
curl: (28) Connection timed out after 8001 milliseconds
```

Per safety guidance, retries were not hammered after timeout confirmation.

## CLI results

- Not executed on node (unreachable).

## CGI results

- Not executed on node (unreachable).

## GUI observations

- Not observed on node (web endpoint timed out).

## Heatmap behavior

- Node-side runtime behavior not testable in this cycle due reachability failure.

## Repeated start/stop results

- Not executed on node (unreachable).

## spectral_scan_ctl final state

- Not verifiable on node (unreachable).

## Overall status

- **PARTIAL**

## Main blocker

- Bench node/network reachability failure (SSH and HTTP timeout). Node install + r4 runtime retest blocked.
