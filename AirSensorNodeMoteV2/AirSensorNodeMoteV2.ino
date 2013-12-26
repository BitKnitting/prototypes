/*****************************************************
 * Initial version of the Air Sensor Node
 * assumes a circuit that includes:
 *  - Moteino (433MHz, RFM69HW)
 *  - DHT22 + 10K resistor
 *  - TCS34725
 *  - RGB 5mm LED + 3 330 ohm resistors
 * Copyright M Johnson 12/2013
 ******************************************************/
#include <Narcoleptic.h>
//Turn on/off debug statements by setting DEBUG to 1 BEFORE including debugHelpers.h
#define DEBUG 1
#include <debugHelpers.h>
#define SERIAL_BAUD   115200
typedef enum State{  
  LOW_SIGNAL_STRENGTH,
  NOT_WORKING_SENSORS,
  DATA_NOT_SENT,
  WORKING
} 
State;
State state;
const uint8_t redPin = 3;
const uint8_t greenPin = 5;
const uint8_t bluePin = 6;
/******************************************************************************
 * Start of initializing
 *******************************************************************************/
/*****************************************************
 * RFM69
 ******************************************************/
#include <SPI.h>
#include <SPIFlash.h>
#include <RFM69.h>
#define NODEID        2    //unique for each node on same network
#define NETWORKID     100  //the same on all nodes that talk to each other
#define BASESYSTEMID  1
#define FREQUENCY   RF69_433MHZ
RFM69 radio;
const uint8_t SEND_RETRIES = 5;
//amount of time to wait between sensor readings
const uint16_t  MS_DELAY=5000;
/*****************************************************
 * Let the sketch know the number of sensor readings
 * Each reading takes 2 bytes.  The first byte lets the rx know the
 * total number of sensor readings.  V1 has 4 readings: Humidity, Air Temp, LUX, and Color Temp
 ******************************************************/
const byte  NUMSENSORREADINGS=4;
unsigned char sensorReadings[NUMSENSORREADINGS*2+1] = {
  0};
/*****************************************************
 * DHT22 Temp and Humidity Sensor
 ******************************************************/
#include <DHT.h>
const byte DHTPIN=7;
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
/*****************************************************
 * TCS34725 light sensor
 ******************************************************/
//I2C library
#include <Wire.h>
#include "Adafruit_TCS34725.h"
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_700MS, TCS34725_GAIN_1X);
void setup() { 
#if DEBUG
  Serial.begin(SERIAL_BAUD);
#endif
  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);
  DEBUG_PRINTLNF("************** SensorNodeMoteV1 ***************");
  DEBUG_PRINTF("****>>>>Free Ram: ");
  DEBUG_PRINTLN(getFreeRam());
  DEBUG_PRINTF("****>>>>Sketch Size: ");
  DEBUG_PRINT(sketchSize());
  DEBUG_PRINTLNF("");
  //blink blue
  blink(0,0,255,5,50);
  radio.initialize(FREQUENCY,NODEID,NETWORKID);
  radio.setHighPower();
  //blink green
  blink(0,255,0,5,50);
  //turn LED off
  setColor(0,0,0);
  //at least start the loop if we got this far
  /******************************************************************************
   * Set the state of the device to WORKING.
   *******************************************************************************/
  state = WORKING;
  /******************************************************************************
   * End of initializing
   *******************************************************************************/
}
void loop() {
  /******************************************************************************
   * Check to see if the battery level is low.  If it is, give a visual clue through the RGB.
   *******************************************************************************/
  //*****>>>>>TBD: check battery level
  /******************************************************************************
   * Read sensors and send readings to the Base System.
   *******************************************************************************/
  doTasks();
  if (state == NOT_WORKING_SENSORS) {
    //LED = aqua: Failed to get sensor readings
    blink(0,255,255,2,50);
  }
  else if (state == DATA_NOT_SENT) {
    //blink purple: Failed to find the Base System
    blink(128,0,128,2,50);
  }
  //successfully read and sent sensor data
  else {
    //blink green
    blink(0,255,0,2,50);
  }
  delay(MS_DELAY);
} 
/******************************************************************************
 * Shift the bits around to be the right order to send on the wire
 *******************************************************************************/
uint16_t wireValue(uint16_t value,unsigned char *buf) {
  buf[0] = value >> 8;
  buf[1] = value;
  return value;
}
/******************************************************************************
 * narcoleptic library at https://code.google.com/p/narcoleptic/
 * The Narcoleptic sleep function can range from 16 to 8,000 milliseconds (eight seconds). 
 * This helper function allows for longer (low power) sleeping
 *******************************************************************************/
void narcolepticDelay(long milliseconds)
{
  while(milliseconds > 0)
  {
    if(milliseconds > 8000)
    {
      milliseconds -= 8000;
      Narcoleptic.delay(8000);
    }
    else
    {
      Narcoleptic.delay(milliseconds);
      break;
    }
  }
}
/******************************************************************************
 * Set the RGB 0 (OFF) to 255 (MAX) from http://arduino.cc/en/Tutorial/ReadASCIIString
 *******************************************************************************/
void setColor( uint8_t red, uint8_t green, uint8_t blue)
{
  // constrain the values to 0 - 255 and invert
  red = 255 - constrain(red, 0, 255);
  green = 255 - constrain(green, 0, 255);
  blue = 255 - constrain(blue, 0, 255);
  analogWrite(redPin, red);
  analogWrite(greenPin, green);
  analogWrite(bluePin, blue);
}
/******************************************************************************
 * Read Sensors and send readings to Base System
 *******************************************************************************/
void doTasks() {
  byte currentPosInSensorReadings = 1;
  float floatVal;
  uint16_t value;
  /******************************************************************************
   * Try several times to get the sensor readings
   *******************************************************************************/
  for (byte i=0; i<=SEND_RETRIES; i++) {
    /******************************************************************************
     * Readings from DHT22
     *******************************************************************************/
    //----------------- HUMIDITY --------------------------
    floatVal = dht.readHumidity();
    //check if a valid float was returned
    if (!isnan(floatVal)) {
      //round to higher integer
      value = floatVal + .5;
      value = wireValue(value,&sensorReadings[currentPosInSensorReadings]);
      currentPosInSensorReadings += 2;
      DEBUG_PRINTF("\n---> Humidity: ");
      DEBUG_PRINTLN(value);      
    }
    else {
      DEBUG_PRINTF("\n---> ERROR: Could not read the Humidity from the DHT22");
      state = NOT_WORKING_SENSORS;
      return;

    }
    //----------------- AIR TEMPERATURE --------------------
    floatVal = dht.readTemperature();
    //check if a valid float was returned
    if (!isnan(floatVal)) {
      //convert to Fahrenheit
      floatVal = round(floatVal*9.0/5.0+32.0);
      //then round to higher integer
      value = floatVal + .5;
      value = wireValue(value,&sensorReadings[currentPosInSensorReadings]);
      currentPosInSensorReadings += 2;
      DEBUG_PRINTF("\n---> Air Temp: ");
      DEBUG_PRINTLN(value);   
    }
    else {
      DEBUG_PRINTF("\n---> ERROR: Could not read the air temp");
      state = NOT_WORKING_SENSORS;
      return;
    }   
    /******************************************************************************
     * Readings from TCS34725
     *******************************************************************************/
    //----------------- COLOR TEMPERATURE -------------------  
    uint16_t r, g, b, c;
    tcs.getRawData(&r, &g, &b, &c);
    value = tcs.calculateColorTemperature(r, g, b);
    if (!isnan(value)) {
      value = wireValue(value,&sensorReadings[currentPosInSensorReadings]);
      currentPosInSensorReadings += 2;
      DEBUG_PRINTF("\n---> Color Temp: ");
      DEBUG_PRINTLN(value);   
    }
    else {
      DEBUG_PRINTF("\n---> ERROR: Could not read the color temp");
      state = NOT_WORKING_SENSORS;
      return;
    }

    //----------------- LUX ----------------------------------  
    value = tcs.calculateLux(r, g, b);
    if (!isnan(value)) {
      value = wireValue(value,&sensorReadings[currentPosInSensorReadings]);
      DEBUG_PRINTF("\n---> LUX: ");
      DEBUG_PRINTLN(value);   
    }
    else {
      DEBUG_PRINTF("\n---> ERROR: Could not read the LUX");
      state = NOT_WORKING_SENSORS;
      return;
    }
    if (state == WORKING) break;
  }
  /******************************************************************************
   * Send readings to the Base System
   *******************************************************************************/
  // RFM69.cpp defaults: bool sendWithRetry(byte toAddress, const void* buffer, byte bufferSize, byte retries=2, byte retryWaitTime=15);
  //Try several times to give time for the Base System to send an ACK
  for (byte i=0; i<=SEND_RETRIES; i++)
  {
    if (radio.sendWithRetry(BASESYSTEMID, sensorReadings,sizeof sensorReadings)) {
      DEBUG_PRINTF("\n---> Sensor readings received by Base System....");
      DEBUG_PRINTF("   [RX_RSSI:");
      DEBUG_PRINT(radio.RSSI);
      DEBUG_PRINTLNF("]");
      return;
    }
  }
  DEBUG_PRINTLNF("---> Could not send sensor readings to Base System");
  state = DATA_NOT_SENT;
  return;
}
/******************************************************************************
 * Blink the RGB LED with a color of red, green, blue
 * numTimesToBlink.  Delay delay_ms between turning on/off blink
 *******************************************************************************/
void blink(uint8_t red,uint8_t green,uint8_t blue,uint8_t numTimesToBlink,uint16_t delay_ms) {
  for (int i=0;i<numTimesToBlink;i++) {
    setColor(red,green,blue);
    delay(delay_ms);
    setColor(0,0,0);
    delay(delay_ms);
  }
}















































