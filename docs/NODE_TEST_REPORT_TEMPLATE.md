# AREDN RFeye Node Test Report

## Node

- Node model:
- AREDN version:
- OpenWrt kernel:
- Radio chipset:
- Driver:
- Firmware:
- Test date:
- Tester:

## Package

- Package version:
- Install method:
- Install result:

## Support status

Command:

```sh
/usr/sbin/rfeye-agent status
```

Result:

```json
PASTE RESULT
```

## Survey test

Command:

```sh
/usr/sbin/rfeye-survey survey
```

Result:

```json
PASTE RESULT
```

## Utilization test

Command:

```sh
/usr/sbin/rfeye-survey utilization
```

Result:

```json
PASTE RESULT
```

## Snapshot test

Command:

```sh
/usr/sbin/rfeye-agent snapshot
```

Result:

```json
PASTE RESULT
```

## Capture test

Command:

```sh
/usr/sbin/rfeye-agent start 5 128 phy0
sleep 6
/usr/sbin/rfeye-agent capture_status
ls -lh /tmp/rfeye/latest.tlv
```

Result:

```text
PASTE RESULT
```

## Parser test

Command:

```sh
/usr/lib/rfeye/rfeye-spectral-parse --input /tmp/rfeye/latest.tlv --phy phy0 --limit 5 --bins 64
```

Result:

```json
PASTE RESULT
```

## CGI tests

Commands:

```sh
curl 'http://localnode.local.mesh/cgi-bin/apps/rfeye/data/agent.sh?action=status'
curl 'http://localnode.local.mesh/cgi-bin/apps/rfeye/data/agent.sh?action=survey'
curl 'http://localnode.local.mesh/cgi-bin/apps/rfeye/data/agent.sh?action=utilization'
curl 'http://localnode.local.mesh/cgi-bin/apps/rfeye/data/agent.sh?action=snapshot'
curl 'http://localnode.local.mesh/cgi-bin/apps/rfeye/data/agent.sh?action=capture_status'
curl 'http://localnode.local.mesh/cgi-bin/apps/rfeye/data/agent.sh?action=survey_raw'
```

Results:

```json
PASTE RESULT
```

## Mesh stability notes

- Did Wi-Fi remain stable?
- Did links drop?
- Did CPU spike?
- Did memory spike?
- Did capture stop cleanly?
- Did spectral scan disable cleanly?
- Did any command hang?

## Conclusion

- Supported:
- Partially supported:
- Unsupported:
- Next action:
