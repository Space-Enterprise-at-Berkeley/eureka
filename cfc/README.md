# cfc
Firmware for the Common Flight Computer (CFC) and FV avionics for vehicles SAD/LHAD/E3.

Build with PlatformIO from this directory (e.g. `pio run -e flight-computer`). Packet headers are
generated at build time from the spec in [`../proto`](../proto) and included as `"proto/Packet_<Name>.h"`.

`comms/` holds the serial→UDP bridge: `python comms/usb_bridge.py`.
