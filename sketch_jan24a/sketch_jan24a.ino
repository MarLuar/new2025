#define BLYNK_TEMPLATE_ID "TMPL6OxX-rwWt"
#define BLYNK_TEMPLATE_NAME "zhello world"
#define BLYNK_AUTH_TOKEN "abTPrM5M65Ile3uWg2P-_FgFn5qTjKYP"

#include <MD_Parola.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <FS.h>  // Include the SPIFFS library

// WiFi credentials
char ssid[] = "koogs";
char pass[] = "unsaypass";

// LED Matrix Display Configuration
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 24
#define CLK_PIN D13
#define DATA_PIN D11
#define CS_PIN D10

MD_Parola P = MD_Parola(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// Define zones
#define ZONE_1 0
#define ZONE_1_DEVICES 4
#define ZONE_2 1
#define ZONE_3 2
#define ZONE_4 3
#define ZONE_5 4
#define ZONE_6 5
#define ZONE_2_DEVICES 4
#define ZONE_3_DEVICES 4
#define ZONE_4_DEVICES 4
#define ZONE_5_DEVICES 4
#define ZONE_6_DEVICES 4

// Scores and fouls
int scores[6][2] = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };


// Persistent text for Zone 2
String zone2Text = "Game Details";
int selectedFile = 1;  // Default to game1.csv

// Timer variables for Zone 4
int timerMinutes = 10;                // Starting minutes
int timerSeconds = 0;                 // Starting seconds
unsigned long previousMillis = 0;     // For non-blocking delay
const unsigned long interval = 1000;  // 1 second interval
bool timerPaused = false;             // Timer pause state

// Timeout LED control
const int timeoutLED = D2;
bool timeoutLEDState = false;
unsigned long timeoutLEDTimestamp = 0;
const unsigned long timeoutLEDInterval = 3000;  // 3 seconds

// Blynk Timer
BlynkTimer timer;

// Debounce variables
unsigned long lastButtonPressTime = 0;
const unsigned long debounceDelay = 200;  // 200 ms debounce delay

// Update display for a zone
void updateZoneDisplay(uint8_t zone) {
  char scoreText[8];  // Format: "X : X"
  sprintf(scoreText, "%d:%d", scores[zone][0], scores[zone][1]);
  P.displayZoneText(zone, scoreText, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
}

// Function to save data to SPIFFS
void saveDataToSPIFFS() {
  // Construct the filename based on selectedFile
  String filename = "/game" + String(selectedFile) + ".csv";
  File file = SPIFFS.open(filename, "w");

  if (!file) {
    Serial.println("Failed to open file for writing");
    return;
  }

  // Write header
  file.println("Zone,Score1,Score2,Game Details,Fouls");

  // Write data for each zone
  for (int i = 0; i < 6; i++) {
    file.printf("%d,%d,%d,%s\n", i + 1, scores[i][0], scores[i][1], (i == ZONE_2 ? zone2Text.c_str() : ""));
  }

  file.close();
  Serial.println("Data saved to " + filename);

  // Read and display file contents for confirmation
  file = SPIFFS.open(filename, "r");
  if (file) {
    Serial.println("--- Saved Game Data ---");
    while (file.available()) {
      Serial.write(file.read());
    }
    Serial.println("\n-----------------------");
    file.close();
  } else {
    Serial.println("Failed to open file for reading");
  }
}

void loadFileContentsToZones() {
  String filename = "/game" + String(selectedFile) + ".csv";
  if (!SPIFFS.exists(filename)) {
    Serial.println("File does not exist: " + filename);
    return;
  }

  File file = SPIFFS.open(filename, "r");
  if (!file) {
    Serial.println("Failed to open file: " + filename);
    return;
  }

  Serial.println("Loading file: " + filename);

  // Skip header
  String header = file.readStringUntil('\n');
  Serial.println("Header: " + header);

  int zoneIndex = 0;
  while (file.available() && zoneIndex < 6) {
    String line = file.readStringUntil('\n');
    Serial.println("Line: " + line);

    int comma1 = line.indexOf(',');
    int comma2 = line.indexOf(',', comma1 + 1);
    int comma3 = line.indexOf(',', comma2 + 1);

    if (comma1 == -1 || comma2 == -1 || comma3 == -1) {
      Serial.println("Invalid line format, skipping...");
      continue;
    }

    int zone = line.substring(0, comma1).toInt();
    int score1 = line.substring(comma1 + 1, comma2).toInt();
    int score2 = line.substring(comma2 + 1, comma3).toInt();
    String details = line.substring(comma3 + 1);

    zoneIndex = zone - 1;  // Convert to 0-based index
    if (zoneIndex >= 0 && zoneIndex < 6) {
      scores[zoneIndex][0] = score1;
      scores[zoneIndex][1] = score2;

      if (zone == ZONE_2 + 1) {  // Update Zone 2 details
        zone2Text = details;
      }
      
      updateZoneDisplay(zoneIndex);  // Update the display for the zone
    }
  }

  file.close();

  // Reset the timer to 10:00
  timerMinutes = 10;
  timerSeconds = 0;
  updateZone4Display();  // Ensure the timer display is updated

  // Update Zone 2 text display
  updateZone2Display(zone2Text.c_str());
  
  Serial.println("File loaded and displays updated. Timer reset to 10:00.");
}

void deleteSelectedFile() {
  // Construct the filename based on selectedFile
  String filename = "/game" + String(selectedFile) + ".csv";

  // Check if the file exists
  if (SPIFFS.exists(filename)) {
    // Delete the file
    if (SPIFFS.remove(filename)) {
      Serial.println("File " + filename + " deleted successfully.");
    } else {
      Serial.println("Failed to delete file " + filename);
    }
  } else {
    Serial.println("File " + filename + " does not exist.");
  }
}

// Blynk Virtual Pin V19 handler (delete file button)
BLYNK_WRITE(V19) {
  int buttonState = param.asInt();  // Get the button state
  if (buttonState == 1) {          // Button pressed
    deleteSelectedFile();
  }
}


BLYNK_WRITE(V8) {
  int fileNumber = param.asInt();  // Get the integer value from Blynk
  if (fileNumber >= 1 && fileNumber <= 10) {
    selectedFile = fileNumber;  // Update the selected file
    Serial.println("Selected file: game" + String(selectedFile) + ".csv");
    loadFileContentsToZones();  // Load and display the file contents
  } else {
    Serial.println("Invalid file number! Must be between 1 and 10.");
  }
}

// Blynk Virtual Pin V0 handler (increment zone1 score1)
BLYNK_WRITE(V0) {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPressTime > debounceDelay) {
    lastButtonPressTime = currentTime;
    if (param.asInt() == 1) {  // If V0 is HIGH
      scores[ZONE_1][0]++;
      updateZoneDisplay(ZONE_1);
    }
  }
}
// Blynk Virtual Pin V1 handler (decrement zone1 score1)
BLYNK_WRITE(V1) {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPressTime > debounceDelay) {
    lastButtonPressTime = currentTime;
    if (param.asInt() == 1) {  // If V1 is HIGH
      scores[ZONE_1][0]--;     // Decrement zone1 score1
      updateZoneDisplay(ZONE_1);
    }
  }
}
BLYNK_WRITE(V2) {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPressTime > debounceDelay) {
    lastButtonPressTime = currentTime;
    if (param.asInt() == 1) {  // If V2 is HIGH
      scores[ZONE_1][1]++;     // Increment zone1 score2
      updateZoneDisplay(ZONE_1);
    }
  }
}

// Blynk Virtual Pin V3 handler (decrement zone1 score2)
BLYNK_WRITE(V3) {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPressTime > debounceDelay) {
    lastButtonPressTime = currentTime;
    if (param.asInt() == 1) {  // If V3 is HIGH
      scores[ZONE_1][1]--;     // Decrement zone1 score2
      updateZoneDisplay(ZONE_1);
    }
  }
}
// Blynk Virtual Pin V4 handler (increment zone2 score1)
BLYNK_WRITE(V4) {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPressTime > debounceDelay) {
    lastButtonPressTime = currentTime;
    if (param.asInt() == 1) {  // If V4 is HIGH
      scores[ZONE_3][0]++;     // Increment zone2 score1
      updateZoneDisplay(ZONE_3);
    }
  }
}

// Blynk Virtual Pin V5 handler (decrement zone2 score1)
BLYNK_WRITE(V5) {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPressTime > debounceDelay) {
    lastButtonPressTime = currentTime;
    if (param.asInt() == 1) {  // If V5 is HIGH
      scores[ZONE_3][0]--;     // Decrement zone2 score1
      updateZoneDisplay(ZONE_3);
    }
  }
}

// Blynk Virtual Pin V6 handler (increment zone2 score2)
BLYNK_WRITE(V6) {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPressTime > debounceDelay) {
    lastButtonPressTime = currentTime;
    if (param.asInt() == 1) {  // If V6 is HIGH
      scores[ZONE_3][1]++;     // Increment zone2 score2
      updateZoneDisplay(ZONE_3);
    }
  }
}

// Blynk Virtual Pin V7 handler (decrement zone2 score2)
BLYNK_WRITE(V7) {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPressTime > debounceDelay) {
    lastButtonPressTime = currentTime;
    if (param.asInt() == 1) {  // If V7 is HIGH
      scores[ZONE_3][1]--;     // Decrement zone2 score2
      updateZoneDisplay(ZONE_3);
    }
  }
}
BLYNK_WRITE(V11) {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPressTime > debounceDelay) {
    lastButtonPressTime = currentTime;
    if (param.asInt() == 1) {  // If V6 is HIGH
      scores[ZONE_5][0]++;     // Increment zone2 score2
      updateZoneDisplay(ZONE_5);
    }
  }
}

BLYNK_WRITE(V12) {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPressTime > debounceDelay) {
    lastButtonPressTime = currentTime;
    if (param.asInt() == 1) {  // If V7 is HIGH
      scores[ZONE_5][0]--;     // Decrement zone2 score2
      updateZoneDisplay(ZONE_5);
    }
  }
}
BLYNK_WRITE(V13) {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPressTime > debounceDelay) {
    lastButtonPressTime = currentTime;
    if (param.asInt() == 1) {  // If V6 is HIGH
      scores[ZONE_5][1]++;     // Increment zone2 score2
      updateZoneDisplay(ZONE_5);
    }
  }
}

BLYNK_WRITE(V14) {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPressTime > debounceDelay) {
    lastButtonPressTime = currentTime;
    if (param.asInt() == 1) {  // If V7 is HIGH
      scores[ZONE_5][1]--;     // Decrement zone2 score2
      updateZoneDisplay(ZONE_5);
    }
  }
}
BLYNK_WRITE(V15) {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPressTime > debounceDelay) {
    lastButtonPressTime = currentTime;
    if (param.asInt() == 1) {  // If V6 is HIGH
      scores[ZONE_6][0]++;     // Increment zone2 score2
      updateZoneDisplay(ZONE_6);
    }
  }
}

BLYNK_WRITE(V16) {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPressTime > debounceDelay) {
    lastButtonPressTime = currentTime;
    if (param.asInt() == 1) {  // If V7 is HIGH
      scores[ZONE_6][0]--;     // Decrement zone2 score2
      updateZoneDisplay(ZONE_6);
    }
  }
}
BLYNK_WRITE(V17) {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPressTime > debounceDelay) {
    lastButtonPressTime = currentTime;
    if (param.asInt() == 1) {  // If V6 is HIGH
      scores[ZONE_6][1]++;     // Increment zone2 score2
      updateZoneDisplay(ZONE_6);
    }
  }
}

BLYNK_WRITE(V18) {
  unsigned long currentTime = millis();
  if (currentTime - lastButtonPressTime > debounceDelay) {
    lastButtonPressTime = currentTime;
    if (param.asInt() == 1) {  // If V7 is HIGH
      scores[ZONE_6][1]--;     // Decrement zone2 score2
      updateZoneDisplay(ZONE_6);
    }
  }
}

// Blynk Virtual Pin V9 handler (save data)
BLYNK_WRITE(V9) {
  if (param.asInt() == 1) {  // If switch is ON
    timerPaused = true;  // Pause the timer
    timeoutLEDTimestamp = millis();
    digitalWrite(timeoutLED, HIGH);  // Turn on the LED
    P.setPause(ZONE_4, 0);  // Ensure no delay
    P.displayZoneText(ZONE_4, "STOP", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    P.displayAnimate();
    saveDataToSPIFFS();
  } else {  // If switch is OFF
    timerPaused = false;  // Resume the timer
    timeoutLEDTimestamp = millis();
    digitalWrite(timeoutLED, HIGH);  // Turn on the LED
    updateZone4Display();  // Update timer display
  }
}

BLYNK_WRITE(V10) {
  String newText = param.asString();  // Get the string from Blynk V10
  zone2Text = newText;  // Update the persistent text
  updateZone2Display(zone2Text.c_str());  // Update the display with the new text
}

// Function to update Zone 2 display (scrolling)
void updateZone2Display(const char* text) {
  P.displayZoneText(ZONE_2, text, PA_CENTER, 100, 100, PA_SCROLL_LEFT, PA_SCROLL_LEFT);
  P.displayReset(ZONE_2);  // Reset animation to loop continuously
}

void setupZone(uint8_t zone, uint8_t startDevice, uint8_t endDevice, const char* text,
               textPosition_t position, uint16_t speed, uint16_t pause,
               textEffect_t effectIn, textEffect_t effectOut) {
  P.setZone(zone, startDevice, endDevice);
  P.displayZoneText(zone, text, position, speed, pause, effectIn, effectOut);
}

void setupZones() {
  // Zone 1 (static, scores)
  setupZone(ZONE_1, 0, ZONE_1_DEVICES - 1, "0 : 0", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);

  // Zone 2 (scrolling, persistent text)
  setupZone(ZONE_2, ZONE_1_DEVICES, ZONE_1_DEVICES + ZONE_2_DEVICES - 1, zone2Text.c_str(), PA_CENTER, 100, 100, PA_SCROLL_LEFT, PA_SCROLL_LEFT);

  // Zone 3 (static, scores)
  setupZone(ZONE_3, ZONE_1_DEVICES + ZONE_2_DEVICES, ZONE_1_DEVICES + ZONE_2_DEVICES + ZONE_3_DEVICES - 1,
            "0 : 0", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);

  // Zone 5 initialized with timer (was Zone 4 previously)
  setupZone(ZONE_5, ZONE_1_DEVICES + ZONE_2_DEVICES + ZONE_3_DEVICES,
            ZONE_1_DEVICES + ZONE_2_DEVICES + ZONE_3_DEVICES + ZONE_5_DEVICES - 1,
            "0 : 0", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);

  // Zone 4 (static, scores, was Zone 5 previously)
  setupZone(ZONE_4, ZONE_1_DEVICES + ZONE_2_DEVICES + ZONE_3_DEVICES + ZONE_5_DEVICES,
            ZONE_1_DEVICES + ZONE_2_DEVICES + ZONE_3_DEVICES + ZONE_5_DEVICES + ZONE_4_DEVICES - 1,
            "0 : 0", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);

  // Zone 6 (static, scores)
  setupZone(ZONE_6, ZONE_1_DEVICES + ZONE_2_DEVICES + ZONE_3_DEVICES + ZONE_5_DEVICES + ZONE_4_DEVICES,
            MAX_DEVICES - 1, "0 : 0", PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
}


void setup() {
  delay(500);
  
  Serial.begin(115200);
  delay(500);
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount SPIFFS");
    return;
  }
  delay(500);
  P.begin(6);  // Initialize six zones
  delay(500);
  setupZones();
  delay(500);
  P.displayAnimate();
  pinMode(timeoutLED, OUTPUT);
  digitalWrite(timeoutLED, LOW);

  // Connect to WiFi and Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
}

void loop() {
  if (P.displayAnimate()) {
    if (P.getZoneStatus(ZONE_2)) {
      P.displayReset(ZONE_2);  // Continuously reset Zone 2 for scrolling
    }
  }

  unsigned long currentMillis = millis();

  // Timer countdown logic (only runs when timer is not paused)
  if (!timerPaused && currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Decrement timer
    if (timerSeconds == 0) {
      if (timerMinutes > 0) {
        timerMinutes--;
        timerSeconds = 59;
      }
    } else {
      timerSeconds--;
    }

    // Update Zone 4
    updateZone4Display();
  }

  if (digitalRead(timeoutLED) == HIGH && (millis() - timeoutLEDTimestamp >= timeoutLEDInterval)) {
    digitalWrite(timeoutLED, LOW);  // Turn off the LED
  }

  Blynk.run();
  timer.run();
}

void updateZone4Display() {
  if (!timerPaused) {  // Only update if the timer is not paused
    char timerText[10];
    sprintf(timerText, "%02d:%02d", timerMinutes, timerSeconds);
    P.setPause(ZONE_4, 0);  // Ensure no delay
    P.displayZoneText(ZONE_4, timerText, PA_CENTER, 0, 0, PA_PRINT, PA_NO_EFFECT);
    
  }
} 
