this is a scale for automating equipment or other devices, running on an arduino nano and using an HX711 adc amp

features:
	custom large numbers (6 digits + a separating line fit on a 20x4 screen)
	basic failsafe for when the relays fail to stop the connected machine (retry if it hasnt stopped after 10 units)
	save tare and calibration to eeprom

the hardware used:
	arduino nano
	2004 lcd, with an i2c backpack
	i2c eeprom (the exact model or capacity shouldnt matter, 8 bytes are used)
	HX711 loadcell amplifier/ADC
	4-relay module (using less is also possible)
	4 momentary pushbuttons
	(optional) buzzer
	5v psu

assembly is easy as there are few connections to make and no additional components needed other than wires and a board

do keep in mind that this program was built for a specific task
and it may need significant modifications to be adapted to your needs

additional libraries needed:
	AbleButtons
	HD44780_LCD_PCF8574
	HX711

using the scale:
	button 1: increment the set weight (on the right side of the screen)
	button 2: decrement the set weight
	button 3: hold to stop, press to cancel calibration
	button 4: press to tare, hold to calibrate
