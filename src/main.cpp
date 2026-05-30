// =============================================================================
// Portable ESP32 MP3 Player
// Spec: ESP32_MP3_Player_Spec.md
//
// Hardware: ESP32 + DFPlayer Mini + SH1106 OLED + rotary encoder + 2 buttons
// All pin assignments and constants live in include/config.h.
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <DFRobotDFPlayerMini.h>
#include <U8g2lib.h>
#include <ESP32Encoder.h>
#include "config.h"
#include "claudeframes.h"

// =============================================================================
// Objects
// =============================================================================

DFRobotDFPlayerMini player;

// Full-buffer (F) mode keeps the entire frame in RAM so sendBuffer() does one
// I2C burst. The SH1106 driver handles its 2-pixel column offset correctly.
// U8X8_PIN_NONE = no reset pin wired.
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

ESP32Encoder encoder;

// =============================================================================
// Player state
// =============================================================================

int  currentTrack  = 1;
int  totalTracks   = 0;
int  volume        = VOL_DEFAULT;
bool isPlaying     = false;
bool displayDirty  = true;   // true = OLED needs a redraw

// Animation state
int  animationFrame = 0;
uint32_t lastAnimationMs = 0;

// Track timing state (for progress bar)
uint32_t trackStartMs = 0;
int currentTrackDuration = 0;  // seconds

// Song duration map (read from SD card or defaults)
#define MAX_TRACKS 100
int songDurations[MAX_TRACKS] = {0};  // 0 = unknown/not loaded

// EQ cycling (optional feature, long-press encoder)
const uint8_t EQ_PRESETS[]  = {
    DFPLAYER_EQ_NORMAL,
    DFPLAYER_EQ_POP,
    DFPLAYER_EQ_ROCK,
    DFPLAYER_EQ_JAZZ,
    DFPLAYER_EQ_CLASSIC,
    DFPLAYER_EQ_BASS
};
const char* EQ_NAMES[] = { "NORMAL", "POP", "ROCK", "JAZZ", "CLASSIC", "BASS" };
const int EQ_COUNT = sizeof(EQ_PRESETS);
int eqIndex = 0;

// =============================================================================
// Encoder state
// =============================================================================

long lastEncoderCount = 0;

// =============================================================================
// Button debounce helpers
// Each button tracks its last stable state and the time it last changed.
// =============================================================================

struct Button {
    uint8_t pin;
    bool    lastStable;      // last debounced state (HIGH = released)
    uint32_t lastChangeMs;   // millis() when raw state last changed
    bool     raw;            // raw reading at last sample
};

Button btnNext = { PIN_BTN_NEXT, HIGH, 0, HIGH };
Button btnPrev = { PIN_BTN_PREV, HIGH, 0, HIGH };
Button btnEnc  = { PIN_ENC_SW,   HIGH, 0, HIGH };

// Returns true on the falling edge (HIGH→LOW) after debounce.
bool buttonPressed(Button &b) {
    bool reading = digitalRead(b.pin);
    if (reading != b.raw) {
        b.raw = reading;
        b.lastChangeMs = millis();
    }
    if ((millis() - b.lastChangeMs) > DEBOUNCE_MS) {
        if (reading != b.lastStable) {
            b.lastStable = reading;
            if (reading == LOW) return true;   // falling edge = press
        }
    }
    return false;
}

// =============================================================================
// Encoder long-press detection
// =============================================================================

uint32_t encPressStartMs  = 0;
bool     encLongPressArmed = false;   // becomes false once the long-press fires

// =============================================================================
// Utility helpers
// =============================================================================

// Format seconds to MM:SS string
void formatTime(int seconds, char* buffer, int buflen) {
    int minutes = seconds / 60;
    int secs = seconds % 60;
    snprintf(buffer, buflen, "%d:%02d", minutes, secs);
}

// Get file size in bytes from SD card for current track
// DFPlayer limitation: we estimate via file count; actual file size requires
// direct SD card access which is complex. For 128kbps MP3, duration ≈ filesize / 16000
// Fallback: use 180 seconds if we can't determine
int estimateDurationFromFileSize(int trackNumber) {
    // For now, estimate based on typical MP3 duration (180s = 3 min default)
    // TODO: If you have a way to read file size from SD card, implement here
    // Duration = fileSize / 16000 (for 128kbps MP3)
    return 180;  // Default estimate
}

// Get duration for a track (from config or estimate)
int getTrackDuration(int trackNumber) {
    if (trackNumber < 1 || trackNumber > MAX_TRACKS) return 180;
    if (songDurations[trackNumber - 1] > 0) {
        return songDurations[trackNumber - 1];  // Use loaded duration
    }
    return estimateDurationFromFileSize(trackNumber);  // Estimate fallback
}

// =============================================================================
// Display helpers
// =============================================================================

void updateDisplay() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);

    // --- Row 1: Claude animation + progress bar ---
    if (isPlaying) {
        // Draw Claude running character
        u8g2.drawBitmap(2, 10, 2, 16, CLAUDE_FRAMES[animationFrame]);
    }

    // Draw progress bar background
    u8g2.drawFrame(20, 14, 100, 4);

    // Draw progress bar fill
    if (currentTrackDuration > 0) {
        uint32_t elapsedMs = millis() - trackStartMs;
        int elapsedSecs = elapsedMs / 1000;
        int progress = (elapsedSecs * 100) / currentTrackDuration;
        if (progress > 100) progress = 100;
        if (progress < 0) progress = 0;

        int barWidth = (progress * 98) / 100;  // 98 to fit in 100-wide frame
        if (barWidth > 0) {
            u8g2.drawBox(21, 15, barWidth, 2);
        }
    }

    // --- Row 2: Track info + time display ---
    char line2[48];
    char timeStr[16];
    if (currentTrackDuration > 0) {
        uint32_t elapsedMs = millis() - trackStartMs;
        int elapsedSecs = elapsedMs / 1000;
        if (elapsedSecs > currentTrackDuration) {
            elapsedSecs = currentTrackDuration;
        }
        formatTime(elapsedSecs, timeStr, sizeof(timeStr));
        char durationStr[16];
        formatTime(currentTrackDuration, durationStr, sizeof(durationStr));
        snprintf(line2, sizeof(line2), "Track %d/%d  %s/%s",
                 currentTrack, totalTracks, timeStr, durationStr);
    } else {
        snprintf(line2, sizeof(line2), "Track %d/%d", currentTrack, totalTracks);
    }
    u8g2.drawStr(0, 40, line2);

    // --- Row 3: Volume + EQ ---
    char line3[32];
    snprintf(line3, sizeof(line3), "Vol %d  %s", volume, EQ_NAMES[eqIndex]);
    u8g2.drawStr(0, 54, line3);

    u8g2.sendBuffer();
    displayDirty = false;
}

void showSplash() {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.drawStr(0, 20, "ESP32 MP3 Player");
    u8g2.drawStr(0, 38, "Initializing...");
    u8g2.sendBuffer();
}

// =============================================================================
// DFPlayer init with retry
// =============================================================================

void initDFPlayer() {
    Serial.println("Waiting for DFPlayer boot...");
    delay(DFPLAYER_BOOT_DELAY_MS);   // DFPlayer needs ~2s to mount SD card (spec §11 gotcha 4)

    for (int attempt = 1; attempt <= DFPLAYER_INIT_RETRIES; attempt++) {
        Serial.printf("DFPlayer init attempt %d/%d\n", attempt, DFPLAYER_INIT_RETRIES);
        if (player.begin(Serial2)) {
            Serial.println("DFPlayer OK");
            return;
        }
        Serial.println("DFPlayer init failed, retrying...");
        delay(500);
    }
    Serial.println("ERROR: DFPlayer init failed after all retries. Check wiring and SD card.");
    // Continue rather than hang — display will still work and we can debug over serial.
}

// =============================================================================
// Track navigation helpers
// =============================================================================

void playTrack(int track) {
    player.playMp3Folder(track);
    currentTrack = track;
    isPlaying = true;
    trackStartMs = millis();  // Reset timer for this track
    currentTrackDuration = getTrackDuration(track);
    animationFrame = 0;  // Reset animation
    displayDirty = true;
    Serial.printf("Playing track %d / %d (duration: %d sec)\n", currentTrack, totalTracks, currentTrackDuration);
}

void nextTrack() {
    int next = (currentTrack % totalTracks) + 1;   // wrap last → 1
    playTrack(next);
}

void prevTrack() {
    int prev = (currentTrack - 2 + totalTracks) % totalTracks + 1;   // wrap 1 → last
    playTrack(prev);
}

// =============================================================================
// Song duration loading
// =============================================================================

void loadSongDurations() {
    // Initialize with defaults (180 seconds each)
    for (int i = 0; i < MAX_TRACKS; i++) {
        songDurations[i] = 180;
    }

    // TODO: If SD card reader is available, load from /songconfig.txt
    // Format: one track per line, "tracknum,duration_seconds"
    // Example:
    // 1,240
    // 2,186
    // 3,320
    //
    // For now, using 180s default. User can manually populate songDurations[]
    // by adding estimated durations based on their MP3 bitrates.

    Serial.println("Song durations initialized (using defaults)");
}

// =============================================================================
// setup()
// =============================================================================

void setup() {
    // --- Debug serial ---
    Serial.begin(115200);
    Serial.println("=== ESP32 MP3 Player starting ===");

    // --- DFPlayer UART (Serial2, RX=16, TX=17) ---
    // GPIO 17 must have a 1kΩ series resistor to DFPlayer RX (spec §11 gotcha 3)
    Serial2.begin(9600, SERIAL_8N1, PIN_DFPLAYER_RX, PIN_DFPLAYER_TX);

    // --- DFPlayer BUSY pin ---
    pinMode(PIN_DFPLAYER_BUSY, INPUT);

    // --- I2C + OLED ---
    Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
    u8g2.begin();
    showSplash();

    // --- Encoder ---
    // Use internal weak pull-ups so no external resistors are needed on CLK/DT
    ESP32Encoder::useInternalWeakPullResistors = puType::up;
    encoder.attachHalfQuad(PIN_ENC_CLK, PIN_ENC_DT);
    encoder.clearCount();
    lastEncoderCount = 0;

    // --- Buttons (INPUT_PULLUP; pressed = LOW) ---
    pinMode(PIN_ENC_SW,   INPUT_PULLUP);
    pinMode(PIN_BTN_NEXT, INPUT_PULLUP);
    pinMode(PIN_BTN_PREV, INPUT_PULLUP);

    // --- DFPlayer init ---
    initDFPlayer();

    totalTracks = player.readFileCounts();
    Serial.printf("SD card has %d tracks\n", totalTracks);
    if (totalTracks <= 0) {
        Serial.println("WARNING: readFileCounts() returned 0. Check SD card.");
        totalTracks = 1;   // prevent division-by-zero in wrap math
    }

    loadSongDurations();

    player.volume(VOL_DEFAULT);
    player.EQ(EQ_PRESETS[eqIndex]);

    // Start playback of the first track automatically
    playTrack(1);

    Serial.println("Setup complete.");
}

// =============================================================================
// loop()
// =============================================================================

void loop() {

    // -------------------------------------------------------------------------
    // 1. Encoder rotation → volume
    // -------------------------------------------------------------------------
    long count = encoder.getCount();
    long delta = count - lastEncoderCount;
    if (delta != 0) {
        lastEncoderCount = count;
        volume += (int)delta;
        if (volume > VOL_MAX) volume = VOL_MAX;
        if (volume < VOL_MIN) volume = VOL_MIN;
        player.volume(volume);
        Serial.printf("Volume: %d\n", volume);
        displayDirty = true;
    }

    // -------------------------------------------------------------------------
    // 2. Encoder push → play/pause (short press) or EQ cycle (long press ≥1s)
    // -------------------------------------------------------------------------
    {
        bool reading = digitalRead(PIN_ENC_SW);

        // Track raw state for debounce
        if (reading != btnEnc.raw) {
            btnEnc.raw = reading;
            btnEnc.lastChangeMs = millis();
        }

        if ((millis() - btnEnc.lastChangeMs) > DEBOUNCE_MS) {
            if (reading != btnEnc.lastStable) {
                btnEnc.lastStable = reading;

                if (reading == LOW) {
                    // Button just pressed — start timing for long-press
                    encPressStartMs   = millis();
                    encLongPressArmed = true;
                } else {
                    // Button just released
                    if (encLongPressArmed) {
                        // Short press: toggle play/pause
                        if (isPlaying) {
                            player.pause();
                            isPlaying = false;
                            Serial.println("Paused");
                        } else {
                            player.start();
                            isPlaying = true;
                            Serial.println("Resumed");
                        }
                        displayDirty = true;
                    }
                    encLongPressArmed = false;
                }
            }
        }

        // Long-press detection while button is still held
        if (encLongPressArmed && reading == LOW &&
            (millis() - encPressStartMs) >= LONG_PRESS_MS) {
            encLongPressArmed = false;   // fire once per hold
            eqIndex = (eqIndex + 1) % EQ_COUNT;
            player.EQ(EQ_PRESETS[eqIndex]);
            Serial.printf("EQ: %s\n", EQ_NAMES[eqIndex]);
            displayDirty = true;
        }
    }

    // -------------------------------------------------------------------------
    // 3. Next / Prev buttons → track navigation (with wrap)
    // -------------------------------------------------------------------------
    if (buttonPressed(btnNext)) {
        Serial.println("Next track");
        nextTrack();
    }
    if (buttonPressed(btnPrev)) {
        Serial.println("Prev track");
        prevTrack();
    }

    // -------------------------------------------------------------------------
    // 4. Auto-advance: detect track end via DFPlayer event OR BUSY pin
    // -------------------------------------------------------------------------

    // Method A: parse DFPlayer serial events (preferred)
    if (player.available()) {
        uint8_t type  = player.readType();
        int     value = player.read();
        if (type == DFPlayerPlayFinished) {
            Serial.printf("Track %d finished\n", value);
            nextTrack();
        }
    }

    // Method B: BUSY pin fallback — only use if event method misses endings.
    // BUSY is LOW while playing; we look for a LOW→HIGH edge after isPlaying was set.
    // Uncomment if needed:
    //
    // static bool lastBusy = LOW;
    // bool busyNow = digitalRead(PIN_DFPLAYER_BUSY);
    // if (lastBusy == LOW && busyNow == HIGH && isPlaying) {
    //     Serial.println("BUSY pin: track ended");
    //     nextTrack();
    // }
    // lastBusy = busyNow;

    // -------------------------------------------------------------------------
    // 5. Animation frame cycling (while playing)
    // -------------------------------------------------------------------------
    if (isPlaying) {
        uint32_t now = millis();
        if (now - lastAnimationMs >= ANIMATION_SPEED_MS) {
            lastAnimationMs = now;
            animationFrame = (animationFrame + 1) % CLAUDE_FRAME_COUNT;
            displayDirty = true;
        }
    }

    // -------------------------------------------------------------------------
    // 6. Redraw OLED only when something changed
    // -------------------------------------------------------------------------
    if (displayDirty) {
        updateDisplay();
    }
}
