/*
This is an exploratory script to evolve the Luttuce Grower Prototype's pH feature capabilities.  It is an evolution
 of the minipH.ino script: https://www.sparkyswidgets.com/portfolio-item/ph-probe-interface/
 ...and now - a smessage from Sparky....
 Sparky's Widgets 2012
 The usage for this design is very simple, as it uses the MCP3221 I2C ADC. Although actual
 pH calculation is done offboard the analog section is very well laid out giving great results
 at varying input voltages (see vRef for adjusting this from say 5v to 3.3v).
 MinipH can operate from 2.7 to 5.5V to accomdate varying levels of system. Power VCC with 3.3v for a raspi!
 
 ADC samples at ~28.8KSPS @12bit (4096 steps) and has 8 I2C address of from A0 to A7 (Default A5)
 simply assemble the 2 BYTE regiters from the standard I2C read for the raw reading.
 conversion to pH shown in code.
 
 Note: MinipH has an optional Vref(4.096V) that can be bypassed as well!
 
 */
//I2C Library
#include <Wire.h>
//We'll want to save calibration and configration information in EEPROM
#include <avr/eeprom.h>
//EEPROM trigger check
#define Write_Check      0x1234

#define ADDRESS 0x4D // MCP3221 A5 in Dec 77 A0 = 72 A7 = 79)
// A0 = x48, A1 = x49, A2 = x4A, A3 = x4B, 
// A4 = x4C, A5 = x4D, A6 = x4E, A7 = x4F

//Our parameter, for ease of use and eeprom access lets use a struct
struct parameters_T
{
  unsigned int WriteCheck;
  int pH7Cal, pH4Cal;
  float pHStep;
} 
params;

float pH;
const float vRef = 4.096; //Our vRef into the ADC wont be exact
//Since you can run VCC lower than Vref its
//best to measure and adjust here
const float opampGain = 5.25; //what is our Op-Amps gain (stage 1)
//state machine variables
unsigned int pHReadCalibrationPeriod = 500;           
unsigned long lastpHCalibrationMillis = 0;


void setup(){
  Wire.begin(); //conects I2C
  Serial.begin(115200);
  //Lets read our Info from the eeprom and setup our params,
  //if we loose power or reset we'll still remember our settings!
  eeprom_read_block(&params, (void *)0, sizeof(params));
  Serial.println(params.pHStep);
  //if its a first time setup or our magic number in eeprom is wrong reset to default
  if (params.WriteCheck != Write_Check){
    reset_Params();
  }
  showHelp();
}

void loop(){
  int adc_result;
  //use the serial monitor to command the minipH
  if(Serial.available() ) 
  {
    char c = Serial.read();
    if (c=='?') showHelp();
    if(c == 'C')
    {
      //Which range?
      int calrange;
      calrange = Serial.parseInt();
      if (calrange == 4 || calrange == 7) {
        //take readings for pHReadCalibrationPeriod  ms
        unsigned long currentMillis = millis();
        lastpHCalibrationMillis = currentMillis;
        while (currentMillis - lastpHCalibrationMillis < pHReadCalibrationPeriod) 
        {       
          //get a pH reading (assumes pH probe is in a calibration solution)
          adc_result = readpH();
          float last_pH = params.pHStep;
          //modify the mV between pH readngs by the current adc reading
          if( calrange == 4 ) calibratepH4(adc_result);
          if( calrange == 7 ) calibratepH7(adc_result);
          //add the new calibration reading to the weighted average.. putting a weight of 70% on latest additions was decided as a starting place...
          params.pHStep  = .7 * params.pHStep + .3*last_pH;
          Serial.print("pH Slope: ");
          Serial.println(params.pHStep);
          currentMillis = millis();      
        }
        //save the new slope so that it is available across boots
        eeprom_write_block(&params, (void *)0, sizeof(params));
        //if calibrating pH 7, check if the weighted average is over 30mV from the ideal 59.16mV..if it is, the probe needs to be replaced.  The slope will decrease over time.
        if (calrange == 7) {
          if (59.16 - params.pHStep > 30) {
            Serial.print("---> Time to replace the pH probe.  Readings are ");
            Serial.print(59.16-params.pHStep);
            Serial.println(" from the 59.16mV!!!");
          }
        }
      }
    }
    if(c == 'I')
    {
      //Lets read in our parameters and spit out the info! 
      eeprom_read_block(&params, (void *)0, sizeof(params));
      Serial.print("pH 7 cal: ");
      Serial.print(params.pH7Cal);
      Serial.print(" | ");
      Serial.print("pH 4 cal: ");
      Serial.print(params.pH4Cal);
      Serial.print(" | ");
      Serial.print("pH probe slope: ");
      Serial.println(params.pHStep); 
    }
  }
}


//Lets read our raw reading while in pH7 calibration fluid and store it
//We will store in raw int formats as this math works the same on pH step calcs
void calibratepH7(int calnum)
{
  params.pH7Cal = calnum;
  calcpHSlope();
}

//Lets read our raw reading while in pH4 calibration fluid and store it
//We will store in raw int formats as this math works the same on pH step calcs
//Temperature compensation can be added by providing the temp offset per degree
//IIRC .009 per degree off 25c (temperature-25*.009 added pH@4calc)
void calibratepH4(int calnum)
{
  params.pH4Cal = calnum;
  calcpHSlope();
}

//This is really the heart of the calibration proccess, we want to capture the
//probes "age" and compare it to the Ideal Probe, the easiest way to capture two readings,
//at known point(4 and 7 for example) and calculate the slope.
//If your slope is drifting too much from Ideal(59.16) its time to clean or replace!
void calcpHSlope ()
{
  float pHSlope; 
  params.pHStep = ((((vRef*(float)(params.pH7Cal - params.pH4Cal))/4096)*1000)/opampGain)/3;
}
int readpH() {    
  Wire.requestFrom(ADDRESS, 2);       
  byte adc_high;
  byte adc_low;
  adc_high = Wire.read();           
  adc_low = Wire.read();
  //now assemble them, remembering our byte maths a Union works well here as well
  return (adc_high * 256) + adc_low;
}
//Now that we know our probe "age" we can calucalate the proper pH Its really a matter of applying the math
//We will find our milivolts based on ADV vref and reading, then we use the 7 calibration
//to find out how many steps that is away from 7, then apply our calibrated slope to calcualte real pH
void calcpH(int raw)
{
  float miliVolts = (((float)raw/4096)*vRef)*1000;
  float temp = ((((vRef*(float)params.pH7Cal)/4096)*1000)- miliVolts)/opampGain;
  pH = 7-(temp/params.pHStep);
}

//This just simply applys defaults to the params incase the need to be reset or
//they have never been set before (!magicnum)
void reset_Params(void)
{
  //Restore to default set of parameters!
  params.WriteCheck = Write_Check;
  params.pH7Cal = 2048; //assume ideal probe and amp conditions 1/2 of 4096
  params.pH4Cal = 1286; //using ideal probe slope we end up this many 12bit units away on the 4 scale
  params.pHStep = 59.16;//ideal probe slope
  eeprom_write_block(&params, (void *)0, sizeof(params)); //write these settings back to eeprom
}

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

const char helpText[] PROGMEM = 
"\n"
"Available commands:" "\n"
"  ?     - shows available comands" "\n"
"  I     - list current values" "\n"
"  C4    - probe is in pH4 calibrated solution.  Start calibration" "\n"
"  C7    - probe is in pH7 calibrated solution.  Start calibration" "\n"
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










