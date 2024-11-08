#include <WiFi.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include <cert.h>

#define LED_BUILTIN 2 // Set to your board's LED pin, or any other GPIO pin with an LED

// WiFi credentials
const char* ssid = "Dialog 4G";
const char* password = "212FFAL1LAN";

// Firmware version and update URLs
const String FirmwareVer = "2.2";
#define URL_fw_Version "https://github.com/ParamiAshinsana/ESP32-OTA/raw/5ab6d967c4f42410495c54be0f014305be239dca/bin_version.txt"

#define URL_fw_Bin "https://github.com/ParamiAshinsana/ESP32-OTA/raw/855622e47528801f2f840ff3759aca2e53836ed3/sketch_nov7a.ino.bin"

// Function prototypes
void connect_wifi();
void firmwareUpdate();
int FirmwareVersionCheck();

// Timing variables
unsigned long previousMillis = 0;
unsigned long previousMillis_2 = 0;
const long interval = 60000;
const long mini_interval = 1000;

struct Button {
  const uint8_t PIN;
  uint32_t numberKeyPresses;
  bool pressed;
};

Button button_boot = {0, 0, false};

// ISR for button press
void IRAM_ATTR isr() {
  button_boot.numberKeyPresses += 1;
  button_boot.pressed = true;
}

// Main repeated function to check for updates
void repeatedCall() {
  static int num = 0;
  unsigned long currentMillis = millis();

  if ((currentMillis - previousMillis) >= interval) {
    previousMillis = currentMillis;
    if (FirmwareVersionCheck()) {
      firmwareUpdate();
    }
  }
  if ((currentMillis - previousMillis_2) >= mini_interval) {
    previousMillis_2 = currentMillis;
    Serial.print("idle loop...");
    Serial.print(num++);
    Serial.print(" Active fw version: ");
    Serial.println(FirmwareVer);

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi connected");
    } else {
      connect_wifi();
    }
  }
}

void setup() {
  pinMode(button_boot.PIN, INPUT);
  attachInterrupt(button_boot.PIN, isr, RISING);
  Serial.begin(115200);
  Serial.print("Active firmware version: ");
  Serial.println(FirmwareVer);
  pinMode(LED_BUILTIN, OUTPUT);
  connect_wifi();
}

void loop() {
  if (button_boot.pressed) {
    Serial.println("Firmware update starting...");
    firmwareUpdate();
    button_boot.pressed = false;
  }
  repeatedCall();
}

// WiFi connection function with timeout
void connect_wifi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi");
  }
}

// Firmware update function
void firmwareUpdate() {
  WiFiClientSecure client;
  client.setCACert(rootCACertificate);
  httpUpdate.setLedPin(LED_BUILTIN, LOW);
  t_httpUpdate_return ret = httpUpdate.update(client, URL_fw_Bin);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTP_UPDATE_FAILED Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("HTTP_UPDATE_OK");
      break;
  }
}

// Function to check if a firmware update is available
int FirmwareVersionCheck() {
  String payload;
  int httpCode;
  String fwurl = URL_fw_Version;
  fwurl += "?" + String(millis());  // Cache busting

  WiFiClientSecure* client = new WiFiClientSecure;
  if (client) {
    client->setCACert(rootCACertificate);

    HTTPClient https;
    if (https.begin(*client, fwurl)) {
      Serial.print("[HTTPS] GET...\n");
      delay(100);
      httpCode = https.GET();
      delay(100);

      if (httpCode == HTTP_CODE_OK) {
        payload = https.getString();
      } else {
        Serial.print("Error downloading version file: ");
        Serial.println(httpCode);
      }
      https.end();
    }
    delete client;
  }

  if (httpCode == HTTP_CODE_OK) {
    payload.trim();
    if (payload.equals(FirmwareVer)) {
      Serial.printf("\nDevice already on latest firmware version: %s\n", FirmwareVer.c_str());
      return 0;
    } else {
      Serial.println(payload);
      Serial.println("New firmware detected");
      return 1;
    }
  }
  return 0;
}
