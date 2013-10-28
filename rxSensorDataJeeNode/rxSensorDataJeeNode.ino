
/*****************************************************
 * Receive air temp, water temp, and humidity to a "base station" from a JeeNode
 * Oct 25, 2013   MKJ
 ******************************************************/
#include <JeeLib.h>

void setup () {
  Serial.begin(57600);
  Serial.println("\n\r******** rxSensorDataJeeNode ***********");
  //initialize the RFM12B so JeeNode can transmit data
  //see RF12.cpp
  //1st param = node ID (0 and 31 are reserved)
  //2nd param = frequency of RF
  //3rd param = "net group" 1-212. Defaults to 212
  rf12_initialize(1,RF12_915MHZ,1);
}

void loop () {
  if (rf12_recvDone() && rf12_crc == 0) {
    uint16_t temp = rf12_data[1] | rf12_data[0] << 8;
    uint16_t humidity = rf12_data[3] | rf12_data[2] << 8;
    uint16_t waterTemp = rf12_data[5] | rf12_data[4] << 8;
    Serial.print("Air Temperature: ");
    Serial.print(temp);
    Serial.print("   |   Humidity: ");
    Serial.print(humidity);
    Serial.print("   |   Water Temperature: ");
    Serial.println(waterTemp);
  }
}


