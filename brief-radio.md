# Project Brief — ESP32 Deutschlandfunk Radio

## Hardware
- **Seeed XIAO ESP32-S3** microcontroller (Wi-Fi, USB-C)
- **MAX98357A** I2S speaker amplifier breakout
- **Dayton Audio PC83-8** full-range speaker (8 ohm)
- 2x mechanical keyboard switches (Cherry MX style) — volume up/down
- 1x on/off toggle switch (inline on USB power)
- Breadboard + jumper wires

## Goal
A simple internet radio. On power-on it connects to Wi-Fi and streams
Deutschlandfunk automatically. Volume adjustable via two physical buttons.

## Interface
- On/off switch — cuts USB power entirely
- Volume up button
- Volume down button

## Software
- PlatformIO CLI (no IDE)
- ESP32-audioI2S library for I2S streaming + MP3 decoding
- Non-blocking Wi-Fi connection with auto-reconnect

## Stream
- Deutschlandfunk 128kbps MP3
- `https://st02.sslstream.dlf.de/dlf/02/128/mp3/stream.mp3?aggregator=web`
