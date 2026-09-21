# TRX300 Golden Fixture

This directory is reserved for the Honda FourTrax TRX300 calibration fixture.

The fixture must not encode TRX300-specific algorithmic behavior.

Expected contents:

```text
source/
  original-page.png

config/
  extraction.yaml

regions/
  non-wire-regions.json

expected/
  wire-inventory.json
  topology-invariants.json

review/
  reference-overlay.png
```

The fixture becomes the first regression corpus once the source image is placed
here and the expected inventory is authored.
