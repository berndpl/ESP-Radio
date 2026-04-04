// Deutschlandfunk Internet Radio
// XIAO ESP32-S3 + MAX98357A + Dayton Audio PC83-8 (8 ohm)
//
// Turns on → connects Wi-Fi → plays Deutschlandfunk.
// Two mechanical keyboard switches for volume up/down.
// On/off switch cuts power entirely.
//
// Wiring: see WIRING.md

#include <Arduino.h>
#include <WiFi.h>
#include <Audio.h>

// ── Wi-Fi ───────────────────────────────────────────────────────────
const char* ssid       = "MokuMoku";
const char* password   = "h1tz31mp4rk1nb3rl1n";
const char* stream_url = "https://st02.sslstream.dlf.de/dlf/02/128/mp3/stream.mp3?aggregator=web";

// ── I2S pins (XIAO ESP32-S3 → MAX98357A) ───────────────────────────
// XIAO pin labels:  D0=GPIO1, D1=GPIO2, D2=GPIO3, D3=GPIO4,
//                   D4=GPIO5, D5=GPIO6, D6=GPIO43, D7=GPIO44
//                   D8=GPIO7, D9=GPIO8, D10=GPIO9
#define I2S_LRC_PIN    1   // D0 → LRC  (word select)
#define I2S_DOUT_PIN   2   // D1 → DIN  (audio data)
#define I2S_BCLK_PIN   3   // D2 → BCLK (bit clock)

// ── Volume button pins (active LOW, internal pull-up) ───────────────
#define BTN_VOL_UP_PIN    5   // D4 → Volume UP switch
#define BTN_VOL_DOWN_PIN  6   // D5 → Volume DOWN switch

// ── Volume ──────────────────────────────────────────────────────────
#define VOL_MIN      0
#define VOL_MAX      21
#define VOL_DEFAULT  21

// ── Debounce ────────────────────────────────────────────────────────
#define DEBOUNCE_MS  200

// ── State machine ───────────────────────────────────────────────────
enum RadioState {
    STATE_WIFI_CONNECTING,
    STATE_PLAYING
};

// ── Globals ─────────────────────────────────────────────────────────
static Audio audio;
static RadioState state = STATE_WIFI_CONNECTING;
static int  currentVolume    = VOL_DEFAULT;
static unsigned long lastBtnUp   = 0;
static unsigned long lastBtnDown = 0;
static unsigned long lastWifiCheck = 0;

// ── Button handling ─────────────────────────────────────────────────
void handleButtons() {
    unsigned long now = millis();

    if (digitalRead(BTN_VOL_UP_PIN) == LOW && (now - lastBtnUp) > DEBOUNCE_MS) {
        lastBtnUp = now;
        currentVolume = min(VOL_MAX, currentVolume + 1);
        audio.setVolume(currentVolume);
        Serial.printf("Volume UP: %d/%d\n", currentVolume, VOL_MAX);
    }

    if (digitalRead(BTN_VOL_DOWN_PIN) == LOW && (now - lastBtnDown) > DEBOUNCE_MS) {
        lastBtnDown = now;
        currentVolume = max(VOL_MIN, currentVolume - 1);
        audio.setVolume(currentVolume);
        Serial.printf("Volume DOWN: %d/%d\n", currentVolume, VOL_MAX);
    }
}

// ── Audio library callbacks ─────────────────────────────────────────
void audio_info(const char* info) {
    Serial.printf("Audio: %s\n", info);
}

void audio_showstreamtitle(const char* info) {
    Serial.printf("Now playing: %s\n", info);
}

void audio_eof_stream(const char* info) {
    Serial.printf("Stream ended: %s — will reconnect\n", info);
}

// ── Setup ───────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);  // USB CDC init time
    Serial.println("\n=== Deutschlandfunk Radio ===");

    // Buttons — internal pull-up, switch connects to GND
    pinMode(BTN_VOL_UP_PIN,   INPUT_PULLUP);
    pinMode(BTN_VOL_DOWN_PIN, INPUT_PULLUP);

    // Audio hardware init
    audio.setPinout(I2S_BCLK_PIN, I2S_LRC_PIN, I2S_DOUT_PIN);
    audio.setVolume(currentVolume);
    Serial.printf("Volume: %d/%d\n", currentVolume, VOL_MAX);

    // Wi-Fi — non-blocking, sleep disabled for stable streaming
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    WiFi.begin(ssid, password);
    Serial.printf("Connecting to %s...\n", ssid);

    state = STATE_WIFI_CONNECTING;
}

// ── Main loop ───────────────────────────────────────────────────────
void loop() {
    handleButtons();

    switch (state) {

    case STATE_WIFI_CONNECTING: {
        unsigned long now = millis();
        if (now - lastWifiCheck < 200) break;  // Check every 200ms
        lastWifiCheck = now;

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            Serial.println("Starting stream...");
            audio.connecttohost(stream_url);
            state = STATE_PLAYING;
        } else {
            // Print a dot every ~2 seconds to show progress
            static int checks = 0;
            if (++checks % 10 == 0) Serial.print(".");
        }
        break;
    }

    case STATE_PLAYING: {
        audio.loop();  // Must be called frequently — feeds the decoder

        // Status + Wi-Fi check every 5 seconds
        unsigned long now = millis();
        static unsigned long lastStatus = 0;
        if (now - lastStatus > 5000) {
            lastStatus = now;
            Serial.printf("[%lus] Playing | Volume: %d/%d\n", now / 1000, currentVolume, VOL_MAX);
        }
        if (now - lastWifiCheck > 2000) {
            lastWifiCheck = now;
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("\nWi-Fi lost — reconnecting...");
                audio.stopSong();
                WiFi.begin(ssid, password);
                state = STATE_WIFI_CONNECTING;
            }
        }
        break;
    }

    }  // switch
}
