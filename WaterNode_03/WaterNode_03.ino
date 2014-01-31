
/*
 * Initial version of the water node
 * remember to set serial monitor to 115200 and carriage RETURN if inputing through a serial monitor
 * Copyright M. Johnson 1/2014
 */
#define DEBUG 1
#include <debugHelpers.h>
#define SERIAL_BAUD   115200
//RFM69
#include <RFM69.h>
#include <SPI.h>
#include <SPIFlash.h>
#define NODEID        2                                                                                    //unique for each node on same network
#define NETWORKID     100                                                                                  //the same on all nodes that talk to each other
#define BASESTATIONID  1                                                                                   //node number that identifies the Base Station within the network groups
#define FREQUENCY   RF69_433MHZ
RFM69 radio;
#include <SoftwareSerial.h>
SoftwareSerial pHserial(5,6);                                                                             // create an instance of the SoftwareSerial library to communicate with the pH sensor
SoftwareSerial ECserial(7,8);                                                                             // create another SoftwareSerial library instance to communicate with the conductivity sensor
#include <OneWire.h>                                                                                      // include the oneWire library to communcate with the water temerature sensor 
const byte waterTemp_PIN=4;
OneWire ds(waterTemp_PIN);                                                                                //create an instance of the oneWire class named ds
char inputString[5];                                                                                      //holds command coming in from serial monitor
byte idx = 0;  //ncrement which byte in the inputString the current char coming into the serial monitor should be 
const byte pH = 0;                                                                                       //to identify that readings should be taken from the pH sensor
const byte EC = 1;                                                                                       //to identify that readings should be taken from the EC sensor
byte sensorType = pH;                                                                                    //if sensorType=pH, take a pH reading.  If sensorType=EC, take an EC reading
boolean input_stringcomplete = false;                                                                    //have we received all the data from the PC
boolean phStringReceived=false;                                                                          //have we received a reading from the pH sensor
boolean ECStringReceived=false;                                                                          //have we received a reading from the EC sensor
char sensorReadingpH[20];                                                                                //holds the string representation of the pH value returned from the sensor
char sensorReadingEC[20];                                                                                //substring holding the string representation of the conductivity values
bool isECReadingInContinuousMode=false;                                                                  //we need to take 25 EC readings before using a reading.  Currently we keep sending a "C\r" because many times the command needs to be sent more than once and we are not sure how many times we might need to send the command to start the EC circuit in continuous mode
byte pHAndConductivityReadingsAvailable=0;                                                               //0 = no readings available.  1 = pH reading available 2 = EC reading available.  3 = both pH and EC reading is available;
bool isRemoteInput=true;                                                                                //input can come from the serial port or from the RFM69HW. The input is handled differently - and there are different commands depending on where the input came from.  Start with setting as remote so that continuous readings prime the pump for the conductivity reading.
/***********************************************************************************/
void setup(){              
  Serial.begin(SERIAL_BAUD);

  DEBUG_PRINTLNF("************** Water Sensor Node v0.1 ***************");
  DEBUG_PRINTF("****>>>>Free Ram: ");
  DEBUG_PRINTLN(freeRam());
  DEBUG_PRINTF("****>>>>Sketch Size: ");
  DEBUG_PRINTLN(sketchSize());   
  ////////////////////////////////////////////////////////////////
  //Initialize the software serial ports for the pH and EC sensors
  //Start listening on the pH serial port (default sensor on serial)
  pHserial.begin(38400);
  ECserial.begin(38400);
  sensorType=pH;                                                                                       //(default) sensor type to read is pH 
  listenOnPort(sensorType);                                                                            //when the sensor type is changed, the listening port is also changed.
  //  -----------------------------------------------------------
  radio.initialize(FREQUENCY,NODEID,NETWORKID);                                                        //Initialize the RFM69
  radio.setHighPower();                                                                                //using the RFM69HW
  DEBUG_PRINTLNF("--> RFM69HW is initialized");
  showHelp();
  getTemp();                                                                                           //first temperature reading is inccorrect.  Throw it out.
}
/***********************************************************************************/
//Commands (see showHelp() ) can come from the serial monitor if hooked up to a PC and within the Arduino IDE
void serialEvent() {                                                                                   //if the hardware serial port receives a char
  char inchar = (char)Serial.read();                                                                   //get the char we just received
  inputString[idx] = inchar;                                                                           //add it to the inputString
  idx++;                                                                                               //next time write to the next byte of the input string
  if(inchar == '\r')
  {
    input_stringcomplete = true;
    isRemoteInput = false;
    inputString[idx] = 0;
    idx=0;
  }  
}
/***********************************************************************************/
void loop(){     //here we go....
  if (input_stringcomplete){                                                                            //A (local) command came in from the serial port
    handleLocalCommand();
    input_stringcomplete = false;
  }
  // holds the string representation of the 3 connectivity values.
  //  -----------------------------------------------------------
  if (pHserial.available()) {                                                                          // reading came in from the pH sensor
    static long nReadings = 0;                                                                         //used to compare how many total readings have been made versus how many of the readings were not valid
    static long nReadingsNotValid=0;
    nReadings++;                                                                                       //one more reading has come in, so increment the count of the total number of pH readings that have been  received.
    byte nBytesReceivedFromSensor = pHserial.readBytesUntil(13,sensorReadingpH,20);                    //read the data sent from pH Circuit until a <CR>. Whether we'll use it (greater than MIN_PRE_READINGS) or not to clear out the serial port.  Also, count how many character have been received.
    sensorReadingpH[nBytesReceivedFromSensor]=0;  
    if (!isRemoteInput) {
      displayString(sensorReadingpH);
    }
    if (isValidReading(sensorReadingpH)) {                                                             //there are a few checks that can be made to check the validity.  The only one I'm using now is a check for non-printable ASCII characters.  The phStringReceived flag is used later in the loop() to switch between reading the pH and the conductivity.
      phStringReceived=true;        
      pHAndConductivityReadingsAvailable = pHAndConductivityReadingsAvailable | 1;                   //turn on the bit that says "yes indeedy, we have received a pH reading"
    }
    else {
      nReadingsNotValid++;
    }
  }
  //  -----------------------------------------------------------
  if (ECserial.available()) {                                                                          //reading came in from connectivity sensor...similar to what is done when a reading comes in from the pH sensor.
    static long nReadings = 0;
    static long nReadingsNotValid = 0;
    nReadings++;
    DEBUG_PRINTF("---> total number of EC readings = ");
    DEBUG_PRINTLN(nReadings);
    isECReadingInContinuousMode = true;
    byte nBytesReceivedFromSensor = ECserial.readBytesUntil(13,sensorReadingEC,20);                    //read the data sent from pH Circuit until a <CR>. Whether we'll use it (greater than MIN_PRE_READINGS) or not to clear out the serial port.  Also, count how many character have been received.
    sensorReadingEC[nBytesReceivedFromSensor]=0;                                                       //add a 0 to the spot in the array just after the last character. This will stop us from transmitting incorrect data that may have been left in the buffer.
    bool validReading = isValidReading(sensorReadingEC);
    if (validReading && nReadings>25) {                                                                //per the data sheet, take 25 readings before using a reading.  Note: some of the readings will most likely contain noise (e.g.: non-printable ASCII) we are counting these as readings under the assumption the challenge is in the wiring or serial part (i.e.: transfer of data from circuit to Arduino)
      if (!isRemoteInput) displayString(sensorReadingEC);                                               //command came in from the serial port to get a reading.  A reading came in from the pH port soon after (i.e.: the boolean was local, the port was the pH port)
      ECStringReceived=true;
      ECserial.print("E\r");                                                                       
      delay(50);
      ECserial.print("E\r");
      delay(50);
      pHAndConductivityReadingsAvailable = pHAndConductivityReadingsAvailable | 2;                   //turn on the bit that says "yes indeedy, we have received a EC reading
    }
    else if (!validReading) {
      nReadingsNotValid++;
    }
  }
  //  -----------------------------------------------------------
  if (radio.receiveDone()) {                                                                           //received a remote command from the Base Station
    isRemoteInput = true;
    DEBUG_PRINTF("[");                                                                                 //write out the signal strength when debugging
    DEBUG_PRINTLNF("--> Received data from the base station");
    DEBUG_PRINT(radio.SENDERID);
    DEBUG_PRINTF("] ");
    DEBUG_PRINTF("   [RX_RSSI:");
    DEBUG_PRINT(radio.RSSI);
    DEBUG_PRINTLNF("]");
    DEBUG_PRINTF("\n\nNumber of bytes received: ");
    DEBUG_PRINTLN(radio.DATALEN);
    if (radio.DATALEN > 0)  {                                                                          //check if data came in from the RFM69
      for (byte b=0;b<radio.DATALEN;b++) {                                                             //put the command into the input string      
        inputString[b] = (char)radio.DATA[b];
      }
      if (inputString[0] == 'a') {                                                                    //the request from the base station was to send a string of readings.  
        DEBUG_PRINTLNF("--> Command sent by base station is to return the sensor readings");
        char sensorReadingwt[20];
        getTempString(sensorReadingwt);                                                               //getting the temperature is a simple mater of write/read to pin assigned to reading/writing to water temp sensor.
        sendReadings(sensorReadingwt,sensorReadingpH,sensorReadingEC);
      } //**TODO: OTHER COMMANDS THAT COME FROM THE BASE STATION
      if (radio.ACK_REQUESTED)                                                                        //the base station requested an ACK be sent back
      {
        radio.sendACK();
        DEBUG_PRINTLNF("\n - ACK sent");
      }
    }
  }
  //  -----------------------------------------------------------
  //Continuously get readings, switching between getting a valid pH reading and a valid conductivity reading
  if (isRemoteInput) {
    if (sensorType == pH ) {                                                                           //switch between requesting and receiving a pH | EC reading
      if (phStringReceived) {                                                                          //Received a valid reading which has been stored in the substring holding the pH reading.  If a request to read a sensor comes in, it is this substring that will be used for the pH sensor readings.
        phStringReceived=false;  
        pHserial.print("E\r");
        delay(50);
        pHserial.print("E\r");                                                                           //stop the continual pH readings.  Send the command twice so there is a better chance the circuit receives the command.
        sensorType = EC;                                                                               //Switch over to getting a conductivity readingds 
        listenOnPort(EC);                                                                              //Remember SoftwareSerial can listen to only one port at a time
        ECStringReceived=false;                                                                        //Let the sketch know we're asking for an EC reading
        ECserial.print(getTemp());                                                                     //send the temp to the EC sensor (in C) so that readings will take the current water temp when calculating the pH
        isECReadingInContinuousMode = false;                                                           //restart continuous EC readings
        delay(50);                                                                                     //since we're using the software serial library, I thought a bit of a pause would diminish the chance of rx/tx errors...guess on my part
      }
      else  {
        pHserial.print("C\r");                                                                         //Tell the circuit to go into continous read.  This command will most likely be called multiple times before a reading comes in. I keep repeating the command because during test sometimes it took 2-4 requests before the circuit would start continuously sending readings.
        delay(50);
      }
    } 
    else if (sensorType == EC) {                                                                       //similar to handling of pH request/reading
      if (ECStringReceived) {
        isECReadingInContinuousMode = false; 
        sensorType = pH;
        phStringReceived=false;
        listenOnPort(pH);
        pHserial.print(getTemp());
        delay(50); 
      }
      else {
        if (!isECReadingInContinuousMode) {
          ECserial.print("C\r"); 
          delay(50);                                                                                    //it takes 500 ms to do a reading, so put a slight pause just so that we don't get ahead of ourselves.  Again, nothing time critical is happening so ok to block.
        }
      }     
    } 
  }
}

//  -----------------------------------------------------------
// A command come in from the serial port
void handleLocalCommand(void) {
  DEBUG_PRINTF("--> Entered command: ");
  DEBUG_PRINTLN(inputString);
  sensorType == pH? pHserial.print("E\r") : ECserial.print("E\r");
  if (inputString[0] == '0')                                                                   //switch to pH sensor
  {
    sensorType = pH;
    DEBUG_PRINTLNF("Sensor type is pH");
    listenOnPort(sensorType);
  }
  else if (inputString[0] == '1')                                                                //switch to EC sensor
  {
    sensorType = EC;
    DEBUG_PRINTLNF("Sensor type is EC");
    listenOnPort(sensorType);
  }
  else if (inputString[0] == 'a') {                                                         //handled similarly to handling when the request from readings came from the base station (over RFM69)...
    char sensorReadings[80];
    char sensorReadingwt[20];
    sensorReadings[0] = 0;
    getTempString(sensorReadingwt);               
    makeSensorString(sensorReadings,sensorReadingwt,sensorReadingpH,sensorReadingEC);      //display sensor readings
    displayString(sensorReadings);
  }
  else if (inputString[0] == 't') {
    char tempString[20];
    getTempString(tempString);
    displayString(tempString);
  }
  else if (inputString[0] == '?') {
    showHelp();
  }
  else {
    sensorType == pH? pHserial.print(inputString) : ECserial.print(inputString);
  }
}
/*-----------------------------------------------------------
 handleLocalCommand() calls this function to display results on the serial monitor
 -----------------------------------------------------------*/
void displayString(char *str) {
  Serial.println(str);
}

/*-----------------------------------------------------------
 The software serial ports can listen to only one port at a time.  Switch to the port that is listening for the bytes from the sensor you wish to take a reading from.
 -----------------------------------------------------------*/
void listenOnPort(const byte sensorType) {
  //  DEBUG_PRINTF("...Setting listening port to the ");
  //  sensorType == pH? DEBUG_PRINTLNF("pH sensor"):DEBUG_PRINTLNF("conductivity sensor");
  sensorType == pH? pHserial.listen() : ECserial.listen();
}
/*-----------------------------------------------------------
 Return the temperature from the DS118B20   water temp sensor.  From https://github.com/sparkfun/H2O_pH_Probe/blob/master/Firmware/H20_pHrobe/H20_pHrobe.ino
 Adafruit has a wiring diagram http://learn.adafruit.com/adafruits-raspberry-pi-lesson-11-ds18b20-temperature-sensing/hardware
 -----------------------------------------------------------*/
float getTemp()
{
  //returns the temperature from one DS18S20 in degrees C
  byte data[12];
  byte addr[8];
  if ( !ds.search(addr)) {
    DEBUG_PRINTLNF("--->Water Temp sensor not found!");
    //no more sensors on chain, reset search
    ds.reset_search();
    return -1000;
  }
  DEBUG_PRINTF("Water temp address***");
  for (int i=0;i<8;i++) {
    DEBUG_PRINT(addr[i]);
  }
  DEBUG_PRINTLNF("****");
  if ( OneWire::crc8( addr, 7) != addr[7]) {
    //Serial.println("CRC is not valid!");
    return -1000;
  }
  if ( addr[0] != 0x10 && addr[0] != 0x28) {
    //Serial.print("Device is not recognized");
    return -1000;
  }
  ds.reset();
  ds.select(addr);
  ds.write(0x44,1); // start conversion, with parasite power on at the end
  byte present = ds.reset();
  ds.select(addr);
  ds.write(0xBE); // Read Scratchpad
  for (int i = 0; i < 9; i++) { // we need 9 bytes
    data[i] = ds.read();
  }
  ds.reset_search();
  byte MSB = data[1];
  byte LSB = data[0];
  float tempRaw = ((MSB << 8) | LSB); //using two's compliment
  float temperature = tempRaw / 16;
  return temperature; // returns the temeperature in degrees C
}
/*-----------------------------------------------------------
 Read the water temp and convert to F
 -----------------------------------------------------------*/
float getTempAndConvertToF() {
  float waterTemp = getTemp();
  waterTemp = waterTemp*1.8+32.;  //temp in F
  return waterTemp;
}
/*-----------------------------------------------------------
 Get a temperature reading from the DS18B20 and return the value as a string
 -----------------------------------------------------------*/
void getTempString(char *resultsString) {
  float wt = getTempAndConvertToF();
  dtostrf (wt, 7, 2, resultsString);
}
//***TODO: Don't send if don't have a valid reading for one or more of the sensor string values...
/*-----------------------------------------------------------
 Check if the sensor reading is within a valid range
 For now this means checking if a non-printable ascii char or "check probe" is in the string...
 in the future this could check the actual value.
 -----------------------------------------------------------*/
bool isValidReading(char *strReading) {
  for (byte b=0;b<strlen(strReading);b++) {
    if (!isAscii(strReading[b])) {                                                          //use the Arduino function isAscii() to determine if the character is not an ascii printable character.
      return false;                                                                         //uh oh - bad character in string so return false.
    }
  }
  return true;
}
/*-----------------------------------------------------------
 Send whatever is in the buffer holding sensor values as string back to the base station.  ***TODO: d there should be a check if there is a valid reading in the string.  If there isn't, then need to get all three valid readings before sending over.
 This could be handled by either sending back some indicator - like sensor data not reading...or (perhaps more complicated) keep taking readings until have valid readings for all three. 
 ----------------------------------------------------------*/
void sendReadings(char *sensorReadingwt,char *sensorReadingpH,char *sensorReadingEC){
  //**TODO: CHECK TO MAKE SURE EACH READING HAS COME IN.  IF MISSING A READING, DON'T SEND YET - RATHER START GETTING READINGS...
  char stringToSend[80];
  stringToSend[0] = 0;
  if (pHAndConductivityReadingsAvailable == 3) makeSensorString(stringToSend,sensorReadingwt,sensorReadingpH,sensorReadingEC);        //Have both readings from the Atlas Scientific circuits for pH and conductivity
  else  strcpy(stringToSend,"Error|No readings available, try again");                       //Don't have readings yet for both pH and conductivity.  Assuming valid strings, this should change since readings are continuously requested
  DEBUG_PRINTF("---> Sending this string of readings: ");
  DEBUG_PRINTLN(stringToSend);
  if (radio.sendWithRetry(BASESTATIONID, stringToSend, strlen(stringToSend)))
    DEBUG_PRINTLNF(" received ACK...");
  else DEBUG_PRINTLNF(" Did not receive ACK...");
}
/*-----------------------------------------------------------
 A check to make sure the character is within the ASCII printable codes  
 ----------------------------------------------------------*/
void makeSensorString(char *stringToMake,char *sensorReadingwt,char *sensorReadingpH,char *sensorReadingEC) {
  strcpy(stringToMake,sensorReadingwt);
  strcat(stringToMake,"|");                                                                 //put a | between readings.
  strcat(stringToMake,sensorReadingpH);
  strcat(stringToMake,"|");                                                                 //put a | between readings.
  strcat(stringToMake,sensorReadingEC);
  DEBUG_PRINTF("---> Sensor string: *");
  DEBUG_PRINTLN(stringToMake);
}
/*-----------------------------------------------------------
 Method for doing help this way from https://github.com/jcw/jeelib/blob/master/examples/RF12/RF12demo/RF12demo.ino  
 ----------------------------------------------------------*/
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

const char helpText[] PROGMEM = 
"\n"
"Available commands:" "\n"
"  ?     - shows available comands" "\n"
"  0     - set mode to pH" "\n"
"  1     - set mode to EC" "\n"
"  t     - take water temperature" "\n"
"  r     - take one sensor reading" "\n"
"  c     = take continuous sensor readings" "\n"
"  e     - stop readings" "\n"
" pH calibration steps - https://www.atlas-scientific.com/_files/_datasheets/_circuit/pH_Circuit_5.0.pdf" "\n"
" EC calibration steps - https://www.atlas-scientific.com/_files/_datasheets/_circuit/EC_Circuit_3.0.pdf" "\n"
;
/*-----------------------------------------------------------
 show command line menu
 -----------------------------------------------------------*/
static void showHelp () {
  showString(helpText);
}
static void showString (PGM_P s) {
  for (;;) {
    char c = pgm_read_byte(s++);
    if (c == 0)
      break;
    if (c == '\n')
      Serial.print('\r');
    Serial.print(c);
  }
}
































































































