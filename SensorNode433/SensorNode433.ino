//One Wire library
#include <VirtualWire.h>
//I2C library
#include <Wire.h>
//LCD library: http://www.dfrobot.com/wiki/index.php/I2C/TWI_LCD1602_Module_(SKU:_DFR0063)
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27,16,2);  // set the LCD address to 0x27 for a 16 chars and 2 line display
#include <debugHelpers.h>
/*****************************************************
 * Digital i/o pin for 433 MHz tx
 ******************************************************/
const byte  DATASENDPIN=4;
/*****************************************************
 * Let the sketch know the number of sensor readings
 * Each reading takes 2 bytes.  The first byte lets the rx know the
 * total number of sensor readings.
 ******************************************************/
const byte  NUMSENSORREADINGS=6;
unsigned char sensorReadings[NUMSENSORREADINGS*2+1] = {
  0};
/*****************************************************
 * DHT22 Temp and Humidity Sensor
 ******************************************************/
#include <DHT.h>
const byte DHTPIN=5;
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
/*****************************************************
 * the One-Wire library is used to read the DS18B20 water temp sensor
 ******************************************************/
#include <OneWire.h>
const byte WATER_TEMP_PIN=6;
OneWire ds(WATER_TEMP_PIN);
byte addr[8];
/*****************************************************
 * TCS34725 light sensor
 ******************************************************/
#include "Adafruit_TCS34725.h"
Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_700MS, TCS34725_GAIN_1X);
/*****************************************************
 * Atlas Scientific pH sensor
 ******************************************************/
#include <SoftwareSerial.h> // include the software serial library to add an aditional serial port to talk to the pH stamp
SoftwareSerial pHserial(2,3); // (RX,TX) 
/*****************************************************
 * Used for timing between LCD writes when sending data and then number of increments before sending data
 * TIME_INCREMENT_DELAY * NUM_TIME_INCREMENTS = time (in ms) between readings
 ******************************************************/
int TIME_INCREMENT_DELAY = 1200;
byte NUM_TIME_INCREMENTS = 6;
byte currentNumTimeIncrements=1;
void setup() {
  Serial.begin(57600);
  lcd.init();
  lcd.backlight();
  lcd.print("Free Ram: ");
  lcd.print(getFreeRam());
  Serial.println(F("************** SensorNode433 ***************"));
  Serial.print(F("Free Ram: "));
  Serial.println(getFreeRam());
  //SET UP TX
  //SET TRANSFER RATE AT 2000bps
  vw_setup(2000);
  //SET THE DIGITAL i/o for data send
  vw_set_tx_pin(DATASENDPIN);
  //Set up Water Temp sensor - there is only one 1 wire sensor
  if ( !ds.search(addr)) {
    Serial.println(F("---> ERROR: Did not find the DS18B20 Water Temperature Sensor!"));
    return;
  }
  else {
    Serial.print(F("DS18B20 ROM ="));
    for(byte i = 0; i < 8; i++) {
      Serial.write(' ');
      Serial.print(addr[i], HEX);
    }
    Serial.println();
  }
  if (!tcs.begin()) {
    Serial.println(F("---> ERROR: Did not find the TCS34725 light sensor!"));
    return;
  }

  initPhSensor();
  //set the first byte of the bytes sending to the Base System to be the # of sensor readings
  sensorReadings[0] = NUMSENSORREADINGS;
  writeToLCD("Setup is Done");
}
void loop() {
  float floatVal;
  uint16_t value;
  if (currentNumTimeIncrements == NUM_TIME_INCREMENTS) {
    currentNumTimeIncrements=1;
    //----------------- HUMIDITY --------------------------
    floatVal = dht.readHumidity();
    if (isnan(floatVal)) {
      Serial.println(F("---> ERROR: Could not read the humidity from the DHT22"));
      writeToLCD("Error: humidity");
      return;
    }
    value = floatVal + .5;
    value = wireValue(value,&sensorReadings[1]);
    Serial.print(F("---> Humidity: "));
    Serial.println(value);  
    //----------------- AIR TEMPERATURE --------------------
    floatVal = dht.readTemperature();
    if (isnan(floatVal)) {
      Serial.println(F("---> ERROR: Could not read the temperature from the DHT22"));
      writeToLCD("Error: air temp");
      return;
    }
    floatVal = round(floatVal*9.0/5.0+32.0);  //convert to Fahrenheit
    //round to higher integer
    value = floatVal + .5;
    value = wireValue(value,&sensorReadings[3]);
    Serial.print(F("---> Air Temp: "));
    Serial.println(value);
    //----------------- WATER TEMPERATURE -------------------
    value = getWaterTemperature();
    if (value == 85) {//I've noticed 85 is the value I get when the sensor isn't working..but could be a correct reading
      Serial.println(F("WARNING: The water temperature sensor is returning 85.  This might mean the sensor is not working correctly!"));
      writeToLCD("Warn: water Temp");
    }
    //Fahrenheit...
    floatVal = round((float)value*9.0/5.0+32.0);
    value = floatVal + .5;
    Serial.print(F("---> Water Temp: "));
    Serial.println(value);
    value = wireValue(value,&sensorReadings[5]); 
    //----------------- COLOR TEMPERATURE -------------------  
    uint16_t r, g, b, c;
    tcs.getRawData(&r, &g, &b, &c);
    value = tcs.calculateColorTemperature(r, g, b);
    if (isnan(value)) {
      Serial.println(F("---> ERROR: Could not read the color temperature from the TCS34725!"));
      writeToLCD("Error: color temp");
      return;
    }
    Serial.print(F("---> Color Temp: "));
    Serial.println(value);
    value = wireValue(value,&sensorReadings[7]);   
    //----------------- LUX ----------------------------------  
    value = tcs.calculateLux(r, g, b);
    if (isnan(value)) {
      Serial.println(F("---> ERROR: Could not read the LUX from the TCS34725!"));
      writeToLCD("Error: LUX");
      return;
    }
    Serial.print(F("---> LUX: "));
    Serial.println(value);
    value = wireValue(value,&sensorReadings[9]);  
    //----------------- pH -----------------------------------  
    value =  getPh();
    if (isnan(value)) {
      Serial.println(F("---> ERROR: Could not read the pH value!"));
      writeToLCD("Error: pH");
      return;
    }
    Serial.print(F("---> pH: "));
    Serial.println(value);
    Serial.println(F("\n----------------------------------------------"));
    value = wireValue(value,&sensorReadings[11]);  
    //----------------- SEND READINGS ------------------------
    vw_send(sensorReadings,sizeof sensorReadings);
        writeToLCD("Sent Readings");
        delay(1000);
  }
  char buf[10];
  itoa(currentNumTimeIncrements,buf,10);
  if (currentNumTimeIncrements != NUM_TIME_INCREMENTS) writeToLCD(buf);
  currentNumTimeIncrements++;
  delay(TIME_INCREMENT_DELAY);
  //
  //TBD: Delay user controlled

}
/******************************************************************************
 * return celsius reading of water temmperature
 *******************************************************************************/
uint16_t getWaterTemperature() {
  //  byte data[12];
  byte data[2];
  ds.reset();
  ds.select(addr);
  //see the DS18B20 data sheet for what the commands mean. http://datasheets.maximintegrated.com/en/ds/DS18B20.pdf
  ds.write(0x44); // read temperature and store it in the scratchpad
  ds.reset();
  ds.select(addr);
  ds.write(0xBE); // Read the water temp from the scratchpad
  for ( byte i = 0; i < 2; i++) {           
    data[i] = ds.read();
  }
  int16_t raw = (data[1] << 8) | data[0];
  //I then divide the result by bit shifting instead of / 16.
  uint16_t celsius = raw >> 4;
  return celsius;
}
/******************************************************************************
 * initialize the pH stamp and send it the E command twice to make sure it is in single (versus continual) read mode
 *******************************************************************************/
void initPhSensor() {
  pHserial.begin(38400);//the pH stamp communicates at 38400 baud by default 
  pHserial.print("e\r");
  delay(50); 
  pHserial.print("e\r");
  delay(50);
}
/******************************************************************************
 * return the pH value
 *******************************************************************************/
uint16_t getPh()
{
  const byte MAXSENSORSTRINGSIZE=20;
  char phData[MAXSENSORSTRINGSIZE];
  char phValue[3]={
    0,0,0        };
  byte nBytesReceived;
  //This function queries the pH stamp to return one reading of pH
  pHserial.print("R\r"); //send R followed by a carriage return prompts the stamp to send back a single pH reading
  delay(50);  // sometimes the first command can be missed, so repeat it.
  pHserial.print("R\r");  
  nBytesReceived = pHserial.readBytesUntil(13,phData,MAXSENSORSTRINGSIZE);//retrieve the pH reading.  The reading comes back as a string 
  if (!nBytesReceived) return NAN;
  //
  //pH readings are returned as a string representing a 2 decimal place floating point e.g.: "3.97" means pH reading of 3.97
  //I'm sending back a 1 decimal point pH value as an uint16_t.  E.g.: 39 for "3.97".  This looses a tad of accuracy - I'm assuming
  //not enough to matter(?) and returns data in the same format as all other sensor readings.  The challenge is knowing which values
  //on the receiving end are actually 1 decimal floats instead on unsigned ints.
  phData[nBytesReceived-1] = 0; 
  strncpy(phValue,phData,1);
  strncat(phValue,&phData[2],1);
  return (atoi(phValue));
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
 * Write a status line to the LCD
 *******************************************************************************/
void writeToLCD(char *str) {
  lcd.clear();
  lcd.print(str);
}





















