#include "ble_control.h"
#include "radio.h"

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEClient.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLESecurity.h>

// Manually define structs and enums from esp_gap_ble_api.h and esp_bt_defs.h to fix compilation issue
typedef uint8_t esp_bd_addr_t[6];
typedef uint8_t esp_bt_octet16_t[16];
typedef esp_bt_octet16_t esp_link_key;
typedef uint8_t esp_ble_addr_type_t;

typedef enum {
    ESP_BT_DEV_TYPE_BR_EDR,
    ESP_BT_DEV_TYPE_BLE,
    ESP_BT_DEV_TYPE_DUMO,
} esp_bt_dev_type_t;

typedef enum {
    ESP_AUTH_SMP_PASSKEY_FAIL = 78,
    ESP_AUTH_SMP_OOB_FAIL,
    ESP_AUTH_SMP_PAIR_AUTH_FAIL,
    ESP_AUTH_SMP_CONFIRM_VALUE_FAIL,
    ESP_AUTH_SMP_PAIR_NOT_SUPPORT,
    ESP_AUTH_SMP_ENC_KEY_SIZE,
    ESP_AUTH_SMP_INVALID_CMD,
    ESP_AUTH_SMP_UNKNOWN_ERR,
    ESP_AUTH_SMP_REPEATED_ATTEMPT,
    ESP_AUTH_SMP_INVALID_PARAMETERS,
    ESP_AUTH_SMP_DHKEY_CHK_FAIL,
    ESP_AUTH_SMP_NUM_COMP_FAIL,
    ESP_AUTH_SMP_BR_PARING_IN_PROGR,
    ESP_AUTH_SMP_XTRANS_DERIVE_NOT_ALLOW,
    ESP_AUTH_SMP_INTERNAL_ERR,
    ESP_AUTH_SMP_UNKNOWN_IO,
    ESP_AUTH_SMP_INIT_FAIL,
    ESP_AUTH_SMP_CONFIRM_FAIL,
    ESP_AUTH_SMP_BUSY,
    ESP_AUTH_SMP_ENC_FAIL,
    ESP_AUTH_SMP_STARTED,
    ESP_AUTH_SMP_RSP_TIMEOUT,
    ESP_AUTH_SMP_DIV_NOT_AVAIL,
    ESP_AUTH_SMP_UNSPEC_ERR,
    ESP_AUTH_SMP_CONN_TOUT,
} esp_ble_auth_fail_rsn_t;

typedef uint8_t esp_ble_auth_req_t;

typedef struct
{
    esp_bd_addr_t             bd_addr;
    bool                      key_present;
    esp_link_key              key;
    uint8_t                   key_type;
    bool                      success;
    esp_ble_auth_fail_rsn_t   fail_reason;
    esp_ble_addr_type_t       addr_type;
    esp_bt_dev_type_t         dev_type;
    esp_ble_auth_req_t        auth_mode;
} esp_ble_auth_cmpl_t;

// --- Xbox Wireless Controller Configuration ---
static BLEAddress targetDeviceAddress("44:16:22:E3:CB:B3");
// Xbox BLE Service UUID: 0x400000 (32-bit UUID, formatted as string for ESP32 BLE library)
static BLEUUID serviceUUID("00400000-0000-1000-8000-00805f9b34fb"); // Xbox BLE Service (0x400000 in 128-bit format)
static BLEUUID charUUID(uint16_t(0x2A4D));    // Standard HID Report

// --- Xbox Controller D-pad Definitions ---
#define DPAD_UP 0x01
#define DPAD_DOWN 0x02

// --- BLE State Machine ---
static BLEClient* pClient;
static BLEScan* pBLEScan;
static bool deviceConnected = false;
static bool doConnect = false;
static BLEAdvertisedDevice* myDevice = nullptr;

// --- D-pad press state tracking ---
static bool dpad_up_down = false;
static bool dpad_down_down = false;

// --- Callback Declarations ---
void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);

// --- BLE Callback Classes ---

class MyClientCallbacks : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
    Serial.println("\n========================================");
    Serial.println("*** BLUETOOTH STATUS: CONNECTED ***");
    Serial.println("*** Xbox Controller is now connected! ***");
    Serial.println("*** Note: Controller light may blink until fully paired/bonded ***");
    Serial.println("*** If you receive button data, connection is working! ***");
    Serial.println("========================================\n");
    deviceConnected = true;
  }

  void onDisconnect(BLEClient* pclient) {
    Serial.println("\n========================================");
    Serial.println("*** BLUETOOTH STATUS: DISCONNECTED ***");
    Serial.println("*** Xbox Controller disconnected. Will attempt to reconnect... ***");
    Serial.println("========================================\n");
    deviceConnected = false;
  }
};

class MySecurityCallbacks : public BLESecurityCallbacks {
    uint32_t onPassKeyRequest() {
        Serial.println(">>> onPassKeyRequest called");
        return 0; // Return 0 to use Just Works pairing (no PIN)
    }

    void onPassKeyNotify(uint32_t pass_key) {
        Serial.println("**************************************");
        Serial.printf("PASSKEY: %06d\n", pass_key);
        Serial.println("Enter this PIN on the Xbox controller to pair.");
        Serial.println("**************************************");
    }

    bool onSecurityRequest() {
        Serial.println(">>> onSecurityRequest called - accepting pairing");
        return true; // Accept security request
    }

    bool onConfirmPIN(uint32_t pin) {
        Serial.printf(">>> onConfirmPIN called with PIN: %06d - accepting\n", pin);
        return true; // Always accept PIN
    }

    void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) {
        Serial.println("\n========================================");
        Serial.println("--- Authentication Complete ---");
        if (cmpl.success) {
            Serial.println("*** Pairing and bonding SUCCESSFUL! ***");
            Serial.println("*** Controller should stop blinking now ***");
        } else {
            Serial.printf("*** Authentication FAILED. Reason: 0x%x ***\n", cmpl.fail_reason);
            Serial.println("*** Controller will continue blinking ***");
        }
        Serial.println("========================================\n");
    }
};

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    // Always show that we're finding devices (helps debug if scanning works)
    static int deviceCount = 0;
    deviceCount++;
    if (deviceCount == 1) {
      Serial.println("\n*** BLE SCAN WORKING: Found first device! ***");
    }
    
    // Always show device info (VERBOSE_BLE_DEBUG is enabled)
    Serial.print("BLE Device #");
    Serial.print(deviceCount);
    Serial.print(": ");
    Serial.print(advertisedDevice.getAddress().toString().c_str());
    Serial.print(" - RSSI: ");
    Serial.print(advertisedDevice.getRSSI());
    if (advertisedDevice.haveName()) {
      Serial.print(" - Name: ");
      Serial.print(advertisedDevice.getName().c_str());
    } else {
      Serial.print(" - Name: (none)");
    }
    if (advertisedDevice.haveServiceUUID()) {
      Serial.print(" - ServiceUUID: ");
      Serial.print(advertisedDevice.getServiceUUID().toString().c_str());
    }
    Serial.println();
    
    // Try multiple ways to identify the Xbox controller
    bool isTargetDevice = false;
    
    // Method 1: Check by MAC address (most reliable)
    if (advertisedDevice.getAddress().equals(targetDeviceAddress)) {
      Serial.println(">>> Found Xbox Controller by MAC ADDRESS!");
      isTargetDevice = true;
    }
    
    // Method 2: Check by name (Xbox - various patterns)
    if (!isTargetDevice && advertisedDevice.haveName()) {
      String name = advertisedDevice.getName();
      name.toLowerCase(); // Case-insensitive check
      if (name.indexOf("xbox") != -1 || 
          name.indexOf("controller") != -1 && name.indexOf("wireless") != -1) {
        Serial.print(">>> Found potential Xbox Controller by NAME: ");
        Serial.println(advertisedDevice.getName().c_str());
        Serial.println(">>> Attempting connection...");
        isTargetDevice = true;
      }
    }
    
    // Method 3: Check by service UUID (Xbox BLE service)
    if (!isTargetDevice && advertisedDevice.haveServiceUUID()) {
      BLEUUID advertisedServiceUUID = advertisedDevice.getServiceUUID();
      BLEUUID xboxServiceUUID("00400000-0000-1000-8000-00805f9b34fb"); // Xbox BLE Service (0x400000)
      if (advertisedServiceUUID.equals(xboxServiceUUID)) {
        Serial.println(">>> Found device with Xbox BLE service UUID!");
        Serial.println(">>> Attempting connection to Xbox controller...");
        isTargetDevice = true;
      }
    }
    
    if (isTargetDevice) {
      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
    }
  }
};

// --- Main BLE Functions ---

bool connectToServer() {
  Serial.println("\n========================================");
  Serial.println("*** BLUETOOTH: Attempting Connection ***");
  Serial.print(">>> Forming a connection to ");
  Serial.print(myDevice->getAddress().toString().c_str());
  if (myDevice->haveName()) {
    Serial.print(" (");
    Serial.print(myDevice->getName().c_str());
    Serial.print(")");
  }
  Serial.println();
  
  #if VERBOSE_BLE_DEBUG
    Serial.println(">>> Attempting BLE connection...");
  #endif
  
  if (!pClient->connect(myDevice)) {
    Serial.println(">>> ERROR: Failed to connect to device.");
    Serial.println("========================================\n");
    return false;
  }
  
  Serial.println(">>> Connected! Waiting for pairing to complete...");
  
  // Request MTU - this sometimes helps trigger pairing
  pClient->setMTU(517); // Request maximum MTU
  
  // Give time for pairing/bonding to complete
  delay(3000);
  
  // Check if we're still connected after pairing attempt
  if (!pClient->isConnected()) {
    Serial.println(">>> ERROR: Connection lost during pairing.");
    Serial.println(">>> This might indicate pairing failed.");
    Serial.println("========================================\n");
    return false;
  }
  
  Serial.println(">>> Still connected, discovering services...");
  Serial.println(">>> If controller light is still blinking, pairing may not have completed.");
  Serial.println(">>> Watch for 'Authentication Complete' messages above.");

  // Try to find Xbox BLE service or HID service (Xbox controllers often use HID)
  BLERemoteService* pRemoteService = nullptr;
  
  // Try Xbox BLE service UUID first
  pRemoteService = pClient->getService(serviceUUID);
  
  #if VERBOSE_BLE_DEBUG
    if (pRemoteService) {
      Serial.println(">>> Found Xbox BLE service (0x400000)");
    } else {
      Serial.println(">>> Xbox BLE service not found, checking HID service...");
    }
  #endif
  
  // If Xbox service not found, try standard HID service (Xbox controllers often use this)
  if (pRemoteService == nullptr) {
    BLEUUID hidServiceUUID(uint16_t(0x1812)); // Standard HID Service
    pRemoteService = pClient->getService(hidServiceUUID);
    if (pRemoteService) {
      Serial.println(">>> Found HID service (0x1812) - Xbox controller likely uses this");
    }
  }
  
  // If still not found, try to list all services and find any that might work
  if (pRemoteService == nullptr) {
    std::map<std::string, BLERemoteService*>* services = pClient->getServices();
    Serial.print(">>> Device has ");
    Serial.print(services->size());
    Serial.println(" service(s):");
    
    for (auto const& pair : *services) {
      Serial.print(">>>   Service UUID: ");
      Serial.println(pair.first.c_str());
      
      // Try any service that might be Xbox-related or HID-related
      if (pair.first.find("400000") != std::string::npos || 
          pair.first.find("1812") != std::string::npos) {
        pRemoteService = pair.second;
        Serial.println(">>>   Using this service for Xbox controller");
        break;
      }
    }
  }

  if (pRemoteService == nullptr) {
    Serial.println(">>> ERROR: Failed to find Xbox BLE service or HID service.");
    pClient->disconnect();
    return false;
  }

  Serial.println(">>> Discovering characteristics...");
  
  // Try to find Xbox controller characteristic
  BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
  
  #if VERBOSE_BLE_DEBUG
    if (pRemoteCharacteristic) {
      Serial.println(">>> Found standard characteristic (0x2A4D)");
    } else {
      Serial.println(">>> Standard characteristic not found, listing all characteristics...");
      std::map<std::string, BLERemoteCharacteristic*>* characteristics = pRemoteService->getCharacteristics();
      for (auto const& pair : *characteristics) {
        Serial.print(">>>   Characteristic UUID: ");
        Serial.println(pair.first.c_str());
      }
    }
  #endif
  
  // If standard characteristic not found, try to find any notify-capable characteristic
  if (pRemoteCharacteristic == nullptr) {
    std::map<std::string, BLERemoteCharacteristic*>* characteristics = pRemoteService->getCharacteristics();
    for (auto const& pair : *characteristics) {
      BLERemoteCharacteristic* pChar = pair.second;
      if (pChar->canNotify()) {
        Serial.print(">>> Found notify-capable characteristic: ");
        Serial.println(pair.first.c_str());
        pRemoteCharacteristic = pChar;
        break;
      }
    }
  }

  if (pRemoteCharacteristic == nullptr) {
    Serial.println(">>> ERROR: Failed to find Xbox controller characteristic.");
    pClient->disconnect();
    return false;
  }

  if(pRemoteCharacteristic->canNotify()) {
    // Enable notifications/indications
    pRemoteCharacteristic->registerForNotify(notifyCallback);
    Serial.println(">>> Registered notification callback");
    
    // Also try to write to the CCCD (Client Characteristic Configuration Descriptor) to enable notifications
    // Some devices require this explicit write
    try {
      BLERemoteDescriptor* pCCCD = pRemoteCharacteristic->getDescriptor(BLEUUID((uint16_t)0x2902));
      if (pCCCD != nullptr) {
        uint8_t notifyOn[] = {0x01, 0x00}; // Enable notifications
        pCCCD->writeValue(notifyOn, 2, true);
        Serial.println(">>> CCCD descriptor written to enable notifications");
      } else {
        Serial.println(">>> Warning: CCCD descriptor not found, using registerForNotify only");
      }
    } catch (...) {
      Serial.println(">>> Warning: Could not write CCCD descriptor, using registerForNotify only");
    }
    
    Serial.println(">>> Successfully subscribed to Xbox controller notifications!");
    Serial.println(">>> Xbox Controller is ready! Press D-pad UP for volume up, D-pad DOWN for volume down.");
    Serial.println(">>> Watch for 'NOTIFICATION RECEIVED' messages - if none appear, pairing may be incomplete.");
    Serial.println(">>> If controller light is blinking, pairing may still be in progress.");
    Serial.println(">>> Try pressing buttons on the controller - you should see notification messages.");
    Serial.println("========================================\n");
  } else {
    Serial.println(">>> ERROR: Xbox controller characteristic does not support notifications.");
    Serial.println("========================================\n");
    pClient->disconnect();
    return false;
  }
  return true;
}

void notifyCallback(BLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    // Always log that we received a notification (even if empty)
    Serial.print(">>> NOTIFICATION RECEIVED! length=");
    Serial.print(length);
    Serial.print(" isNotify=");
    Serial.println(isNotify ? "true" : "false");
    
    if (length < 1) {
      Serial.println(">>> Empty report received");
      return;
    }

    // Always show the report data to help debug the format
    Serial.print(">>> Report data: ");
    for (size_t i = 0; i < length && i < 20; i++) { // Limit to 20 bytes for readability
      Serial.print(" [");
      Serial.print(i);
      Serial.print("]=0x");
      if (pData[i] < 0x10) Serial.print("0");
      Serial.print(pData[i], HEX);
    }
    Serial.println();
    
    // Show byte 12 specifically (Xbox D-pad location)
    if (length >= 13) {
      Serial.print(">>> Byte 12 (D-pad): 0x");
      if (pData[12] < 0x10) Serial.print("0");
      Serial.print(pData[12], HEX);
      Serial.print(" (");
      Serial.print(pData[12]);
      Serial.println(")");
    }
    
    if (length < 3) {
      Serial.println(">>> Report too short for button parsing, but data received!");
      return;
    }

    bool dpad_up_pressed_now = false;
    bool dpad_down_pressed_now = false;
    bool any_button_pressed = false;
    String pressed_buttons = "";

    // Xbox Wireless Controller HID report format (based on esp32s3_xbox_adapter project):
    // Bytes 0-11: Analog sticks and triggers (joyLHori, joyLVert, joyRHori, joyRVert, trigLT, trigRT)
    // Byte 12: D-pad direction (0x00=none, 0x01=UP, 0x02=UP+RIGHT, 0x04=RIGHT, etc.)
    // Byte 13: Main buttons (A, B, X, Y, LB, RB)
    // Byte 14: Center buttons (Select, Start, Xbox, LS, RS)
    // Byte 15: Share button
    
    // Check for any button activity (non-zero data usually means something is pressed)
    for (size_t i = 1; i < length && i < 16; i++) {
      if (pData[i] != 0) {
        any_button_pressed = true;
        break;
      }
    }
    
    // Method 1: Check byte 12 for D-pad (Xbox controller format from esp32s3_xbox_adapter)
    if (length >= 13) {
      uint8_t dpad_value = pData[12];
      // D-pad values: 0=none, 1=UP, 2=UP+RIGHT, 3=RIGHT, 4=DOWN+RIGHT, 5=DOWN, 6=DOWN+LEFT, 7=LEFT, 8=UP+LEFT
      Serial.print(">>> Checking byte 12, value=");
      Serial.print(dpad_value);
      Serial.print(" (0x");
      if (dpad_value < 0x10) Serial.print("0");
      Serial.print(dpad_value, HEX);
      Serial.println(")");
      
      if (dpad_value == 1 || dpad_value == 2 || dpad_value == 8) { // UP, UP+RIGHT, or UP+LEFT
        dpad_up_pressed_now = true;
        pressed_buttons += "D-pad UP ";
        Serial.println(">>> D-pad UP detected!");
      }
      if (dpad_value == 4 || dpad_value == 5 || dpad_value == 6) { // DOWN+RIGHT, DOWN, or DOWN+LEFT
        dpad_down_pressed_now = true;
        pressed_buttons += "D-pad DOWN ";
        Serial.println(">>> D-pad DOWN detected!");
      }
      if (dpad_value == 2 || dpad_value == 3 || dpad_value == 4) pressed_buttons += "D-pad RIGHT ";
      if (dpad_value == 6 || dpad_value == 7 || dpad_value == 8) pressed_buttons += "D-pad LEFT ";
      
      // Also check if it's non-zero (any direction pressed)
      if (dpad_value != 0 && !dpad_up_pressed_now && !dpad_down_pressed_now) {
        Serial.print(">>> D-pad pressed but not UP/DOWN: value=");
        Serial.println(dpad_value);
      }
    } else {
      Serial.print(">>> Report too short for byte 12 check (length=");
      Serial.print(length);
      Serial.println(")");
    }
    
    // Method 2: Check byte 2 lower nibble (common HID gamepad format) as fallback
    if (!dpad_up_pressed_now && !dpad_down_pressed_now && length >= 3) {
      uint8_t dpad_byte = pData[2] & 0x0F;
      if (dpad_byte == 0x01 || dpad_byte == 0x07 || dpad_byte == 0x05) {
        dpad_up_pressed_now = true;
        pressed_buttons += "D-pad UP(alt) ";
      }
      if (dpad_byte == 0x02 || dpad_byte == 0x06 || dpad_byte == 0x0A) {
        dpad_down_pressed_now = true;
        pressed_buttons += "D-pad DOWN(alt) ";
      }
    }
    
    // Also show other button info for debugging
    if (length >= 14) {
      uint8_t main_buttons = pData[13];
      if (main_buttons & 0x01) pressed_buttons += "A ";
      if (main_buttons & 0x02) pressed_buttons += "B ";
      if (main_buttons & 0x08) pressed_buttons += "X ";
      if (main_buttons & 0x10) pressed_buttons += "Y ";
      if (main_buttons & 0x40) pressed_buttons += "LB ";
      if (main_buttons & 0x80) pressed_buttons += "RB ";
    }
    
    if (length >= 15) {
      uint8_t center_buttons = pData[14];
      if (center_buttons & 0x04) pressed_buttons += "Select ";
      if (center_buttons & 0x08) pressed_buttons += "Start ";
      if (center_buttons & 0x10) pressed_buttons += "Xbox ";
    }

    // Show message when any button is pressed
    if (any_button_pressed) {
      if (pressed_buttons.length() > 0) {
        Serial.print(">>> BUTTON PRESSED: ");
        Serial.println(pressed_buttons);
      } else {
        Serial.println(">>> BUTTON PRESSED: (detected activity but format unknown)");
      }
    }

    // Only trigger volume control on press (not release) - edge detection
    if (dpad_up_pressed_now && !dpad_up_down) {
        Serial.println(">>> D-pad UP pressed -> Volume UP");
        radio_increase_volume();
    }
    if (dpad_down_pressed_now && !dpad_down_down) {
        Serial.println(">>> D-pad DOWN pressed -> Volume DOWN");
        radio_decrease_volume();
    }

    dpad_up_down = dpad_up_pressed_now;
    dpad_down_down = dpad_down_pressed_now;
}

// --- Public Functions ---

void ble_control_setup() {
  Serial.println("\n=== Xbox Controller Control Sketch ===");
  Serial.println("Initializing BLE...");
  
  // Initialize BLE device
  BLEDevice::init("ESP32-Radio-Xbox");
  Serial.println(">>> BLEDevice::init() completed");

  // Set security callbacks
  BLEDevice::setSecurityCallbacks(new MySecurityCallbacks());
  Serial.println(">>> Security callbacks set");
  
  // Enable BLE security: Xbox controllers require pairing/bonding
  BLESecurity *pSecurity = new BLESecurity();
  // Use Just Works pairing (no PIN required) with bonding
  pSecurity->setAuthenticationMode(ESP_LE_AUTH_BOND); // Bonding required
  pSecurity->setCapability(ESP_IO_CAP_NONE); // No input/output capability (Just Works pairing)
  pSecurity->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  Serial.println(">>> BLE security enabled (Just Works pairing with bonding)");
  
  // Set BLE power level
  BLEDevice::setPower(ESP_PWR_LVL_P9); 
  Serial.println(">>> BLE power set to maximum (P9)");
  
  // Create BLE client
  pClient = BLEDevice::createClient();
  if (pClient == nullptr) {
    Serial.println(">>> ERROR: Failed to create BLE client!");
    return;
  }
  Serial.println(">>> BLE client created");
  pClient->setClientCallbacks(new MyClientCallbacks());
  Serial.println(">>> Client callbacks set");

  // Setup BLE scan
  pBLEScan = BLEDevice::getScan();
  if (pBLEScan == nullptr) {
    Serial.println(">>> ERROR: Failed to get BLE scan object!");
    return;
  }
  Serial.println(">>> BLE scan object obtained");
  
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  Serial.println(">>> Advertised device callbacks set");
  
  pBLEScan->setActiveScan(true);
  Serial.println(">>> Active scan enabled");
  
  pBLEScan->setInterval(1349);  // Scan interval
  pBLEScan->setWindow(449);      // Scan window
  Serial.println(">>> Scan parameters configured (interval: 1349ms, window: 449ms)");
  
  // Start scanning
  bool scanStarted = pBLEScan->start(0, false);     // 0 = continuous scan, false = don't clear results
  if (scanStarted) {
    Serial.println(">>> BLE scan STARTED successfully!");
  } else {
    Serial.println(">>> ERROR: Failed to start BLE scan!");
  }
  
  Serial.println("\n========================================");
  Serial.println(">>> BLE initialized. Scanning for Xbox Controller...");
  Serial.println(">>> Make sure your Xbox controller is powered on and in pairing mode!");
  Serial.println(">>> Looking for devices with:");
  Serial.println(">>>   - Name containing 'Xbox'");
  Serial.println(">>>   - MAC address: 44:16:22:E3:CB:B3");
  Serial.println(">>>   - Xbox BLE service (0x400000)");
  Serial.println("========================================\n");
}
static unsigned long lastScanStatusTime = 0;
static unsigned long scanStartTime = 0;
static unsigned long lastConnectionStatusTime = 0;
static bool lastReportedConnectionState = false;

void ble_control_loop() {
  unsigned long now = millis();
  
  // Report connection state changes and periodically if disconnected
  if (deviceConnected != lastReportedConnectionState) {
    if (deviceConnected) {
      Serial.println("\n*** BLUETOOTH CONNECTION STATE: CONNECTED ***");
    } else {
      Serial.println("\n*** BLUETOOTH CONNECTION STATE: DISCONNECTED ***");
    }
    lastReportedConnectionState = deviceConnected;
    lastConnectionStatusTime = now;
  } else if (!deviceConnected && (now - lastConnectionStatusTime > 10000)) {
    // Every 10 seconds when disconnected, show status
    Serial.println("\n*** BLUETOOTH STATUS: DISCONNECTED - Attempting to reconnect... ***");
    Serial.println("*** Make sure the Xbox Controller is powered on and in pairing mode ***");
    lastConnectionStatusTime = now;
  }

  if (doConnect) {
    Serial.println("\n>>> Attempting to connect to Xbox Controller...");
    if (connectToServer()) {
      Serial.println(">>> Connection successful!");
    } else {
      Serial.println(">>> Connection failed, will re-scan...");
      delay(1000); // Brief delay before rescanning
    }
    doConnect = false;
    if (myDevice != nullptr) {
        delete myDevice;
        myDevice = nullptr;
    }
  }

  // Print periodic scan status
  if (!deviceConnected) {
    if (scanStartTime == 0) {
      scanStartTime = now;
      Serial.println("\n*** Starting BLE scan for Xbox Controller... ***");
    }
    
    // Check if scan is actually running
    bool isScanning = (pBLEScan != nullptr && pBLEScan->isScanning());
    if (!isScanning && (now - scanStartTime > 2000)) {
      Serial.println("\n*** WARNING: BLE scan is NOT running! Attempting to restart... ***");
      scanStartTime = 0;
      lastScanStatusTime = 0;
      delay(500);
      if (pBLEScan != nullptr) {
        pBLEScan->start(0, false);
      }
    }
    
    if (isScanning && (now - lastScanStatusTime > 5000)) { // Every 5 seconds
      unsigned long scanDuration = (now - scanStartTime) / 1000;
      Serial.print("*** Still scanning for Xbox Controller... (");
      Serial.print(scanDuration);
      Serial.println(" seconds elapsed)");
      Serial.println("*** Make sure Xbox Controller is powered on and in pairing mode ***");
      Serial.println("*** If you see 'BLE SCAN WORKING' messages, scanning is active ***");
      lastScanStatusTime = now;
    }
  }

  // Auto-reconnect: if disconnected and not scanning, restart scan
  if (!deviceConnected && !pBLEScan->isScanning()) {
    Serial.println("\n*** BLUETOOTH: Not connected and not scanning. Restarting scan... ***");
    scanStartTime = 0;
    lastScanStatusTime = 0;
    delay(500);
    pBLEScan->start(0, false);
  }
}

bool ble_is_connected() {
  return deviceConnected;
}
