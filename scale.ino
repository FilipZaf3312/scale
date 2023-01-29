#include <AbleButtons.h>
#include <Wire.h>
#include <HD44780_LCD_PCF8574.h>
#include <HX711.h>

using Button = AblePullupClickerButton;
using ButtonList = AblePullupClickerButtonList;

//D5 - D8 BUTTONS
const int BUTTON_1 = 5;
const int BUTTON_2 = 6;
const int BUTTON_3 = 7;
const int BUTTON_4 = 8;

Button button_1(BUTTON_1);
Button button_2(BUTTON_2);
Button button_3(BUTTON_3);
Button button_4(BUTTON_4);

Button *buttons[] = {
	&button_1,
	&button_2,
	&button_3,
	&button_4
};
ButtonList button_list(buttons);

//D9 - D12 RELAYS
const int RELAY_1 = 9;
const int RELAY_2 = 10;
const int RELAY_3 = 11;
const int RELAY_4 = 12;

//D13 BUZZER/INDICATOR
const int BUZZER = 13;

//I2C BUS
const int EEPROM_ADRESS = 80;
const int SCREEN_ADRESS = 39; //0x27
HD44780LCD screen(4, 20, SCREEN_ADRESS);

//HX711
const int HX711_DT = 2;
const int HX711_SCL = 3;
HX711 hx711;

//VARIABLES
int set_weight = 100;
int weight = 0;
int stop_weight = 100;


void setup(){
	button_list.begin(); //setup buttons
	button_1.setHeldTime(200);
	button_2.setHeldTime(200);
	button_3.setHeldTime(2000);
	button_4.setHeldTime(2000);
	
	pinMode(RELAY_1, OUTPUT); //setup relays
	pinMode(RELAY_2, OUTPUT);
	pinMode(RELAY_3, OUTPUT);
	pinMode(RELAY_4, OUTPUT);
	
	pinMode(BUZZER, OUTPUT); //setup buzzer
	
	pinMode(PIN_WIRE_SDA, INPUT_PULLUP); //setup I2C
	pinMode(PIN_WIRE_SCL, INPUT_PULLUP);
	Wire.begin(); //begin I2C
	
	delay(200); //wait for screen to start
	screen.PCF8574_LCDInit(LCDCursorTypeOff); //start screen
	screen.PCF8574_LCDClearScreen(); //clear the screen
	screen.PCF8574_LCDBackLightSet(true); //turn on the backlight
	screen.PCF8574_LCDGOTO(LCDLineNumberOne, 0); //go to the first line and row
	screen.print("Starting...");
	
	uint8_t char0[8] = {0b00000, 0b00000, 0b00000, 0b00000, 0b11111, 0b11111, 0b11111, 0b11111}; //custom characters for large numbers
	uint8_t char1[8] = {0b00000, 0b00000, 0b00000, 0b00000, 0b10000, 0b11000, 0b11000, 0b11100};
	uint8_t char2[8] = {0b11100, 0b11100, 0b11100, 0b11100, 0b11100, 0b11100, 0b11100, 0b11100};
	uint8_t char3[8] = {0b11100, 0b11000, 0b11000, 0b10000, 0b00000, 0b00000, 0b00000, 0b00000};
	uint8_t char4[8] = {0b11111, 0b11111, 0b11111, 0b11111, 0b00000, 0b00000, 0b00000, 0b00000};
	uint8_t char5[8] = {0b00111, 0b00011, 0b00011, 0b00001, 0b00000, 0b00000, 0b00000, 0b00000};
	uint8_t char6[8] = {0b00111, 0b00111, 0b00111, 0b00111, 0b00111, 0b00111, 0b00111, 0b00111};
	uint8_t char7[8] = {0b00000, 0b00000, 0b00000, 0b00000, 0b00001, 0b00011, 0b00011, 0b00111};
	
	screen.PCF8574_LCDCreateCustomChar(0, char0); //load custom characters to the screen
	screen.PCF8574_LCDCreateCustomChar(1, char1);
	screen.PCF8574_LCDCreateCustomChar(2, char2);
	screen.PCF8574_LCDCreateCustomChar(3, char3);
	screen.PCF8574_LCDCreateCustomChar(4, char4);
	screen.PCF8574_LCDCreateCustomChar(5, char5);
	screen.PCF8574_LCDCreateCustomChar(6, char6);
	screen.PCF8574_LCDCreateCustomChar(7, char7);
	
	hx711.begin(HX711_DT, HX711_SCL); //start the sensor
	hx711.wait_ready();
	hx711.set_gain(HX711_CHANNEL_A_GAIN_128, true); //set the gain
	load_settings(); //set offset and scale settings
	
	trigger_relays(); //test the relays
	
	screen.PCF8574_LCDClearScreen();
	screen.print("Ready."); //show that were done
	
	show_weight(); //start normal operation
}


void loop(){
	button_list.handle();
	
	//decrement
	if(button_1.resetClicked() and set_weight > 0){
		set_weight -= 10;
		update_set_weight();
	}
	
	//fast decrement
	if(button_1.isHeld() and set_weight > 0){
		set_weight -= 10;
		update_set_weight();
	}
	
	//increment
	if(button_2.resetClicked() and set_weight < 990){
		set_weight += 10;
		update_set_weight();
	}
	
	//fast increment
	if(button_2.isHeld() and set_weight < 990){
		set_weight += 10;
		update_set_weight();
	}
	
	if(button_3.resetClicked()){
		//
	}
	
	//stop
	if(button_3.isHeld()){
		trigger_relays();
		
		stop_weight = weight;
	}
	
	//tare
	if(button_4.resetClicked()){
		screen.PCF8574_LCDClearScreen();
		screen.PCF8574_LCDGOTO(LCDLineNumberTwo, 0);
		screen.print("Tare...");
		
		hx711.tare(25);
		
		show_weight();
	}
	
	//calibrate
	if(button_4.isHeld()){
		button_list.handle();
		
		screen.PCF8574_LCDClearScreen();
		screen.PCF8574_LCDGOTO(LCDLineNumberTwo, 0);
		screen.print("Tare...");
		
		hx711.tare(25);
		
		screen.PCF8574_LCDClearScreen();
		screen.PCF8574_LCDGOTO(LCDLineNumberTwo, 0);
		screen.print("Calibration");
		screen.PCF8574_LCDGOTO(LCDLineNumberThree, 0);
		screen.print("Weight:");
		
		int cal_weight = 100;
		update_cal_weight(cal_weight);
		
		while(not button_4.isHeld()){
			button_list.handle();
			
			//decrement
			if(button_1.resetClicked() and cal_weight > 0){
				cal_weight -= 10;
				update_cal_weight(cal_weight);
			}
			
			//fast decrement
			if(button_1.isHeld() and cal_weight > 0){
				cal_weight -= 10;
				update_cal_weight(cal_weight);
			}
			
			//increment
			if(button_2.resetClicked() and cal_weight < 990){
				cal_weight += 10;
				update_cal_weight(cal_weight);
			}
			
			//fast increment
			if(button_2.isHeld() and cal_weight < 990){
				cal_weight += 10;
				update_cal_weight(cal_weight);
			}
			
			//cancel
			if(button_3.isHeld()){
				show_weight();
				button_4.resetClicked();
				goto canceled;
			}
		}
		
		screen.PCF8574_LCDClearScreen();
		screen.PCF8574_LCDGOTO(LCDLineNumberTwo, 0);
		screen.print("Calibrating...");
		
		hx711.calibrate_scale(cal_weight, 25);
		
		store_settings();
		
		show_weight();
	}
	
	canceled:
	update_weight();
	
	//when the set weight is reached, set off the relays
	if(weight >= set_weight and weight < stop_weight + 10){
		trigger_relays();
		stop_weight = weight;
	}
}


//prepare the screen for displaying weight
void show_weight(){
	screen.PCF8574_LCDClearScreen();
	screen.PCF8574_LCDGOTO(LCDLineNumberOne, 9);
	screen.PCF8574_LCDPrintCustomChar(6);
	screen.PCF8574_LCDPrintCustomChar(2);
	screen.PCF8574_LCDGOTO(LCDLineNumberTwo, 9);
	screen.PCF8574_LCDPrintCustomChar(6);
	screen.PCF8574_LCDPrintCustomChar(2);
	screen.PCF8574_LCDGOTO(LCDLineNumberThree, 9);
	screen.PCF8574_LCDPrintCustomChar(6);
	screen.PCF8574_LCDPrintCustomChar(2);
	screen.PCF8574_LCDGOTO(LCDLineNumberFour, 9);
	screen.PCF8574_LCDPrintCustomChar(6);
	screen.PCF8574_LCDPrintCustomChar(2);
	
	update_weight();
	update_set_weight();
}


//write the weight value to the screen
void update_weight(){
	weight *= 9; //get the average of the last 10 readings
	weight += hx711.read();
	weight /= 10;
	
	String weight_str;
	
	if(weight < -99){
		weight_str = "-99";
	}
	else if(weight > 999){
		weight_str = "999";
	}
	else {
		weight_str = weight;
		
		while(weight_str.length() < 3){
			weight_str = " " + weight_str;
		}
	}
	
	print_big_char(weight_str.charAt(0), 0);
	print_big_char(weight_str.charAt(1), 3);
	print_big_char(weight_str.charAt(2), 6);
}


//write the set_weight value to the screen
void update_set_weight(){
	String set_weight_str(set_weight);
	
	while(set_weight_str.length() < 3){
		set_weight_str = " " + set_weight_str;
	}
	
	print_big_char(set_weight_str.charAt(0), 11);
	print_big_char(set_weight_str.charAt(1), 14);
	print_big_char(set_weight_str.charAt(2), 17);
}


//for the calibration function
void update_cal_weight(int cal_weight){
	String cal_weight_str(cal_weight);
	
	while(cal_weight_str.length() < 3){
		cal_weight_str = " " + cal_weight_str;
	}
	
	print_big_char(cal_weight_str.charAt(0), 11);
	print_big_char(cal_weight_str.charAt(1), 14);
	print_big_char(cal_weight_str.charAt(2), 17);
}


//prints a large character to the screen
void print_big_char(char digit, int row){
	screen.PCF8574_LCDGOTO(LCDLineNumberOne, row);
	
	switch(digit){
		default:
			screen.print("   ");
			screen.PCF8574_LCDGOTO(LCDLineNumberTwo, row);
			screen.print("   ");
			screen.PCF8574_LCDGOTO(LCDLineNumberThree, row);
			screen.print("   ");
			screen.PCF8574_LCDGOTO(LCDLineNumberFour, row);
			screen.print("   ");
			
			break;
		
		case '0':
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(LCDLineNumberThree, row);
			
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(LCDLineNumberFour, row);
			
			screen.PCF8574_LCDPrintCustomChar(5);
			screen.PCF8574_LCDPrintCustomChar(4);
			screen.PCF8574_LCDPrintCustomChar(3);
			
			break;
		
		case '1':
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDGOTO(LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDGOTO(LCDLineNumberThree, row);
			
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDGOTO(LCDLineNumberFour, row);
			
			screen.PCF8574_LCDPrintCustomChar(5);
			screen.PCF8574_LCDPrintCustomChar(4);
			screen.PCF8574_LCDPrintCustomChar(3);
			
			break;
		
		case '2':
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(LCDLineNumberThree, row);
			
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.print("  ");
			screen.PCF8574_LCDGOTO(LCDLineNumberFour, row);
			
			screen.PCF8574_LCDPrintCustomChar(5);
			screen.PCF8574_LCDPrintCustomChar(4);
			screen.PCF8574_LCDPrintCustomChar(3);
			
			break;
		
		case '3':
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(LCDLineNumberThree, row);
			
			screen.print("  ");
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(LCDLineNumberFour, row);
			
			screen.PCF8574_LCDPrintCustomChar(5);
			screen.PCF8574_LCDPrintCustomChar(4);
			screen.PCF8574_LCDPrintCustomChar(3);
			
			break;
		
		case '4':
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(LCDLineNumberThree, row);
			
			screen.print("  ");
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(LCDLineNumberFour, row);
			
			screen.print("  ");
			screen.PCF8574_LCDPrintCustomChar(3);
			
			break;
		
		case '5':
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(LCDLineNumberThree, row);
			
			screen.print("  ");
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(LCDLineNumberFour, row);
			
			screen.PCF8574_LCDPrintCustomChar(5);
			screen.PCF8574_LCDPrintCustomChar(4);
			screen.PCF8574_LCDPrintCustomChar(3);
			
			break;
		
		case '6':
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(LCDLineNumberThree, row);
			
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(LCDLineNumberFour, row);
			
			screen.PCF8574_LCDPrintCustomChar(5);
			screen.PCF8574_LCDPrintCustomChar(4);
			screen.PCF8574_LCDPrintCustomChar(3);
			break;
		
		case '7':
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(LCDLineNumberThree, row);
			
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDGOTO(LCDLineNumberFour, row);
			
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(5);
			screen.PCF8574_LCDSendChar(' ');
			
			break;
		
		case '8':
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(LCDLineNumberThree, row);
			
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(LCDLineNumberFour, row);
			
			screen.PCF8574_LCDPrintCustomChar(5);
			screen.PCF8574_LCDPrintCustomChar(4);
			screen.PCF8574_LCDPrintCustomChar(3);
			break;
		
		case '9':
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(LCDLineNumberThree, row);
			
			screen.print("  ");
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(LCDLineNumberFour, row);
			
			screen.PCF8574_LCDPrintCustomChar(5);
			screen.PCF8574_LCDPrintCustomChar(4);
			screen.PCF8574_LCDPrintCustomChar(3);
			
			break;
		
		case '-':
			screen.print("   ");
			screen.PCF8574_LCDGOTO(LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(LCDLineNumberThree, row);
			
			screen.print("   ");
			screen.PCF8574_LCDGOTO(LCDLineNumberFour, row);
			
			screen.print("   ");
			
			break;
	}
	
}


//enables all 4 relays for a short time, slightly staggered
void trigger_relays(){
	screen.PCF8574_LCDBackLightSet(false);
	digitalWrite(BUZZER, HIGH);
	
	digitalWrite(RELAY_1, HIGH);
	delay(20);
	digitalWrite(RELAY_2, HIGH);
	delay(20);
	digitalWrite(RELAY_3, HIGH);
	delay(20);
	digitalWrite(RELAY_4, HIGH);
	delay(500);
	
	digitalWrite(RELAY_1, LOW);
	delay(20);
	digitalWrite(RELAY_2, LOW);
	delay(20);
	digitalWrite(RELAY_3, LOW);
	delay(20);
	digitalWrite(RELAY_4, LOW);
	
	digitalWrite(BUZZER, LOW);
	screen.PCF8574_LCDBackLightSet(true);
}


//load offset and scale settings from eeprom
void load_settings(){
	Wire.beginTransmission(EEPROM_ADRESS);
	Wire.write(0); //set the adress to 0x00
	Wire.endTransmission(false);
	
	Wire.requestFrom(EEPROM_ADRESS, 8, true); //read 8 bytes from the eeprom: 4 for offset and 4 for scale
	
	while(Wire.available()){
		uint8_t read_bytes[4];
		read_bytes[0] = Wire.read(); //read the first 4 bytes
		read_bytes[1] = Wire.read(); //for loops are evil
		read_bytes[2] = Wire.read(); //yes, i could use Wire.readBytes()
		read_bytes[3] = Wire.read(); //no, i dont know how to use it
		
		long offset = 0;
		
		memcpy(&offset, read_bytes, 4); //copy the array to the var as bytes
		
		hx711.set_offset(offset);
		
		read_bytes[0] = Wire.read(); //read the other 4 bytes
		read_bytes[1] = Wire.read();
		read_bytes[2] = Wire.read();
		read_bytes[3] = Wire.read();
		
		float scale = 0.0;
		
		memcpy(&scale, read_bytes, 4); //copy the array again
		
		hx711.set_scale(scale);
	}
}


//store offset and scale settings to eeprom
void store_settings(){
	Wire.beginTransmission(EEPROM_ADRESS);
	Wire.write(0); //set the adress to 0x00
	uint8_t write_bytes[4];
	long offset = hx711.get_offset();
	float scale = hx711.get_scale();
	
	memcpy(write_bytes, &offset, 4); //copy the var to the array as bytes
	
	Wire.write(write_bytes[0]); //write to the eeprom
	Wire.write(write_bytes[1]);
	Wire.write(write_bytes[2]);
	Wire.write(write_bytes[3]);
	
	memcpy(write_bytes, &scale, 4); //rinse
	
	Wire.write(write_bytes[0]); //and repeat
	Wire.write(write_bytes[1]);
	Wire.write(write_bytes[2]);
	Wire.write(write_bytes[3]);
	
	Wire.endTransmission();
}
