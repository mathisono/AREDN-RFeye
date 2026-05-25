# RFeye r11 Test Results — KJ6DZB-WSB-ACdish5

Date: 2026-05-25 16:09 PDT

## Package

- Commit under test: `3f34455` (`rfeye: improve node GUI layout and heatmap scaling`)
- Version: `aredn-rfeye_0.1.0-r11_mips_24kc.ipk`
- SHA256: `38c8fff8b508dedb731665d23b57d340dbae5a0550b070cd1854acd725cf5aac`

## Step 1 — Access confirmation

### Direct path checks

- `ping -c 3 10.188.138.222` → **100% packet loss**
- `curl -I --max-time 8 http://10.188.138.222:8080/` → **timeout** (`curl: (28) Connection timed out after 8002 milliseconds`)
- `ssh -p 2222 -o ConnectTimeout=8 root@10.188.138.222 'hostname; uptime'` → **timeout** (`ssh: connect to host 10.188.138.222 port 2222: Connection timed out`)

### Jump-host path checks (MSE-88)

- `ssh -o ConnectTimeout=8 root@192.168.3.88 ...` → **auth failure**
  - `root@192.168.3.88: Permission denied (publickey,password).`

## Step 2+ execution status

Blocked due to connectivity/authentication failure before node login.

- Install result: **NOT RUN**
- Package version check on node: **NOT RUN**
- CLI sanity result: **NOT RUN**
- 5-minute backend run result: **NOT RUN**
- CGI result: **NOT RUN**
- GUI scroll result: **NOT RUN**
- Compact top-card result: **NOT RUN**
- Waveform result: **NOT RUN**
- Waterfall result: **NOT RUN**
- Ambient result: **NOT RUN**
- Scaling controls result: **NOT RUN**
- GUI 5-minute run result: **NOT RUN**
- Frames captured: **N/A**
- Waterfall rows: **N/A**
- Storage usage: **N/A**
- Final `spectral_scan_ctl` state: **N/A**
- Screenshots: **None**

## Safety observations

No node actions were executed. No channel/settings changes were attempted.

## Result

**PARTIAL (BLOCKED: ACCESS)**

Reason: live acceptance could not be run from this environment due direct node timeout and jump-host authentication failure.

## Next recommended fix

1. Restore verified access path to bench node (either direct route to `10.188.138.222:2222` or working credentials/key for `root@192.168.3.88`).
2. Re-run full Step 2–Step 7 acceptance sequence immediately after connectivity is restored.
3. Update this report with actual install, CLI, backend, CGI, GUI, and final safety-state results.
