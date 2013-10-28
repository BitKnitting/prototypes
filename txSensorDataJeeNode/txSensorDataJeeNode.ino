
/*****************************************************
 * Send air temp, water temp, and humidity to a "base station" from a JeeNode
 * Oct 25, 2013   MKJ
 ******************************************************/
#include <JeeLib.h>
/*****************************************************
 * the One-Wire library is used to read the DS118B20 water temp sensor
 * The water temp sensor uses the JeeNode's port 2
 ******************************************************/
#include <OneWire.h>
#define ONE_WIRE_BUS  5
// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);
/*****************************************************
 * Add in  the Dallas Temperature Control library written by Miles Burton
 ******************************************************/
#include <DallasTemperature.h>
// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature waterTempSensor(&oneWire);
DeviceAddress tempDeviceAddress; // We'll use this variable to store a found device address
/****************************************************
 * Using a DHT22 to read air temp and humidity
 * the DHT22 uses the port 1 of the JeeNode
 * JeeLabs provides a simplified library for the DHTxx 
 * sensors that was developed with the JeeNode in mind.
 *****************************************************/
DHTxx  dht(4);
//init data buffer to hold sensor data to be transmitted
unsigned char data[6] = {
  0};
void setup() {
  //Serial port great for debugging when sensor node is connected to Mac, but useless when at hydroponics station
  //  Serial.begin(57600);
  //  Serial.println("******** txSensorDataJeeNode ***********");
  /*****************************************************
   * set up the sensor to take the water temp readings
   ******************************************************/
  // Start up the DallasTemperature library
  waterTempSensor.begin();
  //there is currently only one Dallas Temp Sensor
  if (!waterTempSensor.getAddress(tempDeviceAddress,0)) {
    //    Serial.println("---> UHOH Water Temperature Sensor could NOT be found!");
    return;
  }
  //I tested 9 vs 12 bit precision and 9 was "good enough."  Without setting, the default appeared
  //to be 12 bit precision
  waterTempSensor.setResolution(tempDeviceAddress, 9);
  //initialize the RFM12B so JeeNode can transmit data
  //see RF12.cpp
  //1st param = node ID (0 and 31 are reserved)
  //2nd param = frequency of RF
  //3rd param = "net group" 1-212. Defaults to 212
  rf12_initialize(1,RF12_915MHZ,1);
}
void loop() {
  //
  //read the air temp and humidity.  Use "true" for the precision
  //parameter since the sensor is the DHT22
  //
  int t,h;
  //if any of the readings fail, their value will be 0.
  for (int i=0;i<6;i++)
    data[i] = 0;
  if (dht.reading(t,h,true)) {
    data[0] = t >> 8;
    data[1] = t;
    data[2] = h >> 8;
    data[3] = h;
  }
  //debug: write values to serial port
  //    Serial.print("Air Temperature = ");
  //    Serial.print(t);
  //    Serial.print("  |  Humidity = ");
  //    Serial.print(h);
  //    Serial.print("  |  Water Temperature = "); 
  //the library includes functions for Celsius and Fahrenheit
  //I just wanted a fast uint16 so used getTemp and then divided the result by 16 as noted by the library
  //also, I found (by trial and error) it is important to call a function that resets reading the temperature - like requestTemperaturesBYAddress().  
  //If not, I would get 85 for the temperature.
  if ( waterTempSensor.requestTemperaturesByAddress(tempDeviceAddress)) {
    uint16_t wt = waterTempSensor.getTemp(tempDeviceAddress);
    wt = wt >> 4;
    data[4] = wt >> 8;
    data[5] = wt;      
  }
  //    //see RF12.cpp
  //    //1st param = hdr
  //    //2nd param = pointer to data to send
  //    //3rd param = number of bytes to send (between 0 and 65).
  rf12_sendStart(0,data,sizeof data);
  //wait 3 seconds between sending sensor data
  delay(3000);

}








