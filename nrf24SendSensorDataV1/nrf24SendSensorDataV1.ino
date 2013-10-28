#include <SPI.h>
#include <RF24.h>

// ce,csn pins
RF24 radio(9,10);

// init data buffer to hold a sensor type byte and one uint16_t sensor data
unsigned char data[3] = {
  0};
unsigned long count=0;
void setup(void)
{
  Serial.begin(57600);
  Serial.println("**************V1 Send Sensor Data***********");
  radio.begin();
  radio.setPALevel(RF24_PA_LOW);
  radio.setChannel(0x4c);

  // open pipe for writing
  radio.openWritingPipe(0xF0F0F0F0E1LL);

  radio.enableDynamicPayloads();
  radio.setAutoAck(true);
  radio.powerUp();
  Serial.println("...Sending");
}

void loop(void)
{
  //generate a random number to represent a sensor reading
  uint16_t sensorValue = random(10,1001);
  //assign 'T' to represent a Temperature reading
  data[0] = 'T';
  //do the bit shift to put uint16 into uint8 array
  data[1] = sensorValue >> 8;
  data[2] = sensorValue;
  Serial.print(count);
  count++;
  Serial.print(" data[1]: ");
  Serial.print(data[1]);
  Serial.print("  |  data[2]: ");
  Serial.print(data[2]);
  // print and increment the counter
  radio.write(data, sizeof(uint16_t)+1);
  Serial.print("  | Temperature sent:  ");
  Serial.println(sensorValue);
  // pause a second
  delay(1000);
}


