#!/bin/sh
echo "Content-Type: text/html"
echo
cat <<'HTML'
<!doctype html>
<html>
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <title>RFeye (Admin)</title>
  <style>
    body { font-family: sans-serif; margin: 1rem; }
    pre { background: #111; color: #0f0; padding: .75rem; overflow: auto; }
    button { padding: .4rem .7rem; }
  </style>
</head>
<body>
  <h2>AREDN RFeye (Admin)</h2>
  <p>Run on-node probe command:</p>
  <pre>sh /usr/lib/rfeye/rfeye-probe.sh --capture-bytes 4096 --capture-dir /tmp</pre>
  <p><button id="snap">Fetch snapshot</button></p>
  <pre id="out">Click “Fetch snapshot” to pull JSON from snapshot.sh</pre>

  <script>
    (function () {
      var out = document.getElementById('out');
      document.getElementById('snap').addEventListener('click', function () {
        fetch('/cgi-bin/apps/rfeye/admin/snapshot.sh?phy=phy0&bins=64', { cache: 'no-store' })
          .then(function (r) { return r.text(); })
          .then(function (t) {
            try {
              out.textContent = JSON.stringify(JSON.parse(t), null, 2);
            } catch (_) {
              out.textContent = t;
            }
          })
          .catch(function (e) {
            out.textContent = 'snapshot fetch failed: ' + e;
          });
      });
    })();
  </script>
</body>
</html>
HTML
