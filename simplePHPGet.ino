/* 
 *  Prototype to test connecting and sending data to a PHP service.  The PHP service I used was a MAMP running on the same Intranet as the cc3000
 *  Part of the code is based on the work done by Adafruit on the CC3000 chip & Marco Schwartz's Open Home Animation sketch: wifi_weather_station
 *  Margaret Johnson
 */

// Include required libraries
#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <string.h>
#include "utility/debug.h"

#include<stdlib.h>

// Define CC3000 chip pins
#define ADAFRUIT_CC3000_IRQ   3
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10

// Buffer for float to String conversion
// The conversion goes from a float value to a String with two numbers after the decimal.  That means a buffer of size 10 can accommodate a float value up to 999999.99 in order for the last entry to be \0
char buffer[10];

// WiFi network (change with your settings !)
#define WLAN_SSID       "myNetwork"
#define WLAN_PASS       "myPassword"
#define WLAN_SECURITY   WLAN_SEC_WPA2 // This can be WLAN_SEC_UNSEC, WLAN_SEC_WEP, WLAN_SEC_WPA or WLAN_SEC_WPA2

const unsigned long
dhcpTimeout     = 60L * 1000L, // Max time to wait for address from DHCP
connectTimeout  = 15L * 1000L, // Max time to wait for server connection
responseTimeout = 15L * 1000L; // Max time to wait for data from server

uint32_t   t;
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT,
SPI_CLOCK_DIV2);

// PHP's server IP, port, and repository (change with your settings !)
uint32_t ip = cc3000.IP2U32(192,168,1,5);
int port = 8888;
//MAKE SURE TO SET THIS TO THE FOLDER YOU ARE USING IN htdocs FOR THE SERVER SIDE FILES
String repository = "/wifiweather/";

Adafruit_CC3000_Client client;

void setup(void)
{


  Serial.begin(56700);

  Serial.print(F("Initializing..."));
  if(!cc3000.begin()) {
    Serial.println(F("failed. Check your wiring?"));
    return;
  }

  Serial.print(F("OK.\r\nConnecting to network..."));
  cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY);
  Serial.println(F("connected!"));

  Serial.print(F("Requesting address from DHCP server..."));
  for(t=millis(); !cc3000.checkDHCP() && ((millis() - t) < dhcpTimeout); delay(500)) {
    Serial.println("....waiting");
  }
  if(cc3000.checkDHCP()) {
    Serial.println(F("OK"));
  } 
  else {
    Serial.println(F("failed"));
    return;
  }
  //ADDED RANDOM NUMBER TO USE IN PLACE OF SENSOR READING
  randomSeed(analogRead(0));

}

void loop(void)
{
  //Marco's wifi_weather_station app read in humidity and temperature from a DHT sensor.  My goal is to isolate this prototype to connecting and sending data to a PHP service.  I'll use
  //random numbers to represent the humidity and temperature.
  float randomNumber = (float)random(10, 1001);
  // Transform to String
  String temperature = floatToString(randomNumber);
  randomNumber = (float)random(10, 1001);  
  String humidity = floatToString(randomNumber);
  Serial.println(temperature);
  Serial.println(humidity);
  //Open Socket
  Serial.println("...Connecting to server");
  t = millis();
  do {
    client = cc3000.connectTCP(ip, port);
  } 
  while((!client.connected()) &&
    ((millis() - t) < connectTimeout));
  // Send request
  if (client.connected()) {
    Serial.println("Connected"); 
    String request = "GET "+ repository + "sensor.php?temp=" + temperature + "&hum=" + humidity + " HTTP/1.0\r\nConnection: close\r\n\r\n";
    Serial.print("...Sending request:");
    Serial.println(request);
    send_request(request);
  } 
  else {
    Serial.println(F("Connection failed"));    
    return;
  }
  Serial.println("...Reading response");
  show_response();

  Serial.println(F("Cleaning up..."));
  Serial.println(F("...closing socket"));
  client.close();
  //wait some amount of time before sending temperature/humidity to the PHP service.
  delay(10000);

}
/*******************************************************************************
 * send_request
 ********************************************************************************/
bool send_request (String request) {
  // Transform to char
  char requestBuf[request.length()+1];
  request.toCharArray(requestBuf,request.length()); 
  // Send request
  if (client.connected()) {
    client.fastrprintln(requestBuf); 
  } 
  else {
    Serial.println(F("Connection failed"));    
    return false;
  }
  return true;
  free(requestBuf);
}
/*******************************************************************************
 * show_response
 ********************************************************************************/
void show_response() {
  Serial.println(F("-------------------------------------"));
  while (client.available()) {
    // Read answer and print to serial debug
    char c = client.read();
    Serial.print(c);
  }
}
/*******************************************************************************
 * floatToString()
 ********************************************************************************/
// Float to String conversion
String floatToString(float number) {
  //  dtostrf(floatVar, minStringWidthIncDecimalPoint, numVarsAfterDecimal, charBuf);
  dtostrf(number,5,2,buffer);
  return String(buffer);

}
/*******************************************************************************
 * timedRead()
 ********************************************************************************/
// comment from Adafruit's GeoLocation sketch:
// Read from client stream with a 5 second timeout.  Although an
// essentially identical method already exists in the Stream() class,
// it's declared private there...so this is a local copy.
char timedRead(void) {
  unsigned long start = millis();
  while((!client.available()) && ((millis() - start) < responseTimeout));
  return client.read();  // -1 on timeout
}

