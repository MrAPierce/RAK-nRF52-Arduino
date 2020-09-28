/**
 * @file RAK1920_Grove_PIR_AS312.ino
 * @author rakwireless.com
 * @brief Human body detection using Grove PIR Unit.
 * @version 0.1
 * @date 2020-07-28
 * 
 * @copyright Copyright (c) 2020
 * 
 * @note RAK5005-O GPIO mapping to RAK4631 GPIO ports
 * IO1 <-> P0.17 (Arduino GPIO number 17)
 * IO2 <-> P1.02 (Arduino GPIO number 34)
 * IO3 <-> P0.21 (Arduino GPIO number 21)
 * IO4 <-> P0.04 (Arduino GPIO number 4)
 * IO5 <-> P0.09 (Arduino GPIO number 9)
 * IO6 <-> P0.10 (Arduino GPIO number 10)
 * SW1 <-> P0.01 (Arduino GPIO number 1)
 */
void setup()
{
	Serial.begin(115200);
	while (!Serial)
		;
	pinMode(21, INPUT);
}

void loop()
{

	if (digitalRead(21) == 1)
	{
		Serial.println("PIR Status: Sensing");
		Serial.println(" value: 1");
	}
	else
	{
		Serial.println("PIR Status: Not Sensed");
		Serial.println(" value: 0");
	}
	delay(500);
}
