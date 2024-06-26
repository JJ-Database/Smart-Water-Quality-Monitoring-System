#include <Wire.h>
#include <EEPROM.h>
#include <GravityTDS.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <SoftwareSerial.h>  // SoftwareSerial for ESP8266 communication

// Pin Definitions
#define ONE_WIRE_BUS 2           // Temperature sensor pin
#define TDS_SENSOR_PIN A1        // TDS sensor pin
#define PH_SENSOR_PIN A0         // pH sensor pin
#define TURBIDITY_SENSOR_PIN A2  // Turbidity sensor pin
#define RED_LED_PIN 8
#define GREEN_LED_PIN 9
#define BUZZER_PIN 10            // Reassigning buzzer to pin 10

// SoftwareSerial Setup
SoftwareSerial espSerial(5, 6);  // SoftwareSerial pins (RX, TX)

// OneWire and DallasTemperature Setup
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

// GravityTDS Setup
GravityTDS gravityTds;

// Turbidity Sensor Configuration
const int numReadings = 20;                  // Number of readings to average
int turbidityReadings[numReadings] = { 0 };  // Array to store last 20 turbidity readings
int turbidityReadIndex = 0;                  // Index for the current reading
float turbiditySum = 0;                      // Running total of the last 20 readings
float turbiditySquaredSum = 0;               // Running total of squared readings
float turbidityMean = 0;                     // Mean of the last 20 readings
float turbidityStdDev = 0;                   // Standard deviation of the last 20 readings

// Variables for pH, TDS, Temperature, and Turbidity
float calibration_offset = 26.80;
float volt_to_ph_slope = -5.70;
float temperature;
float tdsValue;
float phValue;
float turbidityValue;          // For composite score
float anomalyThreshold = 1.0;  // Anomaly threshold for z-score

// Device Address for Temperature Sensor
DeviceAddress insideThermometer;

// Function to calculate z-score
float calculateZScore(float score, float mean, float stdDev) {
  return (stdDev == 0) ? 0 : (score - mean) / stdDev;  // Avoid divide-by-zero
}

void setup() {
  Serial.begin(9600);       // Serial communication setup
  espSerial.begin(115200);  // Setup SoftwareSerial

  // Initialize pins
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
 // pinMode(BUZZER_PIN, OUTPUT);  // Initialize the new buzzer pin

  sensors.begin();  // Initialize temperature sensor
  if (!sensors.getAddress(insideThermometer, 0)) {
    //Serial.println("Unable to find address for Device 0");
  } else {
    sensors.setResolution(insideThermometer, 9);  // Set resolution
  }

  gravityTds.setPin(TDS_SENSOR_PIN);  // Set TDS sensor pin
  gravityTds.setAref(5.0);            // Analog reference voltage
  gravityTds.setAdcRange(1024);       // ADC range
  gravityTds.begin();                 // Initialize TDS sensor
}

void loop() {
  // Read sensors and update values
  sensors.requestTemperatures();                      // Request temperature
  temperature = sensors.getTempC(insideThermometer);  // Get temperature

  // Turbidity Sensor Readings
  int sensorValue = analogRead(TURBIDITY_SENSOR_PIN);  // Get current reading

  // Update running totals for last 20 readings
  turbiditySum -= turbidityReadings[turbidityReadIndex];
  turbiditySquaredSum -= pow(turbidityReadings[turbidityReadIndex], 2);

  // Store current reading in the array
  turbidityReadings[turbidityReadIndex] = sensorValue;

  // Update running totals
  turbiditySum += sensorValue;
  turbiditySquaredSum += pow(sensorValue, 2);

  // Move to the next index for the circular buffer
  turbidityReadIndex = (turbidityReadIndex + 1) % numReadings;

  // Calculate mean and standard deviation of the last 20 readings
  turbidityMean = turbiditySum / numReadings;
  float variance = (turbiditySquaredSum / numReadings) - pow(turbidityMean, 2);
  turbidityStdDev = sqrt(variance);

  // Calculate z-score for turbidity
  float zScore = calculateZScore(sensorValue, turbidityMean, turbidityStdDev);

  // pH Sensor Reading and Processing
  int sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(PH_SENSOR_PIN);  // Read pH sensor
    delay(30);                         // Short delay for stable readings
  }

  // Calculate the average voltage and pH
  float voltage = sum * 5.0 / 1024 / 10;                        // Convert to voltage
  phValue = volt_to_ph_slope * (voltage) + calibration_offset;  // Convert to pH

  // TDS Reading
  gravityTds.setTemperature(temperature);  // Set temperature for TDS
  gravityTds.update();                     // Update TDS readings
  tdsValue = gravityTds.getTdsValue();     // Get TDS value

  // Calculate composite water quality score
  float waterQualityScore = phValue + tdsValue + temperature + turbidityValue;

  // True if turbidity is more than or equal to 20, indicating clean water
  bool isClean = (sensorValue >= 28);

  // Determine if the data is anomalous based on the z-score and a predefined threshold
  bool isAnomalous = abs(zScore) > anomalyThreshold;

  // Send sensor data to Node Red
  Serial.print("{\"TDS\":");
  Serial.print(tdsValue);
  Serial.print(",\"Temperature\":");
  Serial.print(temperature);
  Serial.print(",\"Turbidity\":");
  Serial.print(sensorValue);
  Serial.print(",\"PH\":");
  Serial.print(phValue);
  Serial.print(",\"WaterQualityScore\":");
  Serial.print(waterQualityScore);
  Serial.print(",\"Anomalous\":");
  Serial.print(isAnomalous ? "true" : "false");
  Serial.print(",\"Cleanliness\":");
  Serial.print(isClean ? "true" : "false");
  Serial.print(",\"ZScore\":");
  Serial.print(zScore, 2);  // Print z-score with 2 decimal places
  Serial.println("}");

  // Control LEDs and buzzer based on anomaly status
  if (isAnomalous) {
    digitalWrite(RED_LED_PIN, HIGH);   // Turn on red LED
    digitalWrite(GREEN_LED_PIN, LOW);  // Turn off green LED
    tone(BUZZER_PIN, 10);            // Turn on buzzer
  } else {
    digitalWrite(RED_LED_PIN, LOW);     // Turn off red LED
    digitalWrite(GREEN_LED_PIN, HIGH);  // Turn on green LED
    
  }

  delay(1500);  // 1.5-second delay
}
