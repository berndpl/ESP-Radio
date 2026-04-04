// Direct speaker test — no amplifier needed
// Connect speaker directly: GPIO4 (D3) → Speaker + , GND → Speaker -
// Generates a square wave by toggling the pin.
// Sound will be VERY quiet but audible if the speaker works.
// Hold the speaker close to your ear.

#include <Arduino.h>

#define SPEAKER_PIN  4   // D3 = GPIO4 (pick a free pin)
#define TONE_HZ    1000  // 1kHz — easier to hear than 440Hz at low volume

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== Direct Speaker Test (no amplifier) ===");
    Serial.println("Connect speaker: XIAO D3 (GPIO4) → Speaker +");
    Serial.println("                 XIAO GND         → Speaker -");
    Serial.println("Playing 1kHz square wave — hold speaker to your ear!");
    Serial.println("Sound will be very quiet without an amplifier.\n");

    pinMode(SPEAKER_PIN, OUTPUT);
}

void loop() {
    // Square wave: toggle pin at the desired frequency
    // Period = 1/freq, half-period for each HIGH/LOW
    digitalWrite(SPEAKER_PIN, HIGH);
    delayMicroseconds(500000 / TONE_HZ);  // half period
    digitalWrite(SPEAKER_PIN, LOW);
    delayMicroseconds(500000 / TONE_HZ);

    // Heartbeat every 2 seconds
    static unsigned long lastPrint = 0;
    static unsigned long cycles = 0;
    cycles++;
    if (cycles % (TONE_HZ * 2) == 0 && millis() - lastPrint > 2000) {
        lastPrint = millis();
        Serial.printf("[%lus] Tone playing on GPIO%d...\n", millis() / 1000, SPEAKER_PIN);
    }
}
