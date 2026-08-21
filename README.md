# Farm-to-Mandi Journey Logger

A chain-of-custody record for every produce batch, for the LEAP Hackathon
AgriTech & Rural Supply Chains track.

When produce arrives spoiled at an APMC mandi there is no evidence of where in
the journey it went wrong, so the farmer has no recourse and no trader is
accountable. This box rides with the consignment and produces that evidence: a
timestamped CSV of position, temperature, humidity and shock, stamped by an RFID
scan at every handover, with a radio alert to the farm the instant a limit is
crossed and a one-line verdict the farmer can hold up at the mandi gate.

It works standalone, on battery, with no internet, no cloud and no phone.

**Two ESP32 nodes:**

- **Remote** — NEO-6M GPS, DHT22, MPU6050, RC522 RFID, microSD, three status
  LEDs, two buttons. Picks a crop from a preset, then logs every two minutes
  until power-off.
- **Base** — SSD1306 OLED at the farm. Shows alerts as they happen and the
  end-of-journey summary.

## Quick start

```powershell
pio run                                  # builds both firmwares
$env:WOKWI_CLI_TOKEN = "..."             # free, https://wokwi.com/dashboard/ci
python tools/lora_bridge.py              # runs both simulations, linked
```

Full demo script, wiring, crop presets, constraint compliance and the
simulated-vs-real breakdown: **[docs/DEMO.md](docs/DEMO.md)**.
