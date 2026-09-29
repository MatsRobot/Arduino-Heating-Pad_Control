#include <Arduino.h>
#include <LiquidCrystal.h>
#include <math.h>

// --- Pin Definitions ---
#define ENCODER_CLK 2   // Interrupt 0
#define ENCODER_DT  3   // Interrupt 1
#define ENCODER_SW  10  // Rotary Pushbutton

#define LCD_RS      8   //
#define LCD_E       9   //
#define LCD_D4      4   //
#define LCD_D5      5   //
#define LCD_D6      6   //
#define LCD_D7      7   //

#define THERMISTOR_PIN A1 //
#define SSR_PIN        13 //

// --- Thermistor Constants (NTC 100K 3950) ---
const float SERIES_RESISTOR = 100000.0; // 100k ohm
const float NOMINAL_RESISTANCE = 100000.0;
const float NOMINAL_TEMPERATURE = 25.0;
const float B_COEFFICIENT = 3950.0;

// --- Calibration & Control Parameters ---
const float TEMP_OFFSET = 0.0;   // Calibration offset in °C (e.g., +1.5 or -2.0)
const float HYSTERESIS  = 1.0;   // Temperature control band in °C below target

// --- Control Variables ---
LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

volatile int targetTemp = 70; // Default target for LCD removal
const int MIN_TEMP = 30;
const int MAX_TEMP = 85;      // Safety ceiling below 90°C KSD301 cutoff

float currentTemp = 0.0;
bool heatingEnabled = false;

// Hardware Interrupt Routine for Encoder
void readEncoder() {
  static unsigned long lastInterruptTime = 0;
  unsigned long interruptTime = millis();
  
  // Debounce (5ms)
  if (interruptTime - lastInterruptTime > 5) {
    if (digitalRead(ENCODER_DT) == digitalRead(ENCODER_CLK)) {
      if (targetTemp < MAX_TEMP) targetTemp++;
    } else {
      if (targetTemp > MIN_TEMP) targetTemp--;
    }
  }
  lastInterruptTime = interruptTime;
}

// Read NTC thermistor temperature with calibration offset
float readThermistor() {
  int rawADC = analogRead(THERMISTOR_PIN);
  if (rawADC == 0) return 0.0; // Prevent divide by zero
  
  float reading = (1023.0 / rawADC) - 1.0;
  reading = SERIES_RESISTOR / reading;
  
  float steinhart;
  steinhart = reading / NOMINAL_RESISTANCE;            // (R/Ro)
  steinhart = log(steinhart);                         // ln(R/Ro)
  steinhart /= B_COEFFICIENT;                          // 1/B * ln(R/Ro)
  steinhart += 1.0 / (NOMINAL_TEMPERATURE + 273.15); // + (1/To)
  steinhart = 1.0 / steinhart;                        // Invert
  steinhart -= 273.15;                                // Convert to °C
  
  return steinhart + TEMP_OFFSET;                    // Apply calibration offset
}

void setup() {
  pinMode(ENCODER_CLK, INPUT_PULLUP);
  pinMode(ENCODER_DT, INPUT_PULLUP);
  pinMode(ENCODER_SW, INPUT_PULLUP);
  pinMode(SSR_PIN, OUTPUT);
  digitalWrite(SSR_PIN, LOW); // Start with SSR OFF

  // Attach Hardware Interrupt to ENCODER_DT for corrected CW/CCW direction
  attachInterrupt(digitalPinToInterrupt(ENCODER_DT), readEncoder, CHANGE);

  lcd.begin(16, 2);
  lcd.clear();
  lcd.print("   LCD HEATER   ");
  lcd.setCursor(0, 1);
  lcd.print("  INITIALIZING  ");
  delay(1500);
  lcd.clear();
}

void loop() {
  // Read Switch Button to Toggle Heating ON/OFF
  static bool lastBtnState = HIGH;
  bool btnState = digitalRead(ENCODER_SW);
  if (lastBtnState == HIGH && btnState == LOW) {
    heatingEnabled = !heatingEnabled; // Toggle state
    delay(50); // Simple debounce
  }
  lastBtnState = btnState;

  // Read current temperature
  currentTemp = readThermistor();

  // Control Logic using configurable HYSTERESIS band
  if (heatingEnabled) {
    if (currentTemp < (targetTemp - HYSTERESIS)) {
      digitalWrite(SSR_PIN, HIGH); // Heat ON
    } else if (currentTemp >= targetTemp) {
      digitalWrite(SSR_PIN, LOW);  // Heat OFF
    }
  } else {
    digitalWrite(SSR_PIN, LOW);    // System OFF
  }

  // Display Updates
  lcd.setCursor(0, 0);
  lcd.print("Pad T: ");
  lcd.print(currentTemp, 1);
  lcd.print((char)223); // Degree symbol
  lcd.print("C   ");

  lcd.setCursor(0, 1);
  lcd.print("Set: ");
  lcd.print(targetTemp);
  lcd.print((char)223);
  lcd.print("C ");

  if (heatingEnabled) {
    if (digitalRead(SSR_PIN) == HIGH) {
      lcd.print("[HEAT]");
    } else {
      lcd.print("[HOLD]");
    }
  } else {
    lcd.print("[OFF] ");
  }

  delay(200); // Display refresh rate
}