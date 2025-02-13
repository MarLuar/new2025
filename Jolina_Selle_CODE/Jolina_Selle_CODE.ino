#include <Servo.h>  // Include the Servo library
#include <Wire.h>
#include <RtcDS1302.h>

#define exhaustFan 4  
#define uvLight 6  
#define dropper 10  
#define buzzer 5  
#define diffuser 2

const int manualExhaustFan = A4;
const int manualUVLight = A5;
const int manualDropper = A3;
const int manualDiffuser = A2;

Servo myServo;
#define DS1302_IO 7
#define DS1302_CLK 9
#define DS1302_RST 8

// Servo angle
int lastAngle = 0;
bool firstPelletDropped = false;
uint8_t lastDropDay = 0;

unsigned long uvStartTime = 0;
bool uvActive = false;
unsigned long diffuserStartTime = 0;
bool diffuserActive = false;
unsigned long exhaustStartTime = 0;
bool exhaustActive = false;
unsigned long dropperStartTime = 0;
bool dropperActive = false;

ThreeWire myWire(DS1302_IO, DS1302_CLK, DS1302_RST);
RtcDS1302<ThreeWire> Rtc(myWire);

void setup() {
  myServo.attach(dropper);
  myServo.write(0);
  pinMode(exhaustFan, OUTPUT);
  pinMode(uvLight, OUTPUT);
  pinMode(buzzer, OUTPUT);
  pinMode(diffuser, OUTPUT); 
  pinMode(manualExhaustFan, INPUT_PULLUP);
  pinMode(manualDropper, INPUT_PULLUP);
  pinMode(manualDiffuser, INPUT_PULLUP);  
  pinMode(manualUVLight, INPUT_PULLUP); 
  digitalWrite(exhaustFan, HIGH);
  digitalWrite(uvLight, HIGH);  
  digitalWrite(diffuser, LOW);  // Turn ON exhaust fan
  delay(200);
  digitalWrite(diffuser, HIGH);  // Turn ON exhaust fan
  Serial.begin(9600);
  
  Rtc.Begin();
Rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));

  if (!Rtc.GetIsRunning()) {
    Serial.println("RTC is not running!");
    Rtc.SetDateTime(RtcDateTime(__DATE__, __TIME__));
  }

  Serial.println("RTC Module Started");
}

void loop() {
  RtcDateTime now = Rtc.GetDateTime();
  Serial.print("Current Time: ");
Serial.print(now.Year(), DEC);
Serial.print('/');
Serial.print(now.Month(), DEC);
Serial.print('/');
Serial.print(now.Day(), DEC);
Serial.print(" ");
Serial.print(now.Hour(), DEC);
Serial.print(':');
Serial.print(now.Minute(), DEC);
Serial.print(':');
Serial.println(now.Second(), DEC);
  static uint8_t lastHour = now.Hour();
  unsigned long currentMillis = millis();

  checkManualControls();

  if (now.Hour() == 0 && now.Minute() == 0 && now.Second() == 0) {   // mao ni
    Serial.println("Activating UV Light Sequence...");
    uvStartTime = currentMillis;
    uvActive = true;
    digitalWrite(uvLight, LOW);
    digitalWrite(buzzer, HIGH);
  }

  if (uvActive && currentMillis - uvStartTime >= 300000) {
    digitalWrite(uvLight, HIGH);
    digitalWrite(buzzer, LOW);
    uvActive = false;
    Serial.println("UV Light and Buzzer deactivated");
    if (!firstPelletDropped) {
      Serial.println("First Pellet Drop after UV Light...");
      dropperStartTime = currentMillis;
      dropperActive = true;
      firstPelletDropped = true;
      lastDropDay = now.Day();
    }
  }

  if (firstPelletDropped && (now.Day() != lastDropDay) && ((now.Day() - lastDropDay) % 3 == 0) && now.Hour() == 0 && now.Minute() == 5 && now.Second() == 10) {
    Serial.println("3-day Pellet Drop Sequence Activated...");
    dropperStartTime = currentMillis;
    dropperActive = true;
    lastDropDay = now.Day();
  }

  if (dropperActive && currentMillis - dropperStartTime >= 2000) {
    myServo.write(60);
    lastAngle += 60;
    if (lastAngle == 180) {
      myServo.write(0);
      lastAngle = 0;
    }
    Serial.println("Pellet dropped");
    dropperActive = false;
  }

  if ((now.Hour() % 3 == 0) && (now.Hour() != lastHour)) {
    Serial.println("Activating 3-hour Exhaust Fan Cycle...");
    digitalWrite(exhaustFan, HIGH);
    Serial.println("EXHAUST FAN OFF");
    digitalWrite(diffuser, LOW);   // Simulate button press
    delay(200);               // Hold for 200ms
    digitalWrite(diffuser, HIGH);  // Release button
    Serial.println("DIFFUSER ON");
    diffuserStartTime = currentMillis;
    diffuserActive = true;
    lastHour = now.Hour();
  }

  if (diffuserActive && currentMillis - diffuserStartTime >= 1200000) {
    digitalWrite(diffuser, LOW);   // Simulate button press
    delay(200);               // Hold for 200ms
    digitalWrite(diffuser, HIGH);  // Release button
    delay(200);
    Serial.println("Perfume Diffuser turned OFF");
    exhaustStartTime = currentMillis;
    exhaustActive = true;
    diffuserActive = false;
  }

  if (exhaustActive && currentMillis - exhaustStartTime >= 2700000) {
    digitalWrite(exhaustFan, LOW);
    Serial.println("Exhaust Fan turned ON");
    exhaustActive = false;
  }
}

void checkManualControls() {
  if (digitalRead(manualExhaustFan) == LOW) {
    digitalWrite(exhaustFan, !digitalRead(exhaustFan));
    Serial.println("Manual: Exhaust Fan toggled");
    delay(500);
  }

  if (digitalRead(manualUVLight) == LOW) {
    digitalWrite(uvLight, !digitalRead(uvLight)); 
    digitalWrite(buzzer, !digitalRead(uvLight));
    Serial.println("Manual: UV Light toggled");
    delay(500);
  }

  if (digitalRead(manualDropper) == LOW) {
    lastAngle += 60;
    if (lastAngle > 180) {
      lastAngle = 0;
    }
    myServo.write(lastAngle);
    Serial.print("Manual: Servo moved to ");
    Serial.println(lastAngle);
    delay(500);
  }

  if (digitalRead(manualDiffuser) == LOW) {
    digitalWrite(diffuser, LOW);   // Simulate button press
      delay(200);               // Hold for 200ms
      digitalWrite(diffuser, HIGH);  // Release button
    Serial.println("Manual: Diffuser toggled");
    delay(500);
  }
}
