// Define Blynk credentials
#define BLYNK_TEMPLATE_ID "TMPL6IS9XnOZf"
#define BLYNK_TEMPLATE_NAME "Energy Monitoring System"
#define BLYNK_AUTH_TOKEN "S-1vdRwHuxtae2cMRKg2RqZDftI_I325"

// Include required libraries
#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <EmonLib.h>
#include <LiquidCrystal_I2C.h>
#include <HardwareSerial.h>
#include <ESP32Servo.h>

// WiFi credentials
char ssid[] = "koogs";
char pass[] = "unsaypass";
int readingCount;

// LCD Display setup
#define LCD_I2C_ADDRESS 0x27
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, 16, 2);

// Energy monitoring setup
EnergyMonitor emon;
const int sensorPin = 34;
const double voltage = 220.0;
double energyWh = 0.0;
double ratePerKWh = 0.0;
unsigned long lastTime = 0;
bool smsSent = false;
unsigned long startupTime = 0;
unsigned long energyThresholdTime = 0;
bool timerStarted = false;
const unsigned long shutdownDelay = 15 * 60 * 1000; // 15 minutes in milliseconds

// SIM800L Serial Setup
HardwareSerial sim800l(1);
#define SIM800_TX 17
#define SIM800_RX 16

// Servo Motor Setup
Servo myServo;
const int servoPin = 33;

// Variables to store previous values for change detection
double prevIrms = 0.0;
double prevEnergyKWh = 0.0;
double prevEstimatedBill = 0.0;

// Function to send SMS using SIM800L module
void sendSMS(String number, String message) {
  Serial.println("Sending SMS...");
  sim800l.println("AT+CMGF=1"); // Set to text mode
  delay(1000);
  sim800l.print("AT+CMGS=\"");
  sim800l.print(number);
  sim800l.println("\"");
  delay(1000);
  sim800l.print(message);
  delay(1000);
  sim800l.write(26); // End SMS
  Serial.println("SMS Sent!");
}

// Blynk virtual pin V4 handler to update rate per kWh
BLYNK_WRITE(V4) { 
  String rateString = param.asStr();
  ratePerKWh = rateString.toDouble();
}

// Function to read energy data and send updates to Blynk
void sendDataToBlynk() {
  // Ignore initial unstable readings
  if (millis() - startupTime < 3000) {
    Serial.println("Ignoring startup readings...");
    return;
  }

  double Irms = emon.calcIrms(1480);
  if (Irms < 0.07) Irms = 0; // Ignore noise

  unsigned long currentTime = millis();
  unsigned long timeElapsed = currentTime - lastTime;
  lastTime = currentTime;

  double timeElapsedHours = timeElapsed / 3600000.0;
  energyWh += (voltage * Irms) * timeElapsedHours;
  double energyKWh = energyWh / 1000.0;
  double estimatedBill = energyKWh * ratePerKWh;

  // Update Blynk only if significant change is detected
  if ((abs(Irms - prevIrms) > 0.05) ||
      (abs(energyKWh - prevEnergyKWh) > 0.002) ||
      (abs(estimatedBill - prevEstimatedBill) > 0.02)) {

    Blynk.virtualWrite(V0, Irms);
    Blynk.virtualWrite(V2, String(energyKWh, 4));
    Blynk.virtualWrite(V3, String(estimatedBill, 2));

    prevIrms = Irms;
    prevEnergyKWh = energyKWh;
    prevEstimatedBill = estimatedBill;
  }

  // Check if energy consumption has reached 1 kWh
  if (energyKWh >= 1.0) {
    if (!smsSent) {
      sendSMS("+639923380007", "Alert: Energy consumption reached 1 kWh! Shutting down electricity in 15 mins.");
      smsSent = true;
    }
    if (!timerStarted) {
      energyThresholdTime = millis();
      timerStarted = true;
    }
  } else {
    timerStarted = false;
    smsSent = false;
  }

  // Check if shutdown delay has passed and activate servo
  if (timerStarted && (millis() - energyThresholdTime >= shutdownDelay)) {
    myServo.write(180); // Move servo to turn off power
  } else {
    myServo.write(0); // Keep power on
  }
}

// Function to update LCD display
void updateLCD() {
  double Irms = emon.calcIrms(1480);
  if (Irms < 0.07) Irms = 0;

  double power = voltage * Irms;
  double energyKWh = energyWh / 1000.0;

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Current: ");
  lcd.print(Irms);
  lcd.print(" A");
  
  lcd.setCursor(0, 1);
  lcd.print("kWh: ");
  lcd.print(energyKWh, 4);
}

// Setup function to initialize all components
void setup() {
  Serial.begin(115200);
  
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Energy Monitor");
  delay(2000);
  
  emon.current(sensorPin, 1.2);
  
  sim800l.begin(9600, SERIAL_8N1, SIM800_RX, SIM800_TX);
  delay(1000);
  Serial.println("Initializing SIM800L...");
  
  myServo.attach(servoPin);
  myServo.write(0);
  
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  
  startupTime = millis();
}

// Main loop function
void loop() {
  Blynk.run(); // Keep Blynk running
  sendDataToBlynk(); // Send sensor data
  updateLCD(); // Update display
  delay(500); // Small delay to avoid excessive processing
}
