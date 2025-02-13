#include <map>  // Include map for ESP32 (handles borrow/return logic)



#include <WiFi.h>
#include <HTTPClient.h>
#include <SPI.h>    
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define SS_PIN 5      // SDA (ESP32)
#define RST_PIN 4     // RST
#define LED_PIN 25    // LED indicator

#define I2C_ADDR 0x27  // LCD I2C Address (0x27 or 0x3F)
#define LCD_COLUMNS 16
#define LCD_ROWS 2

std::map<String, bool> assetStatus; // Tracks asset status: true = borrowed, false = returned

const char* ssid = "koogs";
const char* password = "unsaypass";
const char* scriptURL = "https://script.google.com/macros/s/AKfycbwUdbMdgR5rHVBYtF-hirqN1UFtKh9Obqxfv32MiXBv-iDmS45K2-oVE3NXEYhctHSe/exec"; 

MFRC522 mfrc522(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(I2C_ADDR, LCD_COLUMNS, LCD_ROWS);

String lastScannedID = "";
String lastScannedStudent = "";
bool waitingForAsset = false; 
bool assetScanned = false;
unsigned long assetScanStartTime = 0;

void setup() {
    Serial.begin(115200);
    SPI.begin();
    mfrc522.PCD_Init();
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);  // Ensure LED is OFF at start

    // Initialize LCD
    Wire.begin(21, 22);
    lcd.init();
    lcd.backlight();
    resetLCD();

    // Connect to WiFi
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nConnected to WiFi!");
}

String urlencode(String str) {
    String encoded = "";
    for (int i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        if (c == ' ') encoded += "%20";
        else if (c == '&') encoded += "%26";
        else if (c == '=') encoded += "%3D";
        else if (c == '?') encoded += "%3F";
        else encoded += c;
    }
    return encoded;
}

void sendToGoogleSheets(String assetName, String assetID, String studentName, String studentID, String action) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = String(scriptURL) + "?asset_name=" + urlencode(assetName) 
            + "&asset_id=" + urlencode(assetID)
            + "&student_name=" + urlencode(studentName) 
            + "&student_id=" + urlencode(studentID)
            + "&action=" + urlencode(action);

        Serial.println("Sending data to Google Sheets...");
        http.begin(url);
        int httpCode = http.GET();

        if (httpCode > 0) {
            String response = http.getString();
            Serial.println("Server Response: " + response);
        } else {
            Serial.println("Error sending data");
        }

        http.end();
    } else {
        Serial.println("WiFi not connected!");
    }
}

void loop() {
    // If an asset was scanned, check if 5 seconds have passed
    if (assetScanned && millis() - assetScanStartTime >= 5000) {
        resetLCD();
        assetScanned = false;
    }

    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;

    String uidString = "";
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        uidString += String(mfrc522.uid.uidByte[i], HEX);
    }

    Serial.print("Card UID: ");
    Serial.println(uidString);
    digitalWrite(LED_PIN, HIGH);  
    delay(500);
    digitalWrite(LED_PIN, LOW);  

    if (uidString.equalsIgnoreCase("c2cf4979")) {
        lastScannedStudent = "Mar Luar Igot";
        lastScannedID = "23233299";
        displayLCD(lastScannedStudent, "Scan Asset");
        waitingForAsset = true;
    } 

    else if (uidString.equalsIgnoreCase("62b1707a")) {
        lastScannedStudent = "Hannah Tan";
        lastScannedID = "24204398";
        displayLCD(lastScannedStudent, "Scan Asset");
        waitingForAsset = true;
    } 
    else if (uidString.equalsIgnoreCase("ad693fbf")) {
        lastScannedStudent = "Crisha Luga";
        lastScannedID = "24204638";
        displayLCD(lastScannedStudent, "Scan Asset");
        waitingForAsset = true;
    } 
    else if (uidString.equalsIgnoreCase("81723ebf")) {
        lastScannedStudent = "Lyka Fujita";
        lastScannedID = "24204737";
        displayLCD(lastScannedStudent, "Scan Asset");
        waitingForAsset = true;
    } 
    else if (uidString.equalsIgnoreCase("fb69efdf")) {
        lastScannedStudent = "Francis Cabanes";
        lastScannedID = "24205197";
        displayLCD(lastScannedStudent, "Scan Asset");
        waitingForAsset = true;
    } 
    else if (uidString.equalsIgnoreCase("1de2e913b1080") && waitingForAsset) {  
        processAssetScan("1", "Laptop", lastScannedStudent, lastScannedID);
    } 

    else if (uidString.equalsIgnoreCase("1de1e913b1080") && waitingForAsset) {  
        processAssetScan("2", "Projector", lastScannedStudent, lastScannedID);
    } 

    else if (uidString.equalsIgnoreCase("1de3e913b1080") && waitingForAsset) {  
        processAssetScan("3", "TV", lastScannedStudent, lastScannedID);
    } 

    else if (uidString.equalsIgnoreCase("1de4e913b1080") && waitingForAsset) {  
        processAssetScan("4", "HDMI Cable", lastScannedStudent, lastScannedID);
    } 

    else if (uidString.equalsIgnoreCase("1de5e913b1080") && waitingForAsset) {  
        processAssetScan("5", "Extension Cord", lastScannedStudent, lastScannedID);
    }

    else if (uidString.equalsIgnoreCase("1de6e913b1080") && waitingForAsset) {  
        processAssetScan("6", "Stapler", lastScannedStudent, lastScannedID);
    }
    else if (uidString.equalsIgnoreCase("1de7e913b1080") && waitingForAsset) {  
        processAssetScan("7", "Speaker", lastScannedStudent, lastScannedID);
    }   
    
    else {
        Serial.println("Unknown card.");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Unknown Card!");
        lcd.setCursor(0, 1);
        lcd.print("Try Again.");
        delay(2000);
        resetLCD();
    }

    mfrc522.PICC_HaltA();
}

void processAssetScan(String assetNumber, String assetName, String studentName, String studentID) {
    String action;

    // Toggle Borrowed/Returned status
    if (assetStatus[assetNumber]) {  
        action = "Returned";
        assetStatus[assetNumber] = false;  // Mark as returned
    } else {
        action = "Borrowed";
        assetStatus[assetNumber] = true;  // Mark as borrowed
    }

    digitalWrite(LED_PIN, HIGH);  
    delay(500);
    digitalWrite(LED_PIN, LOW);

    sendToGoogleSheets(assetName, assetNumber, studentName, studentID, action);

    displayLCD(assetName, action);

    lastScannedID = "";
    waitingForAsset = false;

    // Start countdown for LCD reset
    assetScanStartTime = millis();
    assetScanned = true;
}

void displayLCD(String line1, String line2) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(line1);
    lcd.setCursor(0, 1);
    lcd.print(line2);
}

void resetLCD() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Waiting for ID...");
}
