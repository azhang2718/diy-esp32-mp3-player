# Portable ESP32 MP3 Player — Build Spec for Claude Code

This document is the complete specification for a portable, belt-clip MP3 player
built around an ESP32 and a DFPlayer Mini. Hand this file to Claude Code in VS Code
and ask it to implement the PlatformIO project described here. All pin assignments,
library choices, hardware gotchas, and functional requirements are below.

---

## 1. What we are building

A battery-powered MP3 player that:

- Plays MP3 files stored offline on the DFPlayer Mini's microSD card
- Drives low-impedance (3Ω) wired earbuds through a MAX98306 amplifier
- Shows track number, volume, and play state on a 1.3" SH1106 OLED
- Uses a rotary encoder for volume (rotate) and play/pause (push)
- Uses two pushbuttons for next track and previous track
- Runs from a LiPo battery, charged over USB-C, with a physical power switch

The ESP32 is the controller. It does **not** decode audio — the DFPlayer Mini reads
the SD card and decodes MP3 internally. The ESP32 sends the DFPlayer simple commands
over a serial (UART) link and drives the user interface.

---

## 2. Components and their roles

| Component | Role in the system | Firmware interaction |
|---|---|---|
| ESP32 dev board (HiLetgo ESP-WROOM-32) | Main controller: UI, inputs, sends commands to DFPlayer | This is what we program |
| DFPlayer Mini | MP3 player engine: holds SD card, decodes MP3, outputs analog audio | Controlled over UART (Serial2) |
| MAX98306 | Stereo class-D amplifier: boosts DFPlayer line-out to drive 3Ω earbuds | None — analog amp, fixed gain via jumpers, shutdown pin tied high |
| MT3608 | Boost converter: steps LiPo 3.7V up to 5V for the ESP32 | None — pure hardware |
| TP4056 | LiPo charger: charges battery from USB-C, provides protection | None — pure hardware |
| LiPo battery | Power source, 3.7V nominal | None |
| SH1106 1.3" OLED | Display: track number, volume, play/pause state | Controlled over I2C using U8g2 library |
| Rotary encoder (KY-040 style) | Volume up/down (rotate), play/pause (push) | Read via ESP32Encoder library + GPIO |
| 2× pushbuttons | Next track, previous track | Read via GPIO with internal pull-ups |

The MAX98306, MT3608, and TP4056 are entirely hardware — no code touches them.
The firmware only controls the DFPlayer, OLED, encoder, and buttons.

---

## 3. System architecture

```
USB-C ──▶ TP4056 ──▶ LiPo ──▶ Slide switch ──▶ MT3608 (5V) ──▶ ESP32 VIN
                                                                  │
                            ┌─────────────────────────────────────┤
                            │ UART (Serial2)                       │ I2C / GPIO
                            ▼                                      ▼
                     DFPlayer Mini                          SH1106 OLED
                     (SD card inside)                       Rotary encoder
                            │ analog line out               Next / Prev buttons
                            ▼
                     MAX98306 amp
                            │
                            ▼
                     3.5mm jack ──▶ 3Ω earbuds
```

---

## 4. Pin assignments (ESP32)

Use exactly these pins. They avoid the ESP32's strapping pins and flash pins.

| Function | ESP32 GPIO | Notes |
|---|---|---|
| DFPlayer RX (ESP32 TX2 → DFPlayer RX) | GPIO 17 | **Put a 1kΩ resistor in series on this line** (DFPlayer requirement) |
| DFPlayer TX (ESP32 RX2 ← DFPlayer TX) | GPIO 16 | |
| DFPlayer BUSY | GPIO 4 | Optional. LOW while playing, HIGH when idle. Use to detect track end. |
| OLED SDA | GPIO 21 | I2C data (ESP32 default SDA) |
| OLED SCL | GPIO 22 | I2C clock (ESP32 default SCL) |
| Encoder CLK (A) | GPIO 25 | |
| Encoder DT (B) | GPIO 26 | |
| Encoder SW (push) | GPIO 27 | Active LOW, use INPUT_PULLUP |
| Next button | GPIO 32 | Active LOW, use INPUT_PULLUP, other leg to GND |
| Prev button | GPIO 33 | Active LOW, use INPUT_PULLUP, other leg to GND |

Power and ground: every module's VCC goes to a 3.3V or 5V rail as noted in section 9,
every GND goes to a common ground.

---

## 5. PlatformIO configuration

Create `platformio.ini` in the project root with this content:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed = 921600
lib_deps =
    dfrobot/DFRobotDFPlayerMini@^1.0.6
    olikraus/U8g2@^2.35.30
    madhephaestus/ESP32Encoder@^0.11.7
```

Notes:
- `board = esp32dev` is the correct generic profile for the HiLetgo ESP-WROOM-32 board.
- `U8g2` is the **required** display library for the SH1106 controller. Do not use
  Adafruit_SSD1306 — it does not handle the SH1106's 2-pixel column offset and the
  display will be shifted and clipped.
- If `upload_speed = 921600` causes upload failures, drop it to `460800` or `115200`.

---

## 6. SD card preparation (critical — do this before testing audio)

The DFPlayer Mini is notoriously picky about the SD card. Follow this exactly:

1. Format the microSD card as **FAT32** (FAT16 also works for ≤2GB cards).
2. Create a folder named **`mp3`** in the root of the card.
3. Name your files **`0001.mp3`, `0002.mp3`, `0003.mp3`** … with four-digit zero-padded
   numbers, placed inside the `/mp3` folder.
4. Address tracks in firmware with `player.playMp3Folder(n)` — this plays `/mp3/000n.mp3`
   **by filename**, which is reliable.

Why this matters: many DFPlayer clones play files by their order in the FAT table
(i.e. the order they were copied), **not** by filename. If you drag-copy all files at
once and use `player.play(n)`, tracks will play in a seemingly random order. Using the
`/mp3` folder with `playMp3Folder()` addresses files by name and sidesteps the problem
entirely.

---

## 7. Functional requirements

The firmware must implement the following behavior:

### Startup
- Initialize Serial (115200) for debug output to the PlatformIO monitor.
- Initialize Serial2 at 9600 baud on pins RX=16, TX=17 for the DFPlayer.
- Wait ~2 seconds after power-up before talking to the DFPlayer (it needs time to boot
  and mount the SD card).
- Initialize the DFPlayer; if `player.begin()` fails, print an error and retry.
- Read the total file count with `player.readFileCounts()` and store it.
- Set initial volume to a safe low level (e.g. 15 out of 30).
- Set EQ to NORMAL.
- Initialize the OLED and show a splash/ready screen.

### Playback control
- **Encoder rotate clockwise** → volume up (cap at 30).
- **Encoder rotate counterclockwise** → volume down (floor at 0).
- **Encoder push** → toggle play/pause.
- **Next button** → play next track (wrap from last to first).
- **Prev button** → play previous track (wrap from first to last).
- **Auto-advance**: when a track finishes, automatically play the next track. Detect
  track end either by polling the DFPlayer BUSY pin (GPIO 4) or by reading the
  DFPlayer's `DFPlayerPlayFinished` message from `player.available()` / `player.readType()`.

### Display (SH1106 via U8g2)
Show on the OLED, updated whenever state changes:
- Current track number and total (e.g. `Track 3 / 27`)
- Volume level (e.g. `Vol 15`)
- Play/pause state (e.g. a `>` for playing, `||` for paused)

Keep the display update lightweight — only redraw when something changes, not in a tight loop.

### Optional (implement if straightforward)
- **EQ cycling**: long-press the encoder (hold ≥1s) to cycle through the DFPlayer's six
  EQ presets (NORMAL, POP, ROCK, JAZZ, CLASSIC, BASS) and show the current preset on the OLED.

### Input handling requirements
- Debounce both pushbuttons (software debounce, ~30ms) — they are mechanical and will bounce.
- Debounce the encoder push button similarly.
- The encoder rotation is handled by the ESP32Encoder library, which manages quadrature
  decoding in hardware. Read the count and translate deltas into volume steps.
- All buttons use `INPUT_PULLUP`; pressed = LOW.

---

## 8. Suggested project structure

```
mp3player/
├── platformio.ini
├── README.md
├── include/
│   └── config.h          # all pin definitions and constants
└── src/
    └── main.cpp          # main firmware
```

`config.h` should define every pin from section 4 as a named constant, plus constants
for default volume, volume limits, debounce time, and the DFPlayer boot delay. `main.cpp`
contains setup() and loop() and the input/display/player logic. For a project this size a
single main.cpp is fine; split into more files only if it grows.

---

## 9. Hardware wiring reference (for assembly, not firmware)

The firmware author does not need this, but it is included so the spec is self-contained.

### Power
| From | To | Notes |
|---|---|---|
| LiPo (+) | TP4056 B+ | via JST pigtail |
| LiPo (−) | TP4056 B− | |
| TP4056 OUT+ | slide switch → MT3608 IN+ | switch in positive line |
| TP4056 OUT− | MT3608 IN− | |
| MT3608 OUT+ | ESP32 VIN | **set MT3608 trimpot to exactly 5.0V before connecting** |
| MT3608 OUT− | ESP32 GND | |

### DFPlayer Mini
| DFPlayer pin | To | Notes |
|---|---|---|
| VCC | 5V rail (ESP32 VIN / 5V pin) | DFPlayer wants 4–5V |
| GND | common GND | |
| RX | ESP32 GPIO 17 | **through a 1kΩ series resistor** |
| TX | ESP32 GPIO 16 | direct |
| BUSY | ESP32 GPIO 4 | optional |
| DAC_L | MAX98306 INL | left line out |
| DAC_R | MAX98306 INR | right line out |
| DAC_GND / GND | MAX98306 GND | shared analog ground |

### MAX98306 (single-ended for shared-ground 3.5mm jack)
| MAX98306 pin | To | Notes |
|---|---|---|
| VDD | 3.3V or 5V rail | |
| GND | common GND | |
| INL | DFPlayer DAC_L | |
| INR | DFPlayer DAC_R | |
| SD / SHDN | 3.3V via 10kΩ | tie high = always enabled |
| OUTL+ | 3.5mm jack Tip | left |
| OUTR+ | 3.5mm jack Ring | right |
| OUTL− , OUTR− | leave disconnected | not used in single-ended mode |
| (jack Sleeve) | common GND | |

Set the MAX98306 gain jumper to the lowest setting (6 dB) — sensitive 3Ω earbuds need little gain.

### OLED (SH1106)
| OLED pin | To |
|---|---|
| VCC | 3.3V rail |
| GND | common GND |
| SDA | ESP32 GPIO 21 |
| SCL | ESP32 GPIO 22 |

### Rotary encoder
| Encoder pin | To |
|---|---|
| + / VCC | 3.3V rail |
| GND | common GND |
| CLK | ESP32 GPIO 25 |
| DT | ESP32 GPIO 26 |
| SW | ESP32 GPIO 27 |

### Buttons
| Button | To |
|---|---|
| Next: leg 1 / leg 2 | ESP32 GPIO 32 / common GND |
| Prev: leg 1 / leg 2 | ESP32 GPIO 33 / common GND |

---

## 10. Library API quick reference

These are the key calls for each library so the implementation uses the correct names.

### DFRobotDFPlayerMini
```cpp
#include <DFRobotDFPlayerMini.h>
DFRobotDFPlayerMini player;

Serial2.begin(9600, SERIAL_8N1, 16, 17);   // RX2=16, TX2=17
if (!player.begin(Serial2)) { /* handle init failure */ }

player.volume(15);                 // 0–30
player.playMp3Folder(1);           // plays /mp3/0001.mp3 by name (reliable)
player.next();
player.previous();
player.pause();
player.start();                    // resume
player.EQ(DFPLAYER_EQ_NORMAL);     // NORMAL/POP/ROCK/JAZZ/CLASSIC/BASS
int total = player.readFileCounts();

// event handling (e.g. track finished):
if (player.available()) {
    uint8_t type = player.readType();
    int value = player.read();
    // type == DFPlayerPlayFinished  → advance to next track
}
```

### U8g2 (SH1106)
```cpp
#include <U8g2lib.h>
#include <Wire.h>
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

Wire.begin(21, 22);    // SDA, SCL  (defaults, but set explicitly)
u8g2.begin();

u8g2.clearBuffer();
u8g2.setFont(u8g2_font_ncenB08_tr);
u8g2.drawStr(0, 12, "Track 3 / 27");
u8g2.drawStr(0, 28, "Vol 15  >");
u8g2.sendBuffer();
```

### ESP32Encoder
```cpp
#include <ESP32Encoder.h>
ESP32Encoder encoder;

ESP32Encoder::useInternalWeakPullResistors = puType::up;
encoder.attachHalfQuad(25, 26);    // CLK, DT
encoder.clearCount();

long count = encoder.getCount();   // track delta to derive volume steps
```

Encoder push button (GPIO 27) and the two track buttons are plain `digitalRead` with
`INPUT_PULLUP` and software debounce — they are not handled by ESP32Encoder.

---

## 11. Hardware gotchas (read before powering on)

1. **Set the MT3608 output to 5.0V before connecting it to the ESP32.** Power the MT3608
   from the battery, measure its output with a multimeter, and adjust the trimpot to 5.0V.
   An unset MT3608 can output up to ~28V and will destroy the ESP32.
2. **The SH1106 needs U8g2, not Adafruit_SSD1306.** Wrong library = shifted/clipped display.
3. **DFPlayer RX needs a 1kΩ series resistor** from ESP32 GPIO 17. Skipping this causes
   noise on the line and unreliable communication.
4. **Wait ~2 seconds after power-up before talking to the DFPlayer.** It needs time to boot
   and mount the SD card. Calling `player.begin()` too early fails.
5. **SD card files must be in `/mp3` named `0001.mp3` etc.** and addressed with
   `playMp3Folder()`. See section 6.
6. **The MAX98306 is wired single-ended** for the shared-ground headphone jack. Only the
   `+` outputs are used; the `−` outputs are left disconnected.
7. **Do not use ESP32 GPIO 6–11** (connected to internal flash) or GPIO 34–39 as outputs
   (input-only, no pull-ups). The pin map in section 4 already avoids these.

---

## 12. Build and upload

1. Open the project folder in VS Code with the PlatformIO extension installed.
2. Connect the ESP32 over USB. Install the CP2102 or CH340 USB-serial driver if the board
   does not enumerate as a serial port.
3. Build: PlatformIO toolbar → checkmark (Build), or `pio run`.
4. Upload: PlatformIO toolbar → right arrow (Upload), or `pio run -t upload`.
5. Open the serial monitor: PlatformIO toolbar → plug icon, or `pio device monitor`.
6. If upload fails with a connection/timeout error, hold the **BOOT** button on the ESP32
   while upload starts, then release. Some HiLetgo boards need this.

---

## 13. Recommended bring-up sequence (test incrementally)

Do not wire everything and flash the full firmware at once. Build up in stages so faults
are easy to localize. Ask Claude Code to produce a small test sketch for each stage if helpful.

1. **Blink** — verify toolchain, board profile, and upload work (blink GPIO 2 onboard LED).
2. **Serial** — print "hello" to the monitor at 115200, confirm the monitor works.
3. **OLED** — initialize U8g2 SH1106 and draw text. Confirm no offset/clipping.
4. **DFPlayer init** — power the DFPlayer, init Serial2, confirm `player.begin()` succeeds
   and `readFileCounts()` returns your real file count.
5. **Play one track** — `playMp3Folder(1)`, confirm audio out of the MAX98306 into earbuds.
6. **Encoder** — print the encoder count and push state to serial; confirm rotation and press.
7. **Buttons** — print next/prev button presses to serial; confirm debounced single events.
8. **Integrate** — combine into the full player: display + volume + play/pause + skip + auto-advance.

---

## 14. Prompt to give Claude Code

Paste this into Claude Code with this file in your project root:

> Read `ESP32_MP3_Player_Spec.md` in this folder. Set up a PlatformIO project for an
> ESP32 (board `esp32dev`, Arduino framework) implementing the MP3 player described in
> that spec. Create `platformio.ini` with the libraries listed in section 5, a
> `include/config.h` with all pin definitions from section 4, and `src/main.cpp`
> implementing every functional requirement in section 7. Use the exact library APIs
> from section 10 — in particular U8g2 with the SH1106 driver for the display, and
> `playMp3Folder()` for track selection. Follow all the gotchas in section 11. Add code
> comments explaining each section. After writing the code, walk me through the bring-up
> sequence in section 13 starting with the blink test.

---

*End of spec.*
