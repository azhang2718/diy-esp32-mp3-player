# Pocket MP3 Player

This is a personal project in which I constructed a low-cost, portable MP3 player using an ESP32 and a DFPlayer Mini. The MP3 player uses music files from a microSD card, runs on a rechargeable LiPo battery, and clips to your belt (so that I can performative max while rock climbing LMAO). In total, I think the parts about **$30**.

<!-- Add a photo of the finished build here -->
<!-- ![Finished player](docs/photos/build.jpg) -->

## Why I built this

Basically, I lost my Spotify Premium subscription last month (cuz the person whose family plan I was using decided to canceled it) and I really wasn't trying to spend money on a new subscription (I am broek). Spotify free lowk pmo with all its ads, its esp bad when I'm trying to lock in. And so I was scrolling on reels and I saw that someone did a mini project like this and I thought it was pretty interested. I thought abt it and since I had some free time, I also wanted to try it. It'd be pretty cool if I could use this as replacement for Spotify.

## Features

- Plays MP3 files from a microSD card
- Micro-USB rechargeable (LiPo battery, 1000 mAh)
- 0.97" OLED display showing current track, volume, and play/pause state
- Rotary encoder for volume (rotate) and play/pause (push)
- Two dedicated pushbuttons for next/previous track
- Six built-in EQ presets (normal, pop, rock, jazz, classical, bass)
- Dedicated amplifier capable of driving low-impedance earbuds (the earbuds I used were apparently 3 Ohms which is crazy icl)
- No internet needed AND NO ADS YIPPPIEEE

## Bill of materials

| Part | Purpose |
|---|---|---|
| HiLetgo ESP32 dev board (ESP-WROOM-32) | Microcontroller |
| DFPlayer Mini module | MP3 decoder, SD card slot, DAC |
| microSD card (4–16 GB) | Music storage |
| MAX98306 stereo amp module | Audio amplifier |
| SH1106 1.3" OLED display (I²C) | Display |
| KY-040 rotary encoder | Volume + play/pause |
| Tactile pushbuttons × 2 | Next / previous |
| TP4056 charging module (with DW01 protection) | USB-C charging + battery protection |
| MT3608 boost converter | LiPo 3.7V → 5V |
| LiPo battery, 1000–2000 mAh | Power |
| 3.5mm headphone jack (panel-mount) | Audio output |
| Slide switch (SPST panel-mount) | Power on/off |
| Enclosure (~80 × 50 × 25 mm) | Housing |
| Belt clip | Wearable mount |
| Perfboard, wires, headers, 1kΩ resistor, capacitors, heat shrink, JST pigtail | Assembly |

I used mainly AliExpress to buy these parts cuz its cheap, but Amazon is wayyy faster.

## How it works

The ESP32 doesn't actually decode audio itself. The DFPlayer Mini does that, which has an MP3 decoder, an SD card slot, and a DAC built into the module. The ESP32 talks to the DFPlayer over a UART link and sends it simple commands like "play track 5" or "set volume to 20." The DFPlayer's analog output is then amplified by the MAX98306 before reaching the headphone jack.

This split lets the ESP32 focus entirely on the user interface — reading the encoder and buttons, drawing the OLED — while the DFPlayer handles the file I/O and audio decoding. I mainly just did this cuz I read online that the DFPlayer was way easier to implement than doing it from the ESP32, but I might try to change that in a future iteration cuz Claude Code is op. Who knows. Also DFPlayer was pretty cheap so why not try it.

## AI Generated Chart yippiee
```
USB-C ──▶ TP4056 ──▶ LiPo ──▶ Switch ──▶ MT3608 (5V) ──▶ ESP32 VIN
                                                            │
                          ┌─────────────────────────────────┤
                          │ UART                            │ I²C / GPIO
                          ▼                                 ▼
                   DFPlayer Mini                     SH1106 OLED
                   (SD card inside)                  Rotary encoder
                          │ analog line out          Next / Prev buttons
                          ▼
                   MAX98306 amp
                          │
                          ▼
                   3.5mm jack ──▶ earbuds
```
The rest is AI-generated, maybe I'll read it and edit it sometime if its wrong.

## Wiring

| ESP32 GPIO | Connection | Notes |
|---|---|---|
| GPIO 17 (TX2) | DFPlayer RX | **Through a 1 kΩ series resistor** (DFPlayer noise suppression) |
| GPIO 16 (RX2) | DFPlayer TX | Direct |
| GPIO 4 | DFPlayer BUSY | Optional, used for track-end detection |
| GPIO 21 | OLED SDA | I²C data |
| GPIO 22 | OLED SCL | I²C clock |
| GPIO 25 | Encoder CLK (A) | |
| GPIO 26 | Encoder DT (B) | |
| GPIO 27 | Encoder SW (push) | INPUT_PULLUP |
| GPIO 32 | Next button | INPUT_PULLUP, other leg to GND |
| GPIO 33 | Prev button | INPUT_PULLUP, other leg to GND |
| VIN | MT3608 OUT+ (5V) | **Set MT3608 to 5.0V before connecting** |
| GND | Common ground | |

All modules' VCC pins go to either 3.3V (OLED, encoder) or 5V (DFPlayer, MAX98306), and every GND goes to a common ground star point.

The MAX98306 is wired single-ended for the shared-ground 3.5mm jack: `OUTL+` → Tip, `OUTR+` → Ring, `OUTL−` / `OUTR−` left disconnected, jack Sleeve → common ground.

## Software setup

### Requirements

- [VS Code](https://code.visualstudio.com/) with the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode)
- USB-to-serial driver for the ESP32: install the [Silicon Labs CP210x driver](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers) if your board has a CP2102 chip, or the [WCH CH340 driver](https://www.wch-ic.com/downloads/CH341SER_EXE.html) if it has a CH340

### Libraries

These are pulled in automatically by PlatformIO from `platformio.ini`:

- [DFRobotDFPlayerMini](https://github.com/DFRobot/DFRobotDFPlayerMini) — UART control for the DFPlayer Mini
- [U8g2](https://github.com/olikraus/u8g2) — monochrome display library (SH1106-compatible)
- [ESP32Encoder](https://github.com/madhephaestus/ESP32Encoder) — hardware-accelerated rotary encoder reading

### Project structure

```
.
├── platformio.ini       PlatformIO configuration (board, framework, libraries)
├── README.md            this file
├── include/
│   └── config.h         pin definitions and constants
└── src/
    └── main.cpp         main firmware
```

### Build and upload

1. Clone this repo and open the folder in VS Code
2. PlatformIO will automatically install dependencies on first build
3. Connect the ESP32 over USB
4. Use the PlatformIO toolbar: **Build** (✓), then **Upload** (→)
5. Open the **Serial Monitor** (🔌) at 115200 baud to see debug output

If upload fails with a timeout, hold down the **BOOT** button on the ESP32 dev board while starting the upload, then release.

## Preparing the SD card

The DFPlayer Mini is opinionated about file layout. To make it work reliably:

1. Format the microSD card as **FAT32**
2. Create a folder called **`mp3`** in the root of the card
3. Name each music file with a **four-digit zero-padded number** and the `.mp3` extension, placed inside `/mp3`: `0001.mp3`, `0002.mp3`, `0003.mp3`, and so on

Why the strict naming: many DFPlayer modules play files in the order they were copied to the card (FAT table order), not by filename. Using the `/mp3` folder with `playMp3Folder(n)` addresses files by name and avoids that bug.

## Using it

- **Rotate the encoder** to change volume
- **Push the encoder** to play/pause
- **Press Next or Prev** to skip tracks
- **Hold the encoder** for ≥1 second to cycle EQ presets (normal → pop → rock → jazz → classical → bass)

The display shows the current track number, total tracks, volume level, and play state. When a track finishes, the next one starts automatically; the playlist wraps at both ends.

## Things to watch out for

- **Set the MT3608 boost converter to 5.0 V with a multimeter before connecting it to the ESP32.** Out of the box it can output anywhere from 3 V to 28 V. Wrong setting will destroy the ESP32 in milliseconds.
- **Don't substitute the U8g2 library with Adafruit_SSD1306 for the OLED.** The SH1106 controller has a 2-pixel column offset that Adafruit's library doesn't handle correctly. You'll get a shifted, clipped display.
- **The 1 kΩ resistor on the DFPlayer's RX line is not optional.** The DFPlayer's RX is sensitive to electrical noise generated by its own internal switching activity, and without the resistor you get sporadic command failures.
- **Use earbuds, not big speakers.** The MAX98306 wired single-ended is rated for headphones — running real speakers off it requires the bridged (BTL) configuration with a floating-ground load, which a stereo 3.5 mm jack can't provide.

## Future ideas

- Bluetooth audio output to a car stereo (the ESP32 has the radio hardware, just needs firmware)
- Battery level indicator on the OLED
- Custom 3D-printed enclosure
- Music navigation by folder (playlists / albums)
- Crossfade between tracks
- Sleep timer

## License

Use this for whatever you want LMAO

## Credits

- [DFRobot](https://www.dfrobot.com/) for the DFPlayer Mini library and module
- [Oliver Kraus](https://github.com/olikraus) for the U8g2 display library
- [Madhias / madhephaestus](https://github.com/madhephaestus) for the ESP32Encoder library
- [Espressif](https://www.espressif.com/) for the ESP32 platform and Arduino-ESP32 framework

