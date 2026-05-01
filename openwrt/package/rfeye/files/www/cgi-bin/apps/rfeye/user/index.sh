#!/bin/sh
echo "Content-Type: text/html"
echo
cat <<'HTML'
<!doctype html>
<html>
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <title>RFeye (User)</title>
  <style>
    body { font-family: sans-serif; margin: 1rem; }
    .panel { border: 1px solid #ccc; padding: .75rem; margin-bottom: .75rem; }
  </style>
</head>
<body>
  <h2>AREDN RFeye (User)</h2>
  <div class="panel">Live FFT canvas (placeholder)</div>
  <div class="panel">Waterfall canvas (placeholder)</div>
  <div class="panel">Channel utilization meter (placeholder)</div>
  <div class="panel">Noise floor trend (placeholder)</div>
  <div class="panel">Interference events list (placeholder)</div>
</body>
</html>
HTML

