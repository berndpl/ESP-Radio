#include "radio.h"
#include "ble_control.h"

bool radio_started = false;

void setup() {
  Serial.begin(115200);
  delay(2000); 
  Serial.println("\n=== ESP32 INTERNET RADIO WITH BLUETOOTH XBOX CONTROLLER CONTROL ===");
  Serial.println("Waiting for Xbox Controller to connect before starting radio...");

  // Initialize Bluetooth
  ble_control_setup();
}

void loop() {
  // Always handle BLE
  ble_control_loop();

  // If BLE is connected and radio hasn't started yet, start the radio.
  if (ble_is_connected() && !radio_started) {
    Serial.println("\n========================================");
    Serial.println("*** Xbox Controller connected! Starting Radio... ***");
    Serial.println("========================================\n");
    radio_setup();
    radio_started = true;
    Serial.println("\n=== Setup Complete. Radio is running. ===");
  }

  // Only loop the radio if it has been started
  if (radio_started) {
    radio_loop();
  }

  delay(10);
}
