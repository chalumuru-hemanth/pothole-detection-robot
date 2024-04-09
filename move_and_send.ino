#include <TinyGPS++.h>
#include <LiquidCrystal_I2C.h>

#include <Ultrasonic.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h> // Include the HTTPClient library

#define GPS_BAUDRATE 9600  // The default baudrate of NEO-6M is 9600
#define pingPin 13
#define echoPin 12
#define MAX_DISTANCE 200  // Maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500cm.

TinyGPSPlus gps;                          // the TinyGPS++ object
Ultrasonic ultrasonic(pingPin, echoPin);  // Create an Ultrasonic object

const char* ssid = "chalumuru_4G";
const char* password = "wxu8694@WX";

WebServer server(80);

// Motor control pins
#define IN_1 33  // L298N in1 motors Right          GPIO15(D1)
#define IN_2 32  // L298N in2 motors Right          GPIO13(D2)
#define IN_3 25  // L298N in3 motors Left           GPIO2(D3)
#define IN_4 26  // L298N in4 motors Left           GPIO2(D4)

WiFiServer wifiServer(80);

WiFiClient client;  // Assuming you are using WiFiClient for communication with the client

const char* googleScriptURL = "https://script.google.com/macros/s/AKfycbygdKvHjbeYV6ZzS-U_XlTSVHwwqcD_7yi2o2wIpN3KpBKmDiW_TJk-5H1_Bs8a5582/exec";

LiquidCrystal_I2C lcd(0x27, 16, 2);
void executeCommand(String command);

void setup() {
  Serial.begin(9600);

  pinMode(IN_1, OUTPUT);
  pinMode(IN_2, OUTPUT);
  pinMode(IN_3, OUTPUT);
  pinMode(IN_4, OUTPUT);


  lcd.init();         // Initialize the LCD
  lcd.backlight();    // Turn on the backlight
  lcd.clear();        // Clear the LCD screen

  lcd.setCursor(0, 0);
  lcd.print("Connecting to WiFi...");

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
    
  }
  Serial.println("Connected to WiFi");
  lcd.clear();
  lcd.print("Connected to WiFi");
  

  Serial.println(WiFi.localIP());
  lcd.setCursor(0, 1);
  lcd.print("IP");
  lcd.print(WiFi.localIP());
  delay(10000); // Display for 2 seconds

  wifiServer.begin();

  server.on("/", handleRoot);
  server.begin();
}

void loop() {
  server.handleClient();  // Handle incoming client connections

  // Check for incoming client connections
  WiFiClient client = wifiServer.available();
  if (client) {
    // Wait until the client sends some data
    while (client.connected() && !client.available()) {
      delay(1);
    }

    // Read the first line of the request
    String request = client.readStringUntil('\r');
    client.flush();

    // Extract the command from the request
    String command;
    if (request.indexOf("F") != -1) {
      command = "F";  // Move forward
    } else if (request.indexOf("B") != -1) {
      command = "B";  // Move backward
    } else if (request.indexOf("L") != -1) {
      command = "L";  // Turn left
    } else if (request.indexOf("R") != -1) {
      command = "R";  // Turn right
    } else if (request.indexOf("S") != -1) {
      command = "S";  // Stop
    }

    // Execute the command
    executeCommand(command);

    // Send HTTP response
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/plain");
    client.println();
    client.println("OK");

    // Close the connection
    client.stop();
  }

  // Measure the distance in centimeters
  long distance_cm = ultrasonic.distanceRead();

  lcd.clear();

  // Check if the road is clear
  if (distance_cm > 25) {
    // Handle GPS data if available
    while (Serial2.available()) {
      gps.encode(Serial2.read());
    }

    // If GPS data is valid, send it to Google Sheets
    if (gps.location.isValid()) {
      lcd.setCursor(0, 0);
      lcd.print("Lat: ");
      lcd.print(gps.location.lat(), 6);
      lcd.setCursor(0, 1);
      lcd.print("Lng: ");
      lcd.print(gps.location.lng(), 6);
      String timestamp = String(gps.time.hour()) + ":" + String(gps.time.minute()) + ":" + String(gps.time.second());
      // Construct the query parameters
      String url = googleScriptURL;
      url += "?Timestamp=";
      url += timestamp;
      url += "&Latitude=";
      url += String(gps.location.lat(), 6);
      url += "&Longitude=";
      url += String(gps.location.lng(), 6);
      url += "&Distance=";
      url += String(distance_cm);
      Serial.print(url);

      // Make HTTP GET request to Google Apps Script web app
      HTTPClient http;
      http.begin(url);
      int httpResponseCode = http.GET();

      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
      } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      http.end();
    }

    // Print GPS information
    if (gps.location.isValid()) {
      Serial.print("- latitude: ");
      Serial.println(gps.location.lat(), 6);
      client.print("- latitude: ");
      client.println(gps.location.lat(), 6);

      Serial.print("- longitude: ");
      Serial.println(gps.location.lng(), 6);
      client.print("- longitude: ");
      client.println(gps.location.lng(), 6);
    }

    if (gps.speed.isValid()) {
      Serial.print("- speed: ");
      Serial.print(gps.speed.kmph());
      Serial.println(" km/h");
    } else {
      Serial.println("- speed: INVALID");
    }

    if (gps.date.isValid() && gps.time.isValid()) {
      Serial.print("- GPS date&time: ");
      Serial.print(gps.date.year());
      Serial.print("-");
      Serial.print(gps.date.month());
      Serial.print("-");
      Serial.print(gps.date.day());
      Serial.print(" ");
      Serial.print(gps.time.hour());
      Serial.print(":");
      Serial.print(gps.time.minute());
      Serial.print(":");
      Serial.println(gps.time.second());
    } else {
      Serial.println("- GPS date&time: INVALID");
    }

    Serial.print("- Distance: ");
    Serial.print(distance_cm);
    Serial.println(" cm");
    Serial.println();
  } else {a
    lcd.setCursor(0, 0);
    lcd.print("Clean Road");
    Serial.println("Clean road");
    client.println("Clean Road");
  }
}

void executeCommand(String command) {
  if (command == "F") {
    // Move forward
    digitalWrite(IN_1, HIGH);
    digitalWrite(IN_2, LOW);
    digitalWrite(IN_3, HIGH);
    digitalWrite(IN_4, LOW);
  } else if (command == "B") {
    // Move backward
    digitalWrite(IN_1, LOW);
    digitalWrite(IN_2, HIGH);
    digitalWrite(IN_3, LOW);
    digitalWrite(IN_4, HIGH);
  } else if (command == "L") {
    // Turn left
    digitalWrite(IN_1, LOW);
    digitalWrite(IN_2, HIGH);
    digitalWrite(IN_3, HIGH);
    digitalWrite(IN_4, LOW);
  } else if (command == "R") {
    // Turn right
    digitalWrite(IN_1, HIGH);
    digitalWrite(IN_2, LOW);
    digitalWrite(IN_3, LOW);
    digitalWrite(IN_4, HIGH);
  } else if (command == "S") {
    // Stop
    digitalWrite(IN_1, LOW);
    digitalWrite(IN_2, LOW);
    digitalWrite(IN_3, LOW);
    digitalWrite(IN_4, LOW);
  }
}

void handleRoot() {
  // Measure the distance in centimeters
  long distance_cm = ultrasonic.distanceRead();

  // Check if the road is clear
  if (distance_cm <= 25) {
    server.send(200, "text/html", "Clean Road");
  } else {
    // Handle GPS data if available
    while (Serial2.available()) {
      gps.encode(Serial2.read());
    }

    // Print GPS information
    if (gps.location.isValid()) {
      String response = "Latitude: " + String(gps.location.lat(), 6) + "<br>";
      response += "Longitude: " + String(gps.location.lng(), 6);
      server.send(200, "text/html", response);
    } else {
      server.send(200, "text/html", "GPS data not available");
    }
  }
}
