#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ESP32Encoder.h>
#include <AceButton.h>

using namespace ace_button;

#include "secrets.h"  // WiFi credentials and light IPs (copy secrets.h.example to secrets.h)

const int WIZ_PORT = 38899;

// Pin definitions
#define ENCODER_CLK 25
#define ENCODER_DT 26
#define ENCODER_SW 27
#define BUTTON_STUDY 33      // Button 2 → Office Lamp
#define BUTTON_UPLIGHT 32    // Button 1 → Uplight

// Objects
ESP32Encoder encoder;
WiFiUDP udp;
AceButton buttonStudy;
AceButton buttonUplight;
AceButton buttonEncoder;

// Separate ButtonConfig objects for each button
ButtonConfig studyButtonConfig;
ButtonConfig uplightButtonConfig;
ButtonConfig encoderButtonConfig;

// State variables
int brightness = 50;  // 10-100 (WiZ range)
const int MIN_BRIGHTNESS = 10;
const int MAX_BRIGHTNESS = 100;
const int BRIGHTNESS_STEP = 2;  // 2% per detent for smoother control

bool studyLampOn = false;
bool uplightOn = false;
int colorTempMode = 0;  // 0=2200K, 1=2700K, 2=4000K, 3=6500K

static uint32_t messageId = 1;  // Message counter for WiZ protocol


// Function prototypes
void sendWizCommand(IPAddress ip, bool state, int brightness);
void sendWizColorTemp(IPAddress ip, int brightness, int colorTemp);
void handleEncoderButton(AceButton*, uint8_t, uint8_t);
void handleStudyButton(AceButton*, uint8_t, uint8_t);
void handleUplightButton(AceButton*, uint8_t, uint8_t);

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n\n=================================");
  Serial.println("ESP32 WiZ Dimmer Starting...");
  Serial.println("=================================");

  // Setup encoder
  Serial.println("1. Setting up encoder...");
  ESP32Encoder::useInternalWeakPullResistors = UP;
  encoder.attachHalfQuad(ENCODER_DT, ENCODER_CLK);
  encoder.setCount(brightness / BRIGHTNESS_STEP);
  Serial.println("   Encoder OK");

  // Setup buttons
  Serial.println("2. Setting up buttons...");
  pinMode(ENCODER_SW, INPUT_PULLUP);
  pinMode(BUTTON_STUDY, INPUT);
  pinMode(BUTTON_UPLIGHT, INPUT);
  Serial.println("   Pins configured");

  buttonEncoder.init(ENCODER_SW, HIGH);
  buttonStudy.init(BUTTON_STUDY, LOW);
  buttonUplight.init(BUTTON_UPLIGHT, LOW);
  Serial.println("   Buttons initialized");

  // Debug: Show initial button states
  Serial.print("   Button states - Encoder: ");
  Serial.print(digitalRead(ENCODER_SW));
  Serial.print(", Study: ");
  Serial.print(digitalRead(BUTTON_STUDY));
  Serial.print(", Uplight: ");
  Serial.println(digitalRead(BUTTON_UPLIGHT));
  Serial.println("   (Encoder=1/Study=0/Uplight=0 when not pressed)");

  Serial.println("3. Configuring button handlers...");

  // Configure encoder button with its own config
  encoderButtonConfig.setEventHandler(handleEncoderButton);
  encoderButtonConfig.setFeature(ButtonConfig::kFeatureClick);
  encoderButtonConfig.setFeature(ButtonConfig::kFeatureDoubleClick);
  encoderButtonConfig.setFeature(ButtonConfig::kFeatureSuppressAfterDoubleClick);
  encoderButtonConfig.setFeature(ButtonConfig::kFeatureSuppressClickBeforeDoubleClick);
  encoderButtonConfig.setClickDelay(250);  // Time window for detecting double click
  buttonEncoder.setButtonConfig(&encoderButtonConfig);

  // Configure study button with its own config
  studyButtonConfig.setEventHandler(handleStudyButton);
  studyButtonConfig.setFeature(ButtonConfig::kFeatureClick);
  buttonStudy.setButtonConfig(&studyButtonConfig);

  // Configure uplight button with its own config
  uplightButtonConfig.setEventHandler(handleUplightButton);
  uplightButtonConfig.setFeature(ButtonConfig::kFeatureClick);
  buttonUplight.setButtonConfig(&uplightButtonConfig);

  Serial.println("   Button handlers OK");

  // Connect to WiFi
  Serial.println("4. Connecting to WiFi...");
  Serial.print("   SSID: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection FAILED!");
    Serial.print("WiFi status: ");
    Serial.println(WiFi.status());
    Serial.println("Check your SSID and password!");
  }

  udp.begin(38900);

  Serial.println("Ready! Turn the encoder to adjust brightness.");
  Serial.println("Press buttons to toggle lights on/off.");
}

void loop() {
  // WiFi reconnection check (every 30 seconds)
  static unsigned long lastWifiCheck = 0;
  if (millis() - lastWifiCheck > 30000) {
    lastWifiCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("WiFi disconnected, reconnecting...");
      WiFi.disconnect();
      WiFi.begin(ssid, password);
    }
  }

  // Check encoder rotation
  static int lastEncoderCount = brightness / BRIGHTNESS_STEP;
  int currentCount = encoder.getCount();

  // Clamp encoder count to valid range unconditionally (prevents dead zones at limits)
  const int minCount = MIN_BRIGHTNESS / BRIGHTNESS_STEP;
  const int maxCount = MAX_BRIGHTNESS / BRIGHTNESS_STEP;
  if (currentCount < minCount) {
    encoder.setCount(minCount);
    currentCount = minCount;
  } else if (currentCount > maxCount) {
    encoder.setCount(maxCount);
    currentCount = maxCount;
  }

  if (currentCount != lastEncoderCount) {
    lastEncoderCount = currentCount;
    brightness = currentCount * BRIGHTNESS_STEP;
    Serial.print("Brightness: ");
    Serial.println(brightness);

    // Only send to lights that are ON
    bool anySent = false;
    if (studyLampOn) {
      sendWizCommand(STUDY_LAMP, true, brightness);
      anySent = true;
    }
    if (uplightOn) {
      if (anySent) delay(50);
      sendWizCommand(UPLIGHT, true, brightness);
      anySent = true;
    }

    if (!anySent) {
      Serial.println("  (Both lights OFF - brightness will apply when turned ON)");
    }
  }

  // Check buttons
  buttonEncoder.check();
  buttonStudy.check();
  buttonUplight.check();

  delay(10);
}

void sendWizCommand(IPAddress ip, bool state, int brightness) {
  String json;

  if (state) {
    json = "{\"id\":" + String(messageId) +
           ",\"method\":\"setPilot\",\"params\":{\"state\":true,\"dimming\":" +
           String(brightness) + "}}";
  } else {
    json = "{\"id\":" + String(messageId) +
           ",\"method\":\"setPilot\",\"params\":{\"state\":false}}";
  }

  udp.beginPacket(ip, WIZ_PORT);
  udp.print(json);
  int result = udp.endPacket();

  if (result == 0) {
    Serial.print("   ERROR: UDP send failed to ");
    Serial.println(ip);
    return;
  }

  Serial.print("Sent to ");
  Serial.print(ip);
  Serial.print(" [ID:");
  Serial.print(messageId);
  Serial.print("]: ");
  Serial.println(json);

  messageId++;
}

void sendWizColorTemp(IPAddress ip, int brightness, int colorTemp) {
  // Send color temperature and brightness WITHOUT state parameter
  // Lights will adjust if on, ignore if off
  String json = "{\"id\":" + String(messageId) +
                ",\"method\":\"setPilot\",\"params\":{\"dimming\":" +
                String(brightness) + ",\"temp\":" + String(colorTemp) + "}}";

  udp.beginPacket(ip, WIZ_PORT);
  udp.print(json);
  int result = udp.endPacket();

  if (result == 0) {
    Serial.print("   ERROR: UDP send failed to ");
    Serial.println(ip);
    return;
  }

  Serial.print("Sent to ");
  Serial.print(ip);
  Serial.print(" [ID:");
  Serial.print(messageId);
  Serial.print("]: ");
  Serial.println(json);

  messageId++;
}

void handleEncoderButton(AceButton* button, uint8_t eventType, uint8_t buttonState) {
  switch (eventType) {
    case AceButton::kEventClicked: {
      // Cycle through color temperatures — only send to lights that are ON
      int temps[] = {2200, 2700, 4000, 6500};
      const char* names[] = {"2200K (candlelight)", "2700K (warm white)", "4000K (neutral)", "6500K (daylight)"};
      Serial.print("Color temp: ");
      Serial.println(names[colorTempMode]);

      bool anySent = false;
      if (studyLampOn) {
        sendWizColorTemp(STUDY_LAMP, brightness, temps[colorTempMode]);
        anySent = true;
      }
      if (uplightOn) {
        if (anySent) delay(50);
        sendWizColorTemp(UPLIGHT, brightness, temps[colorTempMode]);
      }

      colorTempMode = (colorTempMode + 1) % 4;
      break;
    }

    case AceButton::kEventDoubleClicked:
      // Toggle both lights on/off together
      bool anyOn = studyLampOn || uplightOn;
      studyLampOn = !anyOn;
      uplightOn = !anyOn;
      Serial.print("Encoder button double-click: Turn both lights ");
      Serial.println(!anyOn ? "ON" : "OFF");
      sendWizCommand(STUDY_LAMP, studyLampOn, brightness);
      delay(50);
      sendWizCommand(UPLIGHT, uplightOn, brightness);
      break;
  }
}

void handleStudyButton(AceButton* button, uint8_t eventType, uint8_t buttonState) {
  if (eventType == AceButton::kEventClicked) {
    // Toggle on single click
    studyLampOn = !studyLampOn;
    Serial.print("Study Lamp: ");
    Serial.println(studyLampOn ? "ON" : "OFF");
    sendWizCommand(STUDY_LAMP, studyLampOn, brightness);
  }
}

void handleUplightButton(AceButton* button, uint8_t eventType, uint8_t buttonState) {
  if (eventType == AceButton::kEventClicked) {
    // Toggle on single click
    uplightOn = !uplightOn;
    Serial.print("Uplight: ");
    Serial.println(uplightOn ? "ON" : "OFF");
    sendWizCommand(UPLIGHT, uplightOn, brightness);
  }
}
