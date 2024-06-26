#include <ESP8266WiFi.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <SoftwareSerial.h>
#include <TimeLib.h>

const char* ssid = "littlesillypiggy";
const char* password = "tzuli0120";

const char* api_key = "AIzaSyBvYoWc5MfbmB8SgF9sJUL-RtDuMgYXJzM";
const char* db_url = "node-red-8a553-default-rtdb.firebaseio.com";

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool signupOK = false;
unsigned long sendDataPrevMillis = 0;

SoftwareSerial espSerial(D5, D6);  // RX, TX

void setup() {
  Serial.begin(9600);
  espSerial.begin(115200);

  // Initialize SPIFFS
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system");
    return;
  }

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wifi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected to WiFi");

  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  config.api_key = api_key;
  config.database_url = db_url;

  if (Firebase.signUp(&config, &auth, "", "")) {
    Serial.println("sign up ok");
    signupOK = true;
  } else {
    Serial.println(config.signer.signupError.message.c_str());
  }

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
}

void loop() {
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 2000 || sendDataPrevMillis == 0)) {
    sendDataPrevMillis = millis();

    float tdsValue = 0, ph = 0, waterQualityScore = 0, zScore = 0;
    int temperature = 0, turbidity = 0;
    String anomalous, cleanliness;

    String dataString = "";

    if (Firebase.RTDB.getFloat(&fbdo, "/waterQuality/TDS")) {
      tdsValue = fbdo.floatData();
      dataString += "TDS: " + String(tdsValue) + ", ";
    } else {
      Serial.println("FAILED to get TDS: " + fbdo.errorReason());
    }

    if (Firebase.RTDB.getInt(&fbdo, "/waterQuality/Temperature")) {
      temperature = fbdo.intData();
      dataString += "Temperature: " + String(temperature) + ", ";
    } else {
      Serial.println("FAILED to get Temperature: " + fbdo.errorReason());
    }

    if (Firebase.RTDB.getInt(&fbdo, "/waterQuality/Turbidity")) {
      turbidity = fbdo.intData();
      dataString += "Turbidity: " + String(turbidity) + ", ";
    } else {
      Serial.println("FAILED to get Turbidity: " + fbdo.errorReason());
    }

    if (Firebase.RTDB.getFloat(&fbdo, "/waterQuality/pH")) {
      ph = fbdo.floatData();
      dataString += "pH: " + String(ph) + ", ";
    } else {
      Serial.println("FAILED to get pH: " + fbdo.errorReason());
    }

    if (Firebase.RTDB.getFloat(&fbdo, "/waterQuality/waterQualityScore")) {
      waterQualityScore = fbdo.floatData();
      dataString += "Water Quality Score: " + String(waterQualityScore) + ", ";
    } else {
      Serial.println("FAILED to get Water Quality Score: " + fbdo.errorReason());
    }

    if (Firebase.RTDB.getFloat(&fbdo, "/waterQuality/zScore")) {
      zScore = fbdo.floatData();
      dataString += "Z-Score: " + String(zScore) + ", ";
    } else {
      Serial.println("FAILED to get Z-Score: " + fbdo.errorReason());
    }

    if (Firebase.RTDB.getBool(&fbdo, "/waterQuality/Anomalous")) {
      anomalous = fbdo.stringData();
      dataString += "Anomalous: " + String(anomalous) + ", ";
    } else {
      Serial.println("FAILED to get Anomalous: " + fbdo.errorReason());
    }

    if (Firebase.RTDB.getBool(&fbdo, "/waterQuality/Cleanliness")) {
      cleanliness = fbdo.stringData();
      dataString += "Cleanliness: " + String(cleanliness);
    } else {
      Serial.println("FAILED to get Cleanliness: " + fbdo.errorReason());
    }

    Serial.println(dataString);
  }
}
