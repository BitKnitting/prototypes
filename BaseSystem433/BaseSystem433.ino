//Receiver code
#include <debugHelpers.h>
#include <VirtualWire.h>
#include <SPI.h>
#include <SD.h>

File    sensorDataFile;
const char PartOfHeaderFileName[]="HEADER";
char DataLogFileName[]="datalog.csv";
const byte MAX_CSV_LINE_SIZE=80;
const byte CS=10;  //Chip Select for Adafruit's SD Shield on Arduino Uno
const byte RXpin = 7;
/******************************************************************************
 * SETUP
 *******************************************************************************/
void setup() {
  Serial.begin(57600);
  Serial.println(F("\n************** BaseSystem433 ***************"));
  Serial.print(F("Free Ram: "));
  Serial.println(getFreeRam());
  Serial.println(F("\n...Initializing 433 Mhz Receiver")); //TODO: Any error checks?
  vw_setup(2000);
  vw_set_rx_pin(RXpin);
  vw_rx_start();
  Serial.println(F("\n...Initializing Adafruit's SD Shield"));
  pinMode(SS,OUTPUT);
  //check to see if card is ready
  while (!SD.begin(CS)) {
    Serial.println(F("ERROR: initialization failed. Things to check:"));
    Serial.println(F("* is a card is inserted?"));
    Serial.println(F("* Is your wiring correct?"));
    Serial.println(F("* did you change the chipSelect pin to match your shield or module?"));
//    return; //terminate program
  } 
  Serial.println(F("\n...Opening NEW data logging file"));
  openFile(DataLogFileName,&sensorDataFile);
  if (!sensorDataFile) {
    Serial.print(F("ERROR: Could not open "));
    Serial.println(DataLogFileName);
  //  return;
  }       
  Serial.println(F("\n...Writing Header to Sensor Data Logging File"));
  writeHeaderToCSVFile();
  Serial.println(F("done!"));
}
/******************************************************************************
 * LOOP
 *******************************************************************************/
void loop() {
  //
  //set the length of the buffer that holds incoming data to the maximum message length (as defined in VirtualWire.h - currently this is 80 bytes)
  uint8_t buflen = VW_MAX_MESSAGE_LEN;
  uint8_t buf[buflen];
  char bufcsv[MAX_CSV_LINE_SIZE] = {
    0                 };
  if (vw_get_message(buf,&buflen) ) {
    if (bufLenCorrect(buf,buflen)) {
      for (int i=0;i<buf[0];i++) {  //buf[0] has the number of sensor readings
        uint16_t sensorReading = sensorValue(&buf[i*2+1]);
        if ((strlen(bufcsv) + sizeof sensorReading + 1) > MAX_CSV_LINE_SIZE) {
          Serial.println(F("--->ERROR: There is too much sensor data for the buffer size alloted to a line of the CSV file!"));
          return;
        }
        addSensorReadingToRow(sensorReading,bufcsv);
      }
      //remove the trailing comma from the last sensor reading 
      bufcsv[strlen(bufcsv)-1] = 0;
      Serial.print(F("...Sensor Readings: "));
      Serial.println(bufcsv);
      Serial.println(F("----------------------------"));
      if (!writeSensorDataToCSVFile(DataLogFileName,bufcsv)) {
        Serial.println(F("---> ERROR: Could not open data log file for writing CSV string!"));
        return;
      }
    } 
    else {
      Serial.print(F("---> ERROR:  Did not receive the correct number of bytes: "));
      Serial.println(buflen);
    }
    Serial.print(F("Free Ram: "));
    Serial.println(getFreeRam());
  }
}
/******************************************************************************
 * FUNCTIONS
 *******************************************************************************/
/******************************************************************************
 * Check if the number of bytes sent was the same number as bytes received
 *******************************************************************************/
bool bufLenCorrect(uint8_t *buf,uint8_t buflen) {
  uint8_t numReadings = buf[0];
  //each sensor reading is 2 bytes.  The first byte is the number of sensor readings.
  uint8_t buflenShouldBe = numReadings*2 + 1;
  if (buflenShouldBe == buflen) return true;
  else return false;
}
/******************************************************************************
 * Convert two bytes in the sensor data array into a value
 *******************************************************************************/
uint16_t sensorValue(uint8_t *buf) {
  return buf[1] | buf[0] << 8;
}
/******************************************************************************
 * Open the file for logging sensor data
 *******************************************************************************/
File openFile(char *filename,File *myDataFile)
{
  if (SD.exists(filename)) {
    SD.remove(filename);
    *myDataFile = SD.open(filename,FILE_WRITE);
    if (myDataFile) Serial.println("File opened");
    else Serial.println("File did not open");
  } 
}
/******************************************************************************
 * Write the header for the sensor data in the sensorData.csv file
 *******************************************************************************/
void writeHeaderToCSVFile(){
  //TODO: Replace hard coded with info on sensor data coming in from a sensor node.
  const char* header = "Humidity,Air Temp,Water Temp,Color Temp,LUX,pH";
  sensorDataFile.println(header);
  sensorDataFile.flush();
  sensorDataFile.close();
}/******************************************************************************
 * Write a row of sensor data
 *******************************************************************************/
bool writeSensorDataToCSVFile(char *filename,char *buf) {
  sensorDataFile = SD.open(filename,FILE_WRITE);
  if (sensorDataFile) {
    sensorDataFile.println(buf);
    sensorDataFile.flush();
    sensorDataFile.close();
    return true;
  } 
  else return false;

}
/******************************************************************************
 * Append the sensor data to a line in the csv file
 *******************************************************************************/
void addSensorReadingToRow(uint16_t sensorReading,char *buf) {
  char strValue[5];
  itoa(sensorReading,strValue,10);
  strcat(buf,strValue);
  strcat(buf,",");
}
























