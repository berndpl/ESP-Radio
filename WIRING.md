# Wiring Guide — XIAO ESP32-S3 Deutschlandfunk Radio

## Overview

```
[On/Off Switch] → USB-C → [XIAO ESP32-S3] → I2S → [MAX98357A] → [Speaker 8 ohm]
                                 ↑
                            [Vol+] [Vol-]
```

## Components

| Part                         | Role                     |
|------------------------------|--------------------------|
| Seeed XIAO ESP32-S3          | Microcontroller + Wi-Fi  |
| MAX98357A breakout           | I2S digital amplifier    |
| Dayton Audio PC83-8          | 8 ohm full-range speaker |
| 2x mechanical keyboard switch| Volume up / down         |
| On/off toggle switch         | Power cut on USB 5V      |
| Breadboard + jumper wires    | Connections              |

---

## XIAO ESP32-S3 Pinout Reference

```
           ┌─────────┐
      D0  ─┤ 1    14 ├─  5V
      D1  ─┤ 2    13 ├─  GND
      D2  ─┤ 3    12 ├─  3V3
      D3  ─┤ 4    11 ├─  D10
      D4  ─┤ 5    10 ├─  D9
      D5  ─┤ 6     9 ├─  D8
      D6  ─┤ 7     8 ├─  D7
           └─────────┘
             USB-C

  Label → GPIO mapping:
  D0=GPIO1   D1=GPIO2   D2=GPIO3   D3=GPIO4
  D4=GPIO5   D5=GPIO6   D6=GPIO43  D7=GPIO44
  D8=GPIO7   D9=GPIO8   D10=GPIO9
```

---

## 1. MAX98357A → XIAO ESP32-S3 (I2S Audio)

Pin order on the Adafruit MAX98357A breakout (top to bottom, left side):

```
MAX98357A             XIAO ESP32-S3
(Adafruit board)      connection
─────────────────────────────────────────────────────
Pin 1:  LRC      →    D0            GPIO1   Word select
Pin 2:  BCLK     →    D2            GPIO3   Bit clock
Pin 3:  DIN      →    D1            GPIO2   Audio data
Pin 4:  GAIN     →    (unconnected) —       9 dB default
Pin 5:  SD       →    (unconnected) —       Internal pull-up (amp on)
Pin 6:  GND      →    GND           —       Common ground
Pin 7:  VIN      →    5V            —       Power from USB 5V

Right side: screw terminal for speaker (OUT+ / OUT-)
```

**IMPORTANT:** VIN and GND are at the BOTTOM of this board, not the top!

### GAIN pin options (for later tuning)

| GAIN wired to   | Gain  | When to use                   |
|------------------|-------|-------------------------------|
| Unconnected      | 9 dB  | Start here — safe default     |
| GND              | 12 dB | If you want more volume       |
| VIN (5V)         | 15 dB | Maximum — watch for heat      |

---

## 2. Speaker → MAX98357A

```
MAX98357A    →    Speaker (Dayton Audio PC83-8)
──────────────────────────────────────────────
OUT+         →    Speaker + (red)
OUT-         →    Speaker - (black)
```

No resistors needed. The MAX98357A drives the speaker directly.
At 8 ohm it delivers ~2.5W — well within the speaker's 10W rating.

---

## 3. Volume Buttons → XIAO ESP32-S3

Each Cherry MX-style switch has 2 pins. Wire one pin to a GPIO, the other to GND.
The code enables internal pull-up resistors — **no external resistors needed**.

```
Volume UP switch:       Volume DOWN switch:
  Pin 1 → D4 (GPIO5)     Pin 1 → D5 (GPIO6)
  Pin 2 → GND             Pin 2 → GND
```

How it works: pin reads HIGH normally (pulled up internally).
Pressing the switch connects pin to GND → reads LOW → volume changes.

---

## 4. On/Off Switch

The toggle switch cuts USB power before it reaches the XIAO.

**Recommended approach:** cut the red (5V) wire inside a USB-C cable
and solder the switch inline. When OFF, no power flows — the radio
is completely off.

```
USB Power Source → [On/Off Switch] → USB-C → XIAO ESP32-S3
                   (cuts 5V line)
```

---

## 5. Complete Breadboard Wiring

```
    XIAO ESP32-S3 (plugged into breadboard)
    ════════════════════════════════════════════════════

    XIAO 5V   ────────────────────→  MAX98357A VIN
    XIAO GND  ────────────────────→  MAX98357A GND
    XIAO D2   ────────────────────→  MAX98357A BCLK
    XIAO D0   ────────────────────→  MAX98357A LRC
    XIAO D1   ────────────────────→  MAX98357A DIN

    XIAO D4   ───── [Vol UP switch pin 1]
    GND rail  ───── [Vol UP switch pin 2]

    XIAO D5   ───── [Vol DOWN switch pin 1]
    GND rail  ───── [Vol DOWN switch pin 2]

    MAX98357A OUT+  ──→  Speaker +
    MAX98357A OUT-  ──→  Speaker -

    ════════════════════════════════════════════════════
```

### Breadboard layout suggestion

```
    GND rail (blue)  ─────────────────────────────────
    5V rail (red)    ─────────────────────────────────

    ┌──── XIAO ESP32-S3 ────┐
    │ D0  ─── wire ──→ LRC  │   MAX98357A
    │ D1  ─── wire ──→ DIN  │   (also on breadboard,
    │ D2  ─── wire ──→ BCLK │    right side)
    │ D4  ─── [Vol+] ── GND │
    │ D5  ─── [Vol-] ── GND │
    │ 5V  ─── wire ──→ VIN  │
    │ GND ─── wire ──→ GND  │
    └───────────────────────┘
                               MAX98357A OUT+ ──→ Speaker +
                               MAX98357A OUT- ──→ Speaker -
```

---

## 6. Power Budget

| Component        | Current draw      |
|------------------|-------------------|
| XIAO ESP32-S3    | ~200 mA (Wi-Fi)  |
| MAX98357A @ 8 ohm | ~300 mA peak     |
| **Total**        | **~500 mA peak** |

Well within USB-C spec (up to 900 mA from USB 3.x port).

---

## Troubleshooting

| Symptom                   | Check                                            |
|---------------------------|--------------------------------------------------|
| No sound                  | Serial monitor — is stream connecting?           |
| Crackling / distortion    | Shorter wires to MAX98357A, solid GND connection |
| Very quiet                | Bridge GAIN to GND for +12 dB                   |
| Buttons not working       | Verify: one switch pin to GPIO, other to GND     |
| Wi-Fi won't connect       | Check SSID/password in serial monitor            |
| ESP32 rebooting           | Try different USB cable/port (power issue)        |
| No serial output          | Ensure USB CDC enabled in platformio.ini         |
