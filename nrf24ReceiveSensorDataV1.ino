#include <SPI.h>
#include <RF24.h>

// ce,csn pins
RF24 radio(9,10);
// set up the data buffer to receive the sensor type byte and the uint16_t sensorValue
unsigned char data[3] = {
  0};
  unsigned long count=0;
void setup(void)
{
  // init serial monitor and radio
  Serial.begin(57600);
  Serial.println("**************V1 Receive Sensor Data***********");
  radio.begin();

  radio.setPALevel(RF24_PA_LOW);
  radio.setChannel(0x4c);

  // open pipe for reading
  radio.openReadingPipe(1,0xF0F0F0F0E1LL);

  radio.enableDynamicPayloads();
  radio.setAutoAck(true);
  radio.powerUp();
  radio.startListening();
  Serial.println("...Listening");
}

void loop(void)
{
  // if there is data ready
  if (radio.available())
  {
    // dump the payloads until we've got everything
    bool done = false;
    while (!done)
    {
      // fetch the payload, and see if this was the last one
      done = radio.read(data, sizeof(uint16_t)+1);
    }

    // print the payload
    if (data[0] == 'T') {
      //put the uint16_t back together
      uint16_t sensorValue = data[2] | data[1] << 8;
      Serial.print(count);
      count++;
      Serial.print(" | Temperature Received: "); 
      Serial.println(sensorValue);
    }
    else
      Serial.println("Did not receive a value temperature reading");
  }
}

