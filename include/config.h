#pragma once

// =============================================================================
// Pin assignments — matches spec section 4 exactly.
// Avoids ESP32 strapping pins, flash pins (6-11), and input-only pins (34-39).
// =============================================================================

// DFPlayer Mini UART (Serial2)
#define PIN_DFPLAYER_RX     16   // ESP32 RX2 ← DFPlayer TX (direct)
#define PIN_DFPLAYER_TX     17   // ESP32 TX2 → DFPlayer RX (through 1kΩ series resistor!)
#define PIN_DFPLAYER_BUSY    4   // LOW while playing, HIGH when idle

// OLED SH1106 I2C (ESP32 hardware I2C defaults)
#define PIN_OLED_SDA        21
#define PIN_OLED_SCL        22

// Rotary encoder (KY-040 style)
#define PIN_ENC_CLK         25   // Channel A
#define PIN_ENC_DT          26   // Channel B
#define PIN_ENC_SW          27   // Push button, active LOW

// Track navigation buttons, active LOW (other leg to GND)
#define PIN_BTN_NEXT        32
#define PIN_BTN_PREV        33

// =============================================================================
// Player constants
// =============================================================================

#define DFPLAYER_BOOT_DELAY_MS  2000   // Wait after power-up before init (spec §11 gotcha 4)
#define DFPLAYER_INIT_RETRIES      5   // How many times to retry player.begin() on failure

#define VOL_DEFAULT         15   // Safe starting volume (out of 30)
#define VOL_MIN              0
#define VOL_MAX             30

// =============================================================================
// Animation constants
// =============================================================================

#define ANIMATION_SPEED_MS  300   // Frame switch speed for Claude character (ms)

// =============================================================================
// Input constants
// =============================================================================

#define DEBOUNCE_MS         30   // Software debounce window for mechanical buttons
#define LONG_PRESS_MS     1000   // Hold duration for encoder long-press (EQ cycle)
