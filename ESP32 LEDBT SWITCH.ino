#include <WiFi.h>
#include <WebServer.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLERemoteCharacteristic.h>

// --- Wi-Fi Configuration ---
const char* ssid = "";
const char* password = "";

WebServer server(80);

// --- Status LED ---
#define READY_PIN 26

// --- BLE UUIDs ---
// Standard UUIDs for Melk and Mr. Star
BLEUUID serviceUUID("0000fff0-0000-1000-8000-00805f9b34fb");
BLEUUID charUUID("0000fff3-0000-1000-8000-00805f9b34fb");

// LEDnetWF (Zengge) UUIDs
BLEUUID lednetServiceUUID("0000ffff-0000-1000-8000-00805f9b34fb");
BLEUUID lednetWriteUUID("0000ff01-0000-1000-8000-00805f9b34fb");

// --- BLE Addresses ---
BLEAddress melkAddrs[3] = {
  BLEAddress("BE:69:FF:06:67:11"),
  BLEAddress("BE:67:00:78:14:8A"),
  BLEAddress("BE:67:00:53:11:A5")
};
BLEAddress mrStar("E4:98:BB:F3:19:E2");
BLEAddress lednetAddr("08:65:F0:92:5F:4E");

// --- Global Clients for Melk (Persistent) ---
BLEClient* melkClients[3] = {nullptr, nullptr, nullptr};
BLERemoteCharacteristic* melkChars[3] = {nullptr, nullptr, nullptr};

// --- LED State ---
volatile bool ledState = false;
volatile bool ledUpdate = false;

// --- Connect Helper for Melk ---
bool connectToMelk(int index) {
  if (melkClients[index] == nullptr) {
    melkClients[index] = BLEDevice::createClient();
  }
  
  if (melkClients[index]->isConnected()) return true;

  Serial.printf("Connecting Melk %d... ", index);
  if (melkClients[index]->connect(melkAddrs[index])) {
    Serial.println("OK");
    BLERemoteService* pSvc = melkClients[index]->getService(serviceUUID);
    if (pSvc) {
      melkChars[index] = pSvc->getCharacteristic(charUUID);
      return true;
    }
  }
  Serial.println("Failed");
  return false;
}

// --- Main Sequence Logic ---
void executeSequenceAndReboot(bool state) {
  digitalWrite(READY_PIN, LOW); // LED OFF = Busy
  
  // Payload for Melk and Mr. Star
  byte melkCmd[9] = {0x7E,0x04,0x04,(byte)(state?0x01:0x00),0x00,(byte)(state?0x01:0x00),0xFF,0x00,0xEF};
  byte starCmd[5] = {0xBC,0x01,0x01,(byte)(state?0x01:0x00),0x55};

  // --- LOOP TWICE ---
  for (int k = 1; k <= 2; k++) {
    Serial.printf("\n=== SEQUENCE LOOP %d/2 ===\n", k);

    // 1. SEND TO MELK
    Serial.println("--- 1. Sending to MELK ---");
    for (int i = 0; i < 3; i++) {
      connectToMelk(i);
      if (melkClients[i]->isConnected() && melkChars[i] != nullptr) {
        melkChars[i]->writeValue(melkCmd, 9, false);
        Serial.printf("Melk %d Command Sent.\n", i);
      }
    }
    delay(200);

    // 2. DISCONNECT MELK 
    for (int i = 0; i < 3; i++) {
      if (melkClients[i]->isConnected()) {
        melkClients[i]->disconnect();
      }
    }
    delay(300); 

    // 3. MR STAR PAYLOAD
    Serial.println("--- 3. Connecting to MR STAR ---");
    BLEClient* starClient = BLEDevice::createClient();
    if (starClient->connect(mrStar)) {
      BLERemoteService* pSvc = starClient->getService(serviceUUID);
      if (pSvc) {
        BLERemoteCharacteristic* pChar = pSvc->getCharacteristic(charUUID);
        if (pChar) {
          pChar->writeValue(starCmd, 5, false);
          Serial.println("Mr Star Command Sent.");
        }
      }
      starClient->disconnect();
    }
    
    delay(300);

    // 4. SEND TO LEDnetWF (Zengge Protocol)
    Serial.println("--- 4. Connecting to LEDnetWF ---");
    uint8_t lednetPacket[21] = {
        0x00, 0x01, // Counter
        0x80, 0x00, 0x00, 0x0D, 0x0E, 0x0B, 0x3B, 
        (uint8_t)(state ? 0x23 : 0x24), // Action
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
        0x32, // Speed/Brightness
        0x00, 0x00, 0x00 // Last byte is checksum
    };
    
    // Checksum: Sum of bytes 0 to 19
    int sum = 0;
    for (int i = 0; i < 20; i++) sum += lednetPacket[i];
    lednetPacket[20] = (uint8_t)(sum & 0xFF);

    BLEClient* lednetClient = BLEDevice::createClient();
    if (lednetClient->connect(lednetAddr)) {
      BLERemoteService* pSvc = lednetClient->getService(lednetServiceUUID);
      if (pSvc) {
        BLERemoteCharacteristic* pChar = pSvc->getCharacteristic(lednetWriteUUID);
        if (pChar) {
          pChar->writeValue(lednetPacket, 21, true); // Write with response
          Serial.println("LEDnetWF Command Sent.");
          delay(500); // Give the radio time to receive the ACK
        }
      }
      lednetClient->disconnect();
    }
    // Note: No delete used here; let reboot handle memory cleanup
    
    Serial.printf("=== END LOOP %d ===\n", k);
    delay(500); 
  }

  Serial.println("\n--- SEQUENCE COMPLETE: REBOOTING ---");
  delay(200);
  ESP.restart();
}

// --- Web Handlers ---
void handleRoot() {
  String html = "<html><head><meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>.btn{padding:20px; font-size:25px; display:block; width:100%; margin-bottom:10px; color:white; border:none; border-radius:10px; text-decoration:none; text-align:center;}</style></head>";
  html += "<body style='font-family:sans-serif; text-align:center;'><h1>LED Master</h1>";
  html += "<p>Next Sequence: <b>" + String(ledState ? "OFF" : "ON") + "</b></p>";
  html += "<a href='/on' class='btn' style='background:#4CAF50;'>ALL ON</a>";
  html += "<a href='/off' class='btn' style='background:#f44336;'>ALL OFF</a>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  pinMode(READY_PIN, OUTPUT);
  digitalWrite(READY_PIN, LOW); // LED OFF during startup

  // Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
  Serial.println("\nIP: " + WiFi.localIP().toString());

  // Web Server
  server.on("/", handleRoot);
  server.on("/on", []() { ledState = true; ledUpdate = true; server.send(200, "text/plain", "Starting ON Sequence..."); });
  server.on("/off", []() { ledState = false; ledUpdate = true; server.send(200, "text/plain", "Starting OFF Sequence..."); });
  server.begin();

  // BLE Init
  BLEDevice::init("ESP32_LED_MASTER");
  
  // Initial Melk connections to speed up the first trigger
  for (int i = 0; i < 3; i++) connectToMelk(i);
  
  digitalWrite(READY_PIN, HIGH); // LED indicator if server is ready
  Serial.println("System Ready!");
}

void loop() {
  server.handleClient();

  if (ledUpdate) {
    ledUpdate = false;
    executeSequenceAndReboot(ledState);
  }
}