/*
This is an exploratory script to evolve the Luttuce Grower Prototype's pH feature capabilities.  It is an evolution
 of the minipH.ino script: https://www.sparkyswidgets.com/portfolio-item/ph-probe-interface/
 ...and now - a smessage from Sparky....
 Sparky's Widgets 2012
 The usage for this design is very simple, as it uses the MCP3221 I2C ADC. Although actual
 pH calculation is done offboard the analog section is very well laid out giving great resultsßßßßœ
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
enum UI_states_T
{
  INPUT_CHAR,
  CALIBRATE,
  CHECK_PROBE,
  READ_PH,
  INFO,
  HELP,
  INVALID_ENTRY
}
state;

float pH;
float vRef = 4.93; //Our vRef into the ADC wont be exact
//Since you can run VCC lower than Vref its
//best to measure and adjust here
const float opampGain = 5.25; //what is our Op-Amps gain (stage 1)
//state machine variables
unsigned int pHReadCalibrationPeriod = 500;           
unsigned long lastpHCalibrationMillis = 0;


void setup(){
  Wire.begin(); //conects I2C
  Serial.begin(115200);
  vRef = readVcc();
  //Lets read our Info from the eeprom and setup our params,
  //if we loose power or reset we'll still remember our settings!
  eeprom_read_block(&params, (void *)0, sizeof(params));
  //if its a first time setup or our magic number in eeprom is wrong reset to default
  if (params.WriteCheck != Write_Check){
    reset_Params();
  }
  showHelp();
  state = INPUT_CHAR;
}

void loop(){
  char pH_calibrating;
  //use the serial monitor to command the minipH
  if(Serial.available() ) 
  {
    char c = Serial.read();
    switch(state) 
    {
    case INPUT_CHAR:
      switch(c)
      {
      case 'I':
      case 'i':
        state=INFO;
        break;
      case '4':
      case '7':
        pH_calibrating = Serial.parseInt();
        state = CALIBRATE;
        break;
      default:
        state = INVALID_ENTRY;
        break;
      case 'p':
      case 'P':
        Serial.println("\n***TBD: CHECK PROBE***");
        state = CHECK_PROBE;
        break;
      case 'r':
      case 'R':
        state = READ_PH;
        break;
      case '?':
        state = HELP;
        break;
      }
      break;
    case CALIBRATE:
      calibrate(pH_calibrating);
      state = INPUT_CHAR;
      break;
    case CHECK_PROBE:
      state = INPUT_CHAR;
      {
        long vcc = readVcc();
        Serial.print("Vcc: ");
        Serial.println(vcc);
        bool probeWorks = checkProbe();
        if (probeWorks) Serial.println("The pH Probe is GOOD");
        else Serial.println("The pH probe NEEDS TO BE REPLACED");
      }
      break;
    case READ_PH:
      {
        float pHValue = readpH();
        Serial.print("PH is: ");
        Serial.println(pHValue);
      }
      state = INPUT_CHAR;
      break;
    case HELP:
      showHelp();
      state=INPUT_CHAR;
      break;
    case INFO:
      showInfo();
      state=INPUT_CHAR;
      break;
    case INVALID_ENTRY:
      Serial.println("\n\nThe character you entered is not a valid character, try one of these:");
      showHelp();
      state=INPUT_CHAR;
      break;
    }
  }
}
float readpH() {
  unsigned int adc = readADC();
  return calcpH(adc);
}
int readADC() {    
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
float calcpH(int raw)
{
  float miliVolts = (((float)raw/4096)*vRef)*1000;
  float temp = ((((vRef*(float)params.pH7Cal)/4096)*1000)- miliVolts)/opampGain;
  pH = 7-(temp/params.pHStep);
}
bool checkProbe(){
  //Assumes the probe is in the calibration solution for pH 7
  //check to see if the measurement for pH 7 is within +/- 30mV
  int adc_value = readADC();
  float milliVolts = (float)adc_value*(vRef/4096);
  //a "perfect" value is 0.  Is the value is > +/- 30mV, it needs to be replaced
  if (milliVolts > 30. || milliVolts < -30) {
    //probe needs to be replaced
    return false;
  }
  //check to see if the amount of noise within a span of readings is acceptable
  float last_pH_reading = 0.;
  float avg_pH = 0.;
  unsigned int npHReadings=0;
  unsigned int nBadpHReadings=0;
  unsigned long currentMillis = millis();
  unsigned long start_probe_noise_readings_millis = currentMillis;
  unsigned long probe_noise_reading_period = 1000; //time to take samples from the probe
  while (currentMillis - start_probe_noise_readings_millis < probe_noise_reading_period) 
  {       
    //take a pH reading
    float pH_reading = readpH();
    avg_pH  = .7 * pH_reading + .3*last_pH_reading;
    //don't check readings until a few have come in.  This will hopefully accomodate any initial stray readings that might happen
    if (currentMillis > probe_noise_reading_period/4) {
      npHReadings++;
      if (abs(pH_reading) > 1.1*avg_pH) nBadpHReadings++;
    } 
    last_pH_reading = pH_reading;
    currentMillis = millis();      
  }
  if ((float)nBadpHReadings/npHReadings >.1) return false;
  //if gotten this far, the probe doesn't need to be changed
  return true;
}
//were there more than 10% bad readings?
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
void calibrate(char pHCalibrating) {
  int adc_result;
  unsigned long currentMillis = millis();
  lastpHCalibrationMillis = currentMillis;
  while (currentMillis - lastpHCalibrationMillis < pHReadCalibrationPeriod) 
  {       
    //get a pH reading (assumes pH probe is in a calibration solution)
    adc_result = readADC();
    float last_pH = params.pHStep;
    //modify the mV between pH readngs by the current adc reading
    if( pHCalibrating == 4 ) calibratepH4(adc_result);
    if( pHCalibrating == 7 ) calibratepH7(adc_result);
    //add the new calibration reading to the weighted average.. putting a weight of 70% on latest additions was decided as a starting place...
    params.pHStep  = .7 * params.pHStep + .3*last_pH;
    Serial.print("pH Slope: ");
    Serial.println(params.pHStep);
    currentMillis = millis();      
  }
  //write the new pH voltage step unit to EEPROM so that it is stored in 'permanent' memory
  eeprom_write_block(&params, (void *)0, sizeof(params)); //write these settings back to eeprom
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
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

const char helpText[] PROGMEM = 
"\n"
"Available commands:" "\n"
"  ?     - shows available comands" "\n"
"  I     - list current values" "\n"
"  R     - read the pH value" "\n"
"  4     - probe is in pH4 calibrated solution.  Start calibration" "\n"
"  7     - probe is in pH7 calibrated solution.  Start calibration" "\n"
"  P     - probe is in pH7 calibrated solution. Start check of Probe Health" "\n"
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
/*-----------------------------------------------------------
 show info
 -----------------------------------------------------------*/
void showInfo() {
  Serial.print("VREF: ");
  Serial.print(vRef);
  eeprom_read_block(&params, (void *)0, sizeof(params));
  Serial.print("pH 7 ADC value: ");
  Serial.print(params.pH7Cal);
  Serial.print(" | ");
  Serial.print("pH 4 ADC value: ");
  Serial.print(params.pH4Cal);
  Serial.print(" | ");
  Serial.print("pH probe slope: ");
  Serial.println(params.pHStep); 
}

//a smart person figured this out: https://code.google.com/p/tinkerit/wiki/SecretVoltmeter    
//according to the doc:
//How it works
//The Arduino 328 and 168 have a built in precision voltage reference of 1.1V. This is used sometimes for precision measurement, although for Arduino it usually makes more sense to measure against Vcc, the positive power rail.
//The chip has an internal switch that selects which pin the analogue to digital converter reads. That switch has a few leftover connections, so the chip designers wired them up to useful signals. One of those signals is that 1.1V reference.
//So if you measure how large the known 1.1V reference is in comparison to Vcc, you can back-calculate what Vcc is with a little algebra. 
long readVcc() {
  long result;
  // Read 1.1V reference against AVcc
  ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Convert
  while (bit_is_set(ADCSRA,ADSC));
  result = ADCL;
  result |= ADCH<<8;
  result = 1126400L / result; // Back-calculate AVcc in mV
  return result;
}

























