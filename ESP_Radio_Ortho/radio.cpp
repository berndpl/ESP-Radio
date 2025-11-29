#include "radio.h"
#include "debug_config.h"
#include <Arduino.h>
#include <WiFi.h>

// Attempt to reduce memory usage of the Audio library by shrinking DMA buffers
// This is necessary because DMA buffers use internal RAM, not PSRAM.
#define I2S_DMA_BUF_COUNT 4
#define I2S_DMA_BUF_LEN 512
#include <Audio.h>

// XIAO_ESP32S3 pin definitions: D0=GPIO1, D1=GPIO2, D2=GPIO3
#define I2S_DOUT  2   // MAX98357 DIN (D1)
#define I2S_BCLK  3   // MAX98357 BCLK (D2)
#define I2S_LRC   1   // MAX98357 LRC (D0)

// --- WiFi & Radio Configuration ---
const char* ssid     = "MokuMoku";
const char* password = "h1tz31mp4rk1nb3rl1n";
const char* stream_url = "https://st01.sslstream.dlf.de/dlf/01/128/mp3/stream.mp3?aggregator=web";

// --- Audio State ---
static Audio audio;
static int currentVolume = 10; // 0-21 for Audio library
static bool isPlaying = true;

// --- Audio library callbacks ---
void audio_info(const char *info){
  Serial.print("info        ");
  Serial.println(info);
}

// --- Public Functions ---

void radio_setup() {
  // Initialize WiFi
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Initialize Audio
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  audio.setVolume(currentVolume);
  Serial.print("Initial volume: ");
  Serial.print(currentVolume);
  Serial.println("/21");

  // Start radio stream
  if (isPlaying) {
    audio.connecttohost(stream_url);
  }
}

void radio_loop() {
  if (isPlaying) {
    audio.loop();
  }
}

void radio_increase_volume() {
  currentVolume = min(10, currentVolume + 1);
  audio.setVolume(currentVolume);
  #if VERBOSE_VOLUME_DEBUG
    Serial.print("*** Volume UP: ");
    Serial.print(currentVolume);
    Serial.println("/21");
  #endif
}

void radio_decrease_volume() {
  currentVolume = max(0, currentVolume - 1);
  audio.setVolume(currentVolume);
  #if VERBOSE_VOLUME_DEBUG
    Serial.print("*** Volume DOWN: ");
    Serial.print(currentVolume);
    Serial.println("/21");
  #endif
}

void radio_set_volume(int volume) {
    currentVolume = constrain(volume, 0, 21);
    audio.setVolume(currentVolume);
    #if VERBOSE_VOLUME_DEBUG
      Serial.print("*** Volume set to: ");
      Serial.print(currentVolume);
      Serial.println("/21");
    #endif
}

void radio_toggle_play_pause() {
  isPlaying = !isPlaying;
  if (isPlaying) {
    Serial.println("*** Radio PLAY ***");
    audio.connecttohost(stream_url);
  } else {
    Serial.println("*** Radio PAUSE ***");
    audio.stopSong();
  }
}

bool radio_is_playing() {
    return isPlaying;
}

int radio_get_volume() {
    return currentVolume;
}
