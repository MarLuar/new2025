#define BLYNK_TEMPLATE_ID "TMPL6IS9XnOZf"
#define BLYNK_TEMPLATE_NAME "Energy Monitoring System"
#define BLYNK_AUTH_TOKEN "S-1vdRwHuxtae2cMRKg2RqZDftI_I325"

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <EmonLib.h>
#include <LiquidCrystal_I2C.h>

char ssid[] = "koogs";
char pass[] = "unsaypass";

#define LCD_I2C_ADDRESS 0x27
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, 16, 2);

EnergyMonitor emon;
const int sensorPin = 34;
const double voltage = 220.0;
double energyWh = 0.0;
double ratePerKWh = 0.0;
unsigned long lastTime = 0;

BlynkTimer timer; // Blynk timer for periodic updates

// Function to handle incoming data from V4 (rate per kWh)
BLYNK_WRITE(V3) {
  String rateString = param.asStr();
  ratePerKWh = rateString.toDouble();
}

// Function to trigger when energy reaches 1 kWh
void energyReachedOneKWh() {
  // Perform the desired action when 1 kWh is reached
  Serial.println("Energy reached 1 kWh!");
  // Add your custom logic here, for example, activating a relay, sending a notification, etc.
  // You could also reset energyWh if you want to track subsequent 1 kWh intervals:
  energyWh = 0.0;  // Optionally reset energy for a new cycle
}

// Function to send data to Blynk every 5 seconds
void sendDataToBlynk() {
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

  // Check if energy consumption has reached 1 kWh
  if (energyKWh >= 1.0) {
    energyReachedOneKWh();  // Call the function when 1 kWh is reached
  }

  // Send data to Blynk
  Blynk.virtualWrite(V0, Irms);
  Blynk.virtualWrite(V1, power);
  Blynk.virtualWrite(V2, String(energyKWh, 4));
  Blynk.virtualWrite(V4, String(estimatedBill, 2));

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

  // Connect to WiFi & Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);

  // Set the timer to run every 10 seconds
  timer.setInterval(10000L, sendDataToBlynk);
}

void loop() {
  Blynk.run();
  timer.run(); // Runs the Blynk timer
}
