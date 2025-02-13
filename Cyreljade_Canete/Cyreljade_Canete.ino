#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>  // Include the Servo library
#include <DHT.h>  // Include the DHT library
#include <RtcDS1302.h>
//  ultrasonic sensor 13 is echo 6 is trig
// LCD configuration  
#define LCD_I2C_ADDRESS 0x27
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, 16, 2);
// Ultrasonic sensor configuration
#define TRIG_PIN 11  // Trig pin connected to D11
#define ECHO_PIN 10  // Echo pin connected to D10
// Button pins
const int btnUp = A2;      // Up button
const int btnDown = A3;    // Down button
const int btnSelect = A0;  // Select button
const int btnCancel = A1;  // Cancel button

// Relay and Servo configuration
const int relayPin = 2;  // Relay module connected to A0
const int servoPin = 6;  // Servo motor connected to D6
const int servoPin2 = 5; 
const int buzzer  = 3;
Servo feederServo;        // Create
Servo trapServo;
// DS1302 Pin Definitions
#define DS1302_IO 8
#define DS1302_CLK 7
#define DS1302_RST 9

unsigned long lastScheduleCheckTime = 0;  // Timestamp of the last schedule check
const unsigned long scheduleCheckInterval = 60000;  // 60 seconds (in milliseconds)
unsigned long feedingStartTime = 0;  // Timestamp for scheduled feeding
bool feedingInProgress = false;      // Flag to track if feeding is in progress
// Initialize RTC instance
ThreeWire myWire(DS1302_IO, DS1302_CLK, DS1302_RST); // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);

// DHT sensor configuration
#define DHTPIN 12         // Pin connected to DHT11 sensor
#define DHTTYPE DHT11     // DHT sensor type
DHT dht(DHTPIN, DHTTYPE); // Initialize DHT sensor


// Menu state
int menuIndex = 0;             // Current menu option (0 - Set a New Schedule, 1 - View Schedule, 2 - Manual Feeding, 3 - Reset Trap, 4 - Dispense Duration)
bool settingSchedule = false;  // If user is in the process of setting a schedule
int scheduleFrequency = 1;     // Number of schedules to be set
int scheduleHours[10];         // Array to store hours for each schedule (up to 10 schedules)
int scheduleMinutes[10];       // Array to store minutes for each schedule (up to 10 schedules)

// Dispense duration state
int dispenseDuration = 0;  // 0 = Water, 1 = Food
unsigned long dispenseTime = 0; // Dispense time in milliseconds

bool isIdle = true; // Flag to track whether the system is in idle mode
unsigned long lastUpdate = 0; // Timestamp for periodic updates
const unsigned long updateInterval = 500; // Update interval in milliseconds
unsigned long idleStartTime = 0;
const unsigned long idleTimeout = 7000;     // Timeout for inactivity (7 seconds)

// Debounce
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;  // Debounce delay in milliseconds
bool trapActivated = false;
// Function prototypes
bool isButtonPressed(int pin);
void updateMenu();
void resetToIdle();
void setSchedule();
void viewSchedule();
void setScheduleFrequency();
void setScheduleTime(int scheduleIndex);
void manualFeeding();
void resetTrap();  // Function to reset the trap
void setDispenseDuration(); // Function to set dispense duration
void setDispenseTime();  // Function to set dispense time
void updateIdleState();
void updateIdleDisplay();

void setup() {
  // Initialize LCD
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();
  dht.begin();
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(btnUp, INPUT_PULLUP);
  pinMode(btnDown, INPUT_PULLUP);
  pinMode(btnSelect, INPUT_PULLUP);
  pinMode(btnCancel, INPUT_PULLUP);
  pinMode(buzzer, OUTPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);
  digitalWrite(buzzer, LOW);
  feederServo.attach(servoPin);
  trapServo.attach(servoPin2);
  feederServo.write(0);  // Start servo at position 0
  trapServo.write(0);
  Rtc.Begin();
  
  // ---- Set RTC Time (Run Once, Then Comment This Line Out) ----
  
  
  // Initial idle state
  resetToIdle();
}


void loop() {
  unsigned long currentMillis = millis();
  checkScheduledFeeding();
  readUltrasonicSensor(); 
  if (feedingInProgress && (currentMillis - feedingStartTime >= dispenseTime)) {
    feedingInProgress = false;  // Reset feeding state
    resetToIdle();              // Return to idle state
  }
  // Check if the system is in idle state or menu
  // Update the idle state every second
  if (isIdle && (currentMillis - lastUpdate >= updateInterval)) {
    lastUpdate = currentMillis;
    updateIdleState();
    
  }

  // Automatically reset to idle if inactive for 7 seconds
  if (!isIdle && (currentMillis - idleStartTime >= idleTimeout)) {
    resetToIdle();
  }

  // Button presses for menu navigation
  if (isButtonPressed(btnUp)) {
    menuIndex = (menuIndex > 0) ? menuIndex - 1 : 4;
    updateMenu();
    idleStartTime = currentMillis;  // Reset idle timer on interaction
    isIdle = false;                // Exit idle mode
  }

  if (isButtonPressed(btnDown)) {
    menuIndex = (menuIndex < 4) ? menuIndex + 1 : 0;
    updateMenu();
    idleStartTime = currentMillis;  // Reset idle timer on interaction
    isIdle = false;                // Exit idle mode
  }

  if (isButtonPressed(btnSelect)) {
    if (menuIndex == 0) {
      setSchedule();
      } else if (menuIndex == 1) {
      viewSchedule();
    } else if (menuIndex == 2) {
      manualFeeding();
    } else if (menuIndex == 3) {
      resetTrap();
    } else if (menuIndex == 4) {
      setDispenseDuration();
    }
    idleStartTime = currentMillis;  // Reset idle timer on interaction
    isIdle = false;                // Exit idle mode
  }

  if (isButtonPressed(btnCancel)) {
    resetToIdle();
    idleStartTime = currentMillis;  // Reset idle timer on interaction
  }

  
}


void checkScheduledFeeding() {
  unsigned long currentMillis = millis();
  
  // Only check the schedule every 60 seconds
  if (currentMillis - lastScheduleCheckTime >= scheduleCheckInterval) {
    lastScheduleCheckTime = currentMillis;  // Update last check time
    
    // Get the current time
    RtcDateTime now = Rtc.GetDateTime();
    int currentHour = now.Hour();
    int currentMinute = now.Minute();

    // Check if there is a scheduled feeding
    for (int i = 0; i < scheduleFrequency; i++) {
      if (scheduleHours[i] == currentHour && scheduleMinutes[i] == currentMinute && !feedingInProgress) {
        // Schedule matches the current time, trigger feeding
        feedingStartTime = millis();  // Set the feeding start time
        feedingInProgress = true;     // Mark feeding as in progress
        manualFeeding();              // Call manual feeding function
        break;                        // Exit after the first match
      }
    }
  }
}

// Function to update the menu display
void updateMenu() {
  lcd.clear();  // Clear the display each time to avoid overlapping text
  
  // Only show two menu items at a time
  lcd.setCursor(0, 0);
  if (menuIndex == 0) {
    lcd.print("> Set Schedules");
  } else {
    lcd.print("  Set Schedules");
  }

  lcd.setCursor(0, 1);
  if (menuIndex == 1) {
    lcd.print("> View Schedule");
  } else {
    lcd.print("  View Schedule");
  }

  // Handle additional menu options by switching them out when needed
  if (menuIndex == 2) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("> Manual Feeding");
    lcd.setCursor(0, 1);
    lcd.print("  Reset Trap");
  } else if (menuIndex == 3) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("  Manual Feeding");
    lcd.setCursor(0, 1);
    lcd.print("> Reset Trap");
  } else if (menuIndex == 4) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("> Dispense Time");
  }
}

void manualFeeding() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Manual Feeding...");
  
  // Turn on the relay
  digitalWrite(relayPin, LOW);
  digitalWrite(buzzer, HIGH);

  // Move the servo to 180 degrees
  for (int pos = 0; pos <= 180; pos += 5) {
    feederServo.write(pos);
    delay(50);  // Small delay between movements
  }

  // Wait for 3 seconds
  delay(3000);

  // Turn off the relay and reset servo to 0 degrees
  digitalWrite(relayPin, HIGH);
  digitalWrite(buzzer, LOW);
  for (int pos = 180; pos >= 0; pos -= 5) {
    feederServo.write(pos);
    delay(50);  // Small delay between movements
  }

  // Display confirmation
  lcd.clear();
  lcd.setCursor(0, 0);
  
  lcd.print("Feeding Done!");
  delay(2000);
  resetToIdle();
}

// Function to reset the trap (empty function)
void resetTrap() {
  lcd.clear();
  trapServo.write(0);
  trapActivated = false;
  lcd.setCursor(0, 0);
  lcd.print("Resetting Trap...");
  delay(1000);  // Empty action for reset, just a placeholder
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Trap Reset Done!");
  delay(2000);
  resetToIdle();
}

// Function to set a new schedule
void setSchedule() {
  // First ask for the number of schedules to set
  setScheduleFrequency();

  // Set the schedule time for each frequency
  for (int i = 0; i < scheduleFrequency; i++) {
    setScheduleTime(i);
  }

  // After setting schedules, show confirmation and return to main menu
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Schedules Set!");
  delay(2000);
  resetToIdle();
}

// Function to view the schedule
void viewSchedule() {
  int scheduleIndex = 0;  // Start at the first schedule
  bool viewing = true;

  while (viewing) {
    lcd.setCursor(0, 0);
    lcd.print("Scheduled Times:");

    // Prepare the time string with leading zeros if necessary
    char timeString[6];  // HH:MM format
    sprintf(timeString, "%02d:%02d", scheduleHours[scheduleIndex], scheduleMinutes[scheduleIndex]);

    // Display current schedule
    lcd.setCursor(0, 1);
    lcd.print("Sched ");
    lcd.print(scheduleIndex + 1);
    lcd.print(":  ");
    lcd.print(timeString);  // Show the formatted time

    // Check for next and previous scroll buttons
    if (isButtonPressed(btnUp)) {
      scheduleIndex++;
      if (scheduleIndex >= scheduleFrequency) {
        scheduleIndex = 0;  // Wrap around if the user goes beyond the last schedule
      }
     
    }

    if (isButtonPressed(btnDown)) {
      scheduleIndex--;
      if (scheduleIndex < 0) {
        scheduleIndex = scheduleFrequency - 1;  // Wrap around to the last schedule
      }
     
    }

    // Exit when Select or Cancel is pressed
    if (isButtonPressed(btnSelect) || isButtonPressed(btnCancel)) {
      resetToIdle();
      viewing = false;  // Exit the schedule view
    }

    
  }
}

// Function to set the frequency of schedules
void setScheduleFrequency() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter Frequency:");

  // Let the user increment/decrement the frequency
  while (true) {
    lcd.setCursor(0, 1);
    lcd.print("Schedules: ");

    // Display frequency without leading zero or unwanted 10+ number
    if (scheduleFrequency < 10) {
      lcd.print(" ");                // Clear space for a single-digit number to avoid previous zero
      lcd.print(scheduleFrequency);  // Single digit without leading zero
    } else {
      lcd.print(scheduleFrequency);  // Display as usual for values from 10 upwards
    }

    if (isButtonPressed(btnUp)) {
      scheduleFrequency++;  // Increment frequency by 1
      if (scheduleFrequency > 10) {
        scheduleFrequency = 1;  // Wrap around to 1 if it exceeds 10
      }
      
    }

    if (isButtonPressed(btnDown)) {
      scheduleFrequency--;  // Decrement frequency by 1
      if (scheduleFrequency < 1) {
        scheduleFrequency = 10;  // Wrap around to 10 if it goes below 1
      }
      
    }

    if (isButtonPressed(btnSelect)) {
      break;  // Move to setting the times after selecting frequency
    }
  }
}

// Function to set hour and minute for each schedule
void setScheduleTime(int scheduleIndex) {
  int hour = 8;    // Default hour
  int minute = 0;  // Default minute
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Schedule ");
  lcd.print(scheduleIndex + 1);

  // Set Hour
  lcd.setCursor(0, 1);
  lcd.print("Set Hour : ");
  while (true) {
    lcd.setCursor(12, 1);
    lcd.print(hour);

    if (isButtonPressed(btnUp)) {
      hour = (hour + 1) % 24;  // Increment hour, but it won't go above 23 (wrap around)
      
    }

    if (isButtonPressed(btnDown)) {
      hour = (hour - 1 + 24) % 24;  // Decrement hour, but it won't go below 0 (wrap around)
      
    }

    if (isButtonPressed(btnSelect)) {
      scheduleHours[scheduleIndex] = hour;
      break;  // Proceed to setting the minute
    }
  }

  // Set Minute
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Schedule ");
  lcd.print(scheduleIndex + 1);
  lcd.setCursor(0, 1);
  lcd.print("Set Minute : ");

  while (true) {
    lcd.setCursor(12, 1);
    lcd.print(minute);

    if (isButtonPressed(btnUp)) {
      minute = (minute + 1) % 60;  // Increment minute, but it won't go above 59 (wrap around)
      
    }

    if (isButtonPressed(btnDown)) {
      minute = (minute - 1 + 60) % 60;  // Decrement minute, but it won't go below 0 (wrap around)
      
    }

    if (isButtonPressed(btnSelect)) {
      scheduleMinutes[scheduleIndex] = minute;
      break;  // Move to next schedule
    }
  }
}
void setDispenseDuration() {
  lcd.clear();
  
  

  // Choose Water or Food
  while (true) {
    lcd.setCursor(0, 0);
    if (dispenseDuration == 0) {
      lcd.print("> Water");
    } else {
      lcd.print("  Water");
    }
    
    if (dispenseDuration == 1) {
      lcd.setCursor(0, 1);
      lcd.print("> Food");
    } else {
      lcd.setCursor(0, 1);
      lcd.print("  Food");
    }
    
    if (isButtonPressed(btnUp)) {
      dispenseDuration = (dispenseDuration == 0) ? 1 : 0;
      
    }

    if (isButtonPressed(btnDown)) {
      dispenseDuration = (dispenseDuration == 0) ? 1 : 0;
      
    }

    if (isButtonPressed(btnSelect)) {
      setDispenseTime();  // Move to dispense time setting
      break;
    }

    if (isButtonPressed(btnCancel)) {
      resetToIdle();  // Exit if Cancel is pressed
      break;
    }
  }
}

// Function to set the dispense time
void setDispenseTime() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Dispense Time");

  // Let the user increment/decrement the dispense time
  while (true) {
    lcd.setCursor(0, 1);
    lcd.print("Seconds: ");
    lcd.print(dispenseTime / 1000);  // Show time in seconds

    if (isButtonPressed(btnUp)) {
      dispenseTime += 1000;  // Increment time by 1 second
      
    }

    if (isButtonPressed(btnDown)) {
      dispenseTime = (dispenseTime > 1000) ? dispenseTime - 1000 : 0;  // Decrement time, don't go below 0
      
    }

    if (isButtonPressed(btnSelect)) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Time Set!");
      delay(2000);
      resetToIdle();
      break;
    }

    if (isButtonPressed(btnCancel)) {
      resetToIdle();
      break;
    }
  }
}


// Function to check if a button is pressed (debounced)
bool isButtonPressed(int pin) {
  static unsigned long lastDebounceTime = 0;
  static int lastButtonState = HIGH;

  int currentButtonState = digitalRead(pin);
  if (currentButtonState == LOW && lastButtonState == HIGH && (millis() - lastDebounceTime) > debounceDelay) {
    lastDebounceTime = millis();
    lastButtonState = currentButtonState;
    return true;  // Button pressed
  }

  lastButtonState = currentButtonState;
  return false;
}

// Reset to idle state
void resetToIdle() {
  isIdle = true;  // Enter idle mode
  lastUpdate = millis();
  idleStartTime = millis();
  updateIdleDisplay();
}
void updateIdleDisplay() {
  lcd.clear();
  RtcDateTime now = Rtc.GetDateTime();

  lcd.setCursor(0, 0);
  char timeString[9]; // Format HH:MM:SS
  sprintf(timeString, "%02d:%02d:%02d", now.Hour(), now.Minute(), now.Second());
  lcd.print("Time: ");
  lcd.print(timeString);

  // Read temperature and humidity
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  lcd.setCursor(0, 1);
  if (isnan(temperature) || isnan(humidity)) {
    lcd.print("Sensor Error");
  } else {
    lcd.print("T:");
    lcd.print(temperature, 1);  // Display temperature with 1 decimal
    lcd.print("C H:");
    lcd.print(humidity, 1);    // Display humidity with 1 decimal
    lcd.print("%");
  }
}
void readUltrasonicSensor() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration * 0.034 / 2;
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.println(" cm");
  if (distance > 0 && distance < 20) { // Object detected within 40 cm
        if (!trapActivated) {
            trapServo.write(180); // Move second servo to 180° only once
            trapActivated = true;
        }
    }
}

void updateIdleState() {
  updateIdleDisplay();  // Refresh display in idle mode
}