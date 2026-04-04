// Test tone generator — no Wi-Fi, no streaming
// Plays a 440 Hz sine wave through I2S → MAX98357A → Speaker
// If you hear the tone, your hardware is wired correctly.

#include <Arduino.h>
#include <driver/i2s.h>
#include <math.h>

// Same I2S pins as the radio firmware
#define I2S_BCLK_PIN   3   // D2
#define I2S_LRC_PIN    1   // D0
#define I2S_DOUT_PIN   2   // D1

#define SAMPLE_RATE    44100
#define TONE_FREQ      440    // A4 note
#define AMPLITUDE      10000  // Volume (max ~32767)
#define I2S_PORT       I2S_NUM_0

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== Speaker Test Tone ===");
    Serial.println("Playing 440 Hz sine wave...");
    Serial.println("If you hear a tone, your wiring is correct!");
    Serial.println("If silent, check: SD pin (must be unconnected), VIN, GND, speaker wires.\n");

    // Configure I2S
    i2s_config_t i2s_config = {
        .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate = SAMPLE_RATE,
        .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags = 0,
        .dma_buf_count = 8,
        .dma_buf_len = 64,
        .use_apll = false,
        .tx_desc_auto_clear = true,
    };

    i2s_pin_config_t pin_config = {
        .bck_io_num = I2S_BCLK_PIN,
        .ws_io_num = I2S_LRC_PIN,
        .data_out_num = I2S_DOUT_PIN,
        .data_in_num = I2S_PIN_NO_CHANGE,
    };

    i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_PORT, &pin_config);
    i2s_zero_dma_buffer(I2S_PORT);

    Serial.println("I2S initialized. Generating tone...");
}

void loop() {
    static float phase = 0.0;
    const float phase_increment = 2.0 * M_PI * TONE_FREQ / SAMPLE_RATE;

    int16_t samples[128];  // 64 stereo samples (L+R interleaved)

    for (int i = 0; i < 128; i += 2) {
        int16_t sample = (int16_t)(AMPLITUDE * sinf(phase));
        samples[i]     = sample;  // Left
        samples[i + 1] = sample;  // Right
        phase += phase_increment;
        if (phase >= 2.0 * M_PI) phase -= 2.0 * M_PI;
    }

    size_t bytes_written;
    i2s_write(I2S_PORT, samples, sizeof(samples), &bytes_written, portMAX_DELAY);

    // Print a heartbeat every 2 seconds
    static unsigned long lastPrint = 0;
    if (millis() - lastPrint > 2000) {
        lastPrint = millis();
        Serial.printf("[%lus] Tone playing...\n", millis() / 1000);
    }
}
