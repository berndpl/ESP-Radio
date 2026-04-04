// Wokwi simulation version — tests buttons + state machine
// Wi-Fi and audio are faked since Wokwi can't stream internet radio.
//
// What to test:
//   - Click "Vol +" button → serial shows volume going up
//   - Click "Vol -" button → serial shows volume going down
//   - Volume clamps at 0 and 21
//   - State machine transitions appear in serial output

#include <Arduino.h>

// ── Simulate the same pins as the real build ────────────────────────
#define BTN_VOL_UP_PIN    5   // D4 on XIAO = GPIO5
#define BTN_VOL_DOWN_PIN  6   // D5 on XIAO = GPIO6

#define VOL_MIN      0
#define VOL_MAX      21
#define VOL_DEFAULT  10
#define DEBOUNCE_MS  200

// ── State machine (same as real code) ───────────────────────────────
enum RadioState {
    STATE_WIFI_CONNECTING,
    STATE_PLAYING
};

static RadioState state = STATE_WIFI_CONNECTING;
static int  currentVolume = VOL_DEFAULT;
static unsigned long lastBtnUp   = 0;
static unsigned long lastBtnDown = 0;
static unsigned long stateTimer  = 0;

// ── Button handling (identical to real code) ────────────────────────
void handleButtons() {
    unsigned long now = millis();

    if (digitalRead(BTN_VOL_UP_PIN) == LOW && (now - lastBtnUp) > DEBOUNCE_MS) {
        lastBtnUp = now;
        currentVolume = min(VOL_MAX, currentVolume + 1);
        Serial.printf("Volume UP: %d/%d\n", currentVolume, VOL_MAX);
    }

    if (digitalRead(BTN_VOL_DOWN_PIN) == LOW && (now - lastBtnDown) > DEBOUNCE_MS) {
        lastBtnDown = now;
        currentVolume = max(VOL_MIN, currentVolume - 1);
        Serial.printf("Volume DOWN: %d/%d\n", currentVolume, VOL_MAX);
    }
}

// ── Setup ───────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== Deutschlandfunk Radio (Wokwi Simulation) ===");
    Serial.println("Click Vol+/Vol- buttons to test volume control.\n");

    pinMode(BTN_VOL_UP_PIN,   INPUT_PULLUP);
    pinMode(BTN_VOL_DOWN_PIN, INPUT_PULLUP);

    Serial.printf("Volume: %d/%d\n", currentVolume, VOL_MAX);
    state = STATE_WIFI_CONNECTING;
    stateTimer = millis();
    Serial.println("Connecting to Wi-Fi: MokuMoku...");
}

// ── Main loop ───────────────────────────────────────────────────────
void loop() {
    handleButtons();

    switch (state) {

    case STATE_WIFI_CONNECTING: {
        // Simulate Wi-Fi connecting after 3 seconds
        unsigned long now = millis();
        static int dots = 0;
        if (now - stateTimer > 500 && dots < 6) {
            Serial.print(".");
            dots++;
            stateTimer = now;
        }
        if (now > 3000 && dots >= 6) {
            Serial.println("\nConnected! IP: 192.168.1.42 (simulated)");
            Serial.println("Starting Deutschlandfunk stream...");
            Serial.println("Audio: stream connected (simulated)");
            Serial.printf("Now playing: Deutschlandfunk | Volume: %d/%d\n", currentVolume, VOL_MAX);
            state = STATE_PLAYING;
        }
        break;
    }

    case STATE_PLAYING: {
        // Simulate periodic "now playing" updates
        static unsigned long lastStatus = 0;
        unsigned long now = millis();
        if (now - lastStatus > 10000) {
            lastStatus = now;
            Serial.printf("[%lus] Streaming OK | Volume: %d/%d\n",
                          now / 1000, currentVolume, VOL_MAX);
        }
        break;
    }

    }  // switch
}
