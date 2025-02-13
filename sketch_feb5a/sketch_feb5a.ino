#define BLYNK_TEMPLATE_ID "TMPL6IS9XnOZf"
#define BLYNK_TEMPLATE_NAME "Energy Monitoring System"
#define BLYNK_AUTH_TOKEN "S-1vdRwHuxtae2cMRKg2RqZDftI_I325"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <EmonLib.h>
#include <LiquidCrystal_I2C.h>
#include <HardwareSerial.h>

char ssid[] = "koogs";
char pass[] = "unsaypass";
int readingCount;
#define LCD_I2C_ADDRESS 0x27
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, 16, 2);

EnergyMonitor emon;
const int sensorPin = 34;
const double voltage = 220.0;
double energyWh = 0.0;
double ratePerKWh = 0.0;
unsigned long lastTime = 0;
bool smsSent = false; // Flag to prevent multiple SMS

BlynkTimer timer; // Blynk timer for periodic updates

// SIM800L Serial Setup
HardwareSerial sim800l(1);
#define SIM800_TX 17
#define SIM800_RX 16

// Function to send an SMS
void sendSMS(String number, String message) {
  Serial.println("Sending SMS...");
  sim800l.println("AT+CMGF=1");  // Set SMS mode to text
  delay(1000);
  sim800l.print("AT+CMGS=\"");
  sim800l.print(number);
  sim800l.println("\"");
  delay(1000);
  sim800l.print(message);
  delay(1000);
  sim800l.write(26);  // End message with Ctrl+Z (ASCII 26)
  Serial.println("SMS Sent!");
}

// Function to handle incoming data from V4 (rate per kWh)
BLYNK_WRITE(V4) { 
  String rateString = param.asStr();
  ratePerKWh = rateString.toDouble();
}

// Function to send data to Blynk every 5 seconds
void sendDataToBlynk() {
  readingCount++;
  double Irms = emon.calcIrms(1480);
  if (Irms < 0.07) Irms = 0;

  double power = voltage * Irms;
  unsigned long currentTime = millis();
  unsigned long timeElapsed = currentTime - lastTime;
  lastTime = currentTime;

  double timeElapsedHours = timeElapsed / 3600000.0;
  energyWh += power * timeElapsedHours;
  double energyKWh = energyWh / 1000.0;
  double estimatedBill = energyKWh * ratePerKWh;

  if (readingCount < 5) {
    energyKWh = 0;
    Irms = 0;
    power = 0;
  }

  if (readingCount > 5) {
    // Send data to Blynk
    Blynk.virtualWrite(V0, Irms);
    Blynk.virtualWrite(V1, power);
    Blynk.virtualWrite(V2, String(energyKWh, 4));
    Blynk.virtualWrite(V3, String(estimatedBill, 2));
  }

  // Send SMS when energy reaches 1 kWh
    if (energyKWh >= 1.0 && !smsSent) {
      sendSMS("+639923380007", "Alert: Energy consumption reached 1 kWh! Shutting down electricty in 15 mins.");
    smsSent = true; // Prevent multiple SMS
  }

  // Serial Monitor Output
  Serial.print("Current: "); Serial.print(Irms);
  Serial.print(" A, Power: "); Serial.print(power);
  Serial.print(" W, Energy: "); Serial.print(energyKWh, 4);
  Serial.print(" kWh, Estimated Bill: "); Serial.print(estimatedBill, 2);
  Serial.println(" PHP");

  // Display on LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Current: ");
  lcd.print(Irms);
  lcd.print(" A");
  
  lcd.setCursor(0, 1);
  lcd.print("kWh: ");
  lcd.print(energyKWh, 4);
}

void setup() {
  Serial.begin(115200);
  
  // Initialize LCD
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Energy Monitor");
  delay(2000);
  
  // Initialize Current Sensor
  emon.current(sensorPin, 1.2);
  
  // Initialize SIM800L
  sim800l.begin(9600, SERIAL_8N1, SIM800_RX, SIM800_TX);
  delay(1000);
  Serial.println("Initializing SIM800L...");
  
  // Connect to WiFi & Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  
  // Set the timer to run every 5 seconds
  timer.setInterval(10000L, sendDataToBlynk);
}

void loop() {
  Blynk.run();
  timer.run(); // Runs the Blynk timer
}
