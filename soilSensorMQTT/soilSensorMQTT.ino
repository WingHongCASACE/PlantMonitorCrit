/*  
    This code is from Prof. D. Wilson CASA0014 - Plant Monitor Workshop step 9 (Sending Soil Data to MQTT)
    Changes in the uncommented part of this code is minimal, and is instructed by the workshop
    Part of the commented part of this code is individual effort for better understanding

    
    Get date and time - uses the ezTime library at https://github.com/ropg/ezTime -
    and then show data from a DHT22 on a web page served by the Huzzah and
    push data to an MQTT server - uses library from https://pubsubclient.knolleary.net

    Duncan Wilson
    
    May 2020
*/

//Explanation from respective github repo or arduino reference
#include <ESP8266WiFi.h>    //provides ESP8266 specific Wi-Fi routines for connecting to the network
#include <ESP8266WebServer.h> //a simple web server that knows how to handle HTTP requests such as GET and POST and can only support one simultaneous client
#include <ezTime.h>    //pronounced as "easy time", provides NTP network time lookups, extensive timezone support, formatted time and date strings, user events, millisecond precision and more      
#include <PubSubClient.h> //for sending and receiving MQTT messages
#include <DHT.h>      //a library for DHT series of low cost temperature/humidity sensors
#include <DHT_U.h>    // DHT Temperature & Humidity Unified Sensor Library

#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321; set the type (there is another DHT 11 sensor on the market)

// Sensors - DHT22 and Nails
uint8_t DHTPin = 12;        // on Pin 2 of the Huzzah;  uint8_t=a type of unsigned integer of length 8 bits, max=255
uint8_t soilPin = 0;      // ADC or A0 pin on Huzzah
float Temperature;
float Humidity;
int Moisture = 1; // initial value just in case web page is loaded before readMoisture called
int sensorVCC = 13;
int blueLED = 2;
DHT dht(DHTPin, DHTTYPE);   // Initialize DHT sensor.


// Wifi and MQTT
#include "arduino_secret.h" 
/*
**** please enter your sensitive data in the Secret tab/arduino_secrets.h
**** using format below
* 
* 
*Please note for security and privacy the information in arduino_secrets.h has been modified and thus
*not working. Please contact WH for further action.

#define SECRET_SSID "ssid name"
#define SECRET_PASS "ssid password"
#define SECRET_MQTTUSER "user name - eg student"
#define SECRET_MQTTPASS "password";

Create new tab (from top right corner)
 */

const char* ssid     = SECRET_SSID;
const char* password = SECRET_PASS;
const char* mqttuser = SECRET_MQTTUSER;
const char* mqttpass = SECRET_MQTTPASS;

ESP8266WebServer server(80); 
/*80 is the common plot for a computer to send and 
receive web client-based communication and messages from a webserver and 
is used to send and receive HTML pages/data (From techopedia)*/
const char* mqtt_server = "mqtt.cetools.org";
WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

// Date and time
Timezone GB;



void setup() {
  // Set up LED to be controllable via broker
  // Initialize the BUILTIN_LED pin as an output
  // Turn the LED off by making the voltage HIGH
  pinMode(BUILTIN_LED, OUTPUT);     
  digitalWrite(BUILTIN_LED, HIGH);  

  // Set up the outputs to control the soil sensor
  // switch and the blue LED for status indicator
  pinMode(sensorVCC, OUTPUT); 
  digitalWrite(sensorVCC, LOW);
  pinMode(blueLED, OUTPUT); 
  digitalWrite(blueLED, HIGH);

  // open serial connection for debug info
  Serial.begin(115200); //faster commuication than 9600; remember to set the serial monitor to match the channel
  delay(100);

  // start DHT sensor
  pinMode(DHTPin, INPUT);
  dht.begin();

  // run initialisation functions
  startWifi();
  startWebserver();
  syncDate();

  // start MQTT server
  client.setServer(mqtt_server, 1884);
  client.setCallback(callback);

}

void loop() {
  // handler for receiving requests to webserver; handler clients
  server.handleClient();

  if (minuteChanged()) { //bool; perform the reading and sending data when detected a change in minute
    readMoisture();
    sendMQTT();
    Serial.println(GB.dateTime("H:i:s")); 
    /* UTC.dateTime("l, d-M-y H:i:s.v T"); 
     * l=day of the week e.g. Sunday, d=day of the month 2digits leading zero,
     * M=first 3 letters of a month, y=last 2digits of the year,
     * H=24hour format of hour leading zero, i=minutes leading zero,
     * s=second leading zero, v=ms 3digits, T=timezone abbv*/   
  }
  
  client.loop();
}

void readMoisture(){
  
  // power the sensor
  digitalWrite(sensorVCC, HIGH);
  digitalWrite(blueLED, LOW);
  delay(100);
  // read the value from the sensor:
  Moisture = analogRead(soilPin);         
  //Moisture = map(analogRead(soilPin), 0,320, 0, 100);    // note: if mapping work out max value by dipping in water     
  //stop power 
  digitalWrite(sensorVCC, LOW);  
  digitalWrite(blueLED, HIGH);
  delay(100);
  Serial.print("Wet ");
  Serial.println(Moisture);   // read the value from the nails
}

void startWifi() {
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);      // ssid and password stored in secret and became varable (const)
  WiFi.begin(ssid, password);

  // check to see if connected and wait until you are
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void syncDate() {
  // get real date and time
  waitForSync();
  Serial.println("UTC: " + UTC.dateTime());
  GB.setLocation("Europe/London");
  Serial.println("London time: " + GB.dateTime());

}
void startWebserver() {
  // when connected and IP address obtained start HTTP server
  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);
  server.begin();
  Serial.println("HTTP server started");
}

void sendMQTT() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  Temperature = dht.readTemperature(); // Gets the values of the temperature
  snprintf (msg, 50, "%.1f", Temperature);
  Serial.print("Publish message for t: ");
  Serial.println(msg);
  client.publish("student/CA------/plant/uc------/temperature", msg); //changed the student code here

  Humidity = dht.readHumidity(); // Gets the values of the humidity
  snprintf (msg, 50, "%.0f", Humidity);
  Serial.print("Publish message for h: ");
  Serial.println(msg);
  client.publish("student/CA------/plant/uc------/humidity", msg);  //student code changed

  //Moisture = analogRead(soilPin);   // moisture read by readMoisture function
  snprintf (msg, 50, "%.0i", Moisture);
  Serial.print("Publish message for m: ");
  Serial.println(msg);
  client.publish("student/CA-------/plant/uc------/moisture", msg);   //student code changed

}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because it is active low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect with clientID, username and password
    if (client.connect(clientId.c_str(), mqttuser, mqttpass)) { //c_str() Converts the contents of a String as a C-style, null-terminated string
      Serial.println("connected");
      // ... and resubscribe
      client.subscribe("student/CA-------/plant/uc--------/inTopic");  //student code changed here
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/*when looking at the code I realized I have not tested/implemented this part, that I 
have not opened my port 80 to view the result*/
void handle_OnConnect() {
  Temperature = dht.readTemperature(); // Gets the values of the temperature
  Humidity = dht.readHumidity(); // Gets the values of the humidity
  server.send(200, "text/html", SendHTML(Temperature, Humidity, Moisture));
}

void handle_NotFound() {
  server.send(404, "text/plain", "Not found");
}

String SendHTML(float Temperaturestat, float Humiditystat, int Moisturestat) {
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr += "<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr += "<title>ESP8266 DHT22 Report</title>\n";
  ptr += "<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr += "body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\n";
  ptr += "p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n";
  ptr += "</style>\n";
  ptr += "</head>\n";
  ptr += "<body>\n";
  ptr += "<div id=\"webpage\">\n";
  ptr += "<h1>ESP8266 Huzzah DHT22 Report</h1>\n";

  ptr += "<p>Temperature: ";
  ptr += (int)Temperaturestat;
  ptr += " C</p>";
  ptr += "<p>Humidity: ";
  ptr += (int)Humiditystat;
  ptr += "%</p>";
  ptr += "<p>Moisture: ";
  ptr += Moisturestat;
  ptr += "</p>";
  ptr += "<p>Sampled on: ";
  ptr += GB.dateTime("l,");
  ptr += "<br>";
  ptr += GB.dateTime("d-M-y H:i:s T");
  ptr += "</p>";

  ptr += "</div>\n";
  ptr += "</body>\n";
  ptr += "</html>\n";
  return ptr;
}
