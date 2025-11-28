#include "radio.h"

void setup() {
  Serial.begin(115200);
  delay(2000); 
  Serial.println("\n=== ESP32 INTERNET RADIO ===");

  // Setup WiFi and Audio Stream
  radio_setup();
  
  Serial.println("\n=== Setup Complete. Starting main loop. ===");
}

void loop() {
  // Handle audio processing
  radio_loop();
}
