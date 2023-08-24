#include <AbleButtons.h>
#include <Wire.h>
#include <HD44780_LCD_PCF8574.h>
#include <HX711.h>
#include <EEPROM.h>

using Button = AblePullupClickerButton;
using ButtonList = AblePullupClickerButtonList;

//D5 - D8 BUTTONS
const int BUTTON_INC = 5;
const int BUTTON_DEC = 6;
const int BUTTON_STOP = 7;
const int BUTTON_TARE = 8;

Button button_inc(BUTTON_INC);
Button button_dec(BUTTON_DEC);
Button button_stop(BUTTON_STOP);
Button button_tare(BUTTON_TARE);

Button *buttons[] = {
	&button_inc,
	&button_dec,
	&button_stop,
	&button_tare
};
ButtonList button_list(buttons);

//D9 - D12 RELAYS
const int RELAY_1 = 9;
const int RELAY_2 = 10;
const int RELAY_3 = 11;
const int RELAY_4 = 12;

//D13 BUZZER/INDICATOR
const int BUZZER = 13;

//SCREEN
const int SCREEN_ADRESS = 39; //0x27
TwoWire wire;
HD44780LCD screen(4, 20, SCREEN_ADRESS, &wire);

//HX711
const int HX711_DT = 2;
const int HX711_SCL = 3;
HX711 hx711;

//OTHER CONFIG
const int HYSTERESIS = 5;

const unsigned long DELAY_RELAYS = 20;
const unsigned long DELAY_CAL_MENU = 50;

//VARIABLES
int set_weight = 100;
int weight = 0;
int weight_array[10];
byte weight_array_index = 0;

bool relays_active = false;

unsigned long beep_time = 0;
bool is_beeping = false;


void setup(){
	button_list.begin(); //setup buttons
	button_inc.setHeldTime(100);
	button_dec.setHeldTime(100);
	button_stop.setHeldTime(100);
	button_tare.setHeldTime(1000);
	
	pinMode(RELAY_1, OUTPUT); //setup relays
	pinMode(RELAY_2, OUTPUT);
	pinMode(RELAY_3, OUTPUT);
	pinMode(RELAY_4, OUTPUT);
	
	pinMode(BUZZER, OUTPUT); //setup buzzer
	
	pinMode(PIN_WIRE_SDA, INPUT_PULLUP); //setup I2C
	pinMode(PIN_WIRE_SCL, INPUT_PULLUP);
	Wire.begin(); //begin I2C
	
	delay(200); //wait for screen to start
	screen.PCF8574_LCDInit(HD44780LCD::LCDCursorTypeOff); //start screen
	screen.PCF8574_LCDClearScreen(); //clear the screen
	screen.PCF8574_LCDBackLightSet(true); //turn on the backlight
	screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberOne, 2);
	screen.print("Automatic  Scale");
	screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberTwo, 15);
	screen.print("rev 1");
	screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberThree, 0);
	screen.print("github.com/");
	screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberFour, 0);
	screen.print("FilipZaf3312/scale");
	
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
	
	beep(500);
	show_weight(); //start normal operation
}


void loop(){
	button_list.handle();
	handle_beep();
	
	//decrement
	if(button_dec.resetClicked() and set_weight > 0){
		set_weight -= 10;
		update_set_weight();
	}
	
	//fast decrement
	if(button_dec.isHeld() and set_weight > 0){
		set_weight -= 10;
		update_set_weight();
	}
	
	//increment
	if(button_inc.resetClicked() and set_weight < 990){
		set_weight += 10;
		update_set_weight();
	}
	
	//fast increment
	if(button_inc.isHeld() and set_weight < 990){
		set_weight += 10;
		update_set_weight();
	}
	
	if(button_stop.resetClicked()){
		//
	}
	
	//stop
	if(button_stop.isHeld()){
		set_relays();
		beep(500);
		delay(500);
		reset_relays();
		handle_beep();
	}
	
	//tare
	if(button_tare.resetClicked()){
		screen.PCF8574_LCDClearScreen();
		screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberTwo, 6);
		screen.print("Tare...");
		
		hx711.tare(25);
		
		show_weight();
	}
	
	//calibrate
	if(button_tare.isHeld()){
		button_list.handle();
		
		screen.PCF8574_LCDClearScreen();
		screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberTwo, 6);
		screen.print("Tare...");
		
		hx711.tare(25);
		button_list.handle();
		
		screen.PCF8574_LCDClearScreen();
		screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberTwo, 0);
		screen.print("Calibration");
		screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberThree, 0);
		screen.print("weight:");
		
		int cal_weight = 100;
		update_cal_weight(cal_weight);
		button_list.handle();
		
		while(not button_tare.isHeld()){
			button_list.handle();
			handle_beep();
			
			//decrement
			if(button_dec.resetClicked() and cal_weight > 0){
				cal_weight -= 10;
				update_cal_weight(cal_weight);
			}
			
			//fast decrement
			if(button_dec.isHeld() and cal_weight > 0){
				cal_weight -= 10;
				update_cal_weight(cal_weight);
				delay(DELAY_CAL_MENU);
			}
			
			//increment
			if(button_inc.resetClicked() and cal_weight < 990){
				cal_weight += 10;
				update_cal_weight(cal_weight);
			}
			
			//fast increment
			if(button_inc.isHeld() and cal_weight < 990){
				cal_weight += 10;
				update_cal_weight(cal_weight);
				delay(DELAY_CAL_MENU);
			}
			
			//cancel
			if(button_stop.isHeld()){
				show_weight();
				button_tare.resetClicked();
				button_list.handle();
				
				goto canceled;
			}
		}
		
		screen.PCF8574_LCDClearScreen();
		screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberTwo, 0);
		screen.print("Calibrating...");
		
		hx711.calibrate_scale(cal_weight, 25);
		store_settings();
		
		button_tare.resetClicked();
		button_list.handle();
		show_weight();
	}
	
	canceled:
	update_weight();
	
	//when the set weight is reached, enable the relays
	if(weight >= set_weight and !relays_active){
		set_relays();
		beep(500);
	}
	else if(weight < set_weight - HYSTERESIS and relays_active){
		reset_relays();
	}
}


//prepare the screen for displaying weight
void show_weight(){
	screen.PCF8574_LCDClearScreen();
	//draw the vertical seperating line
	screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberOne, 9);
	screen.PCF8574_LCDPrintCustomChar(6);
	screen.PCF8574_LCDPrintCustomChar(2);
	screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberTwo, 9);
	screen.PCF8574_LCDPrintCustomChar(6);
	screen.PCF8574_LCDPrintCustomChar(2);
	screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberThree, 9);
	screen.PCF8574_LCDPrintCustomChar(6);
	screen.PCF8574_LCDPrintCustomChar(2);
	screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberFour, 9);
	screen.PCF8574_LCDPrintCustomChar(6);
	screen.PCF8574_LCDPrintCustomChar(2);
	
	update_weight();
	update_set_weight();
}


//write the weight value to the screen
void update_weight(){
	weight_array[weight_array_index] = hx711.get_units(1); //read the sensor
	
	weight_array_index ++;
	if(weight_array_index > 9){
		weight_array_index = 0;
	}
	
	weight = 0; //get the average of the last 10 readings
	for(byte i = 0; i < 10; i++){
		weight += weight_array[i];
	}
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
	
	beep(50);
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
	
	beep(50);
}


//set all relays to "on"
void set_relays(){
	relays_active = true;
	digitalWrite(RELAY_1, HIGH);
	delay(DELAY_RELAYS);
	digitalWrite(RELAY_2, HIGH);
	delay(DELAY_RELAYS);
	digitalWrite(RELAY_3, HIGH);
	delay(DELAY_RELAYS);
	digitalWrite(RELAY_4, HIGH);
}


//turn all relays back to "off"
void reset_relays(){
	relays_active = false;
	digitalWrite(RELAY_1, LOW);
	delay(DELAY_RELAYS);
	digitalWrite(RELAY_2, LOW);
	delay(DELAY_RELAYS);
	digitalWrite(RELAY_3, LOW);
	delay(DELAY_RELAYS);
	digitalWrite(RELAY_4, LOW);
}


void beep(unsigned long time){
	is_beeping = true;
	beep_time = millis() + time;
	digitalWrite(BUZZER, HIGH);
}


void handle_beep(){
	if(is_beeping and millis() >= beep_time){
		is_beeping = false;
		digitalWrite(BUZZER, LOW);
	}
}


//prints a large character to the screen
void print_big_char(char digit, int row){
	screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberOne, row);
	
	switch(digit){
		default:
			screen.print("   ");
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberTwo, row);
			screen.print("   ");
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberThree, row);
			screen.print("   ");
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberFour, row);
			screen.print("   ");
			
			break;
		
		case '0':
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberThree, row);
			
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberFour, row);
			
			screen.PCF8574_LCDPrintCustomChar(5);
			screen.PCF8574_LCDPrintCustomChar(4);
			screen.PCF8574_LCDPrintCustomChar(3);
			
			break;
		
		case '1':
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberThree, row);
			
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberFour, row);
			
			screen.PCF8574_LCDPrintCustomChar(5);
			screen.PCF8574_LCDPrintCustomChar(4);
			screen.PCF8574_LCDPrintCustomChar(3);
			
			break;
		
		case '2':
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberThree, row);
			
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.print("  ");
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberFour, row);
			
			screen.PCF8574_LCDPrintCustomChar(5);
			screen.PCF8574_LCDPrintCustomChar(4);
			screen.PCF8574_LCDPrintCustomChar(3);
			
			break;
		
		case '3':
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberThree, row);
			
			screen.print("  ");
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberFour, row);
			
			screen.PCF8574_LCDPrintCustomChar(5);
			screen.PCF8574_LCDPrintCustomChar(4);
			screen.PCF8574_LCDPrintCustomChar(3);
			
			break;
		
		case '4':
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberThree, row);
			
			screen.print("  ");
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberFour, row);
			
			screen.print("  ");
			screen.PCF8574_LCDPrintCustomChar(3);
			
			break;
		
		case '5':
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberThree, row);
			
			screen.print("  ");
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberFour, row);
			
			screen.PCF8574_LCDPrintCustomChar(5);
			screen.PCF8574_LCDPrintCustomChar(4);
			screen.PCF8574_LCDPrintCustomChar(3);
			
			break;
		
		case '6':
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberThree, row);
			
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberFour, row);
			
			screen.PCF8574_LCDPrintCustomChar(5);
			screen.PCF8574_LCDPrintCustomChar(4);
			screen.PCF8574_LCDPrintCustomChar(3);
			break;
		
		case '7':
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberThree, row);
			
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberFour, row);
			
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(5);
			screen.PCF8574_LCDSendChar(' ');
			
			break;
		
		case '8':
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberThree, row);
			
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.PCF8574_LCDSendChar(' ');
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberFour, row);
			
			screen.PCF8574_LCDPrintCustomChar(5);
			screen.PCF8574_LCDPrintCustomChar(4);
			screen.PCF8574_LCDPrintCustomChar(3);
			break;
		
		case '9':
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDPrintCustomChar(6);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberThree, row);
			
			screen.print("  ");
			screen.PCF8574_LCDPrintCustomChar(2);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberFour, row);
			
			screen.PCF8574_LCDPrintCustomChar(5);
			screen.PCF8574_LCDPrintCustomChar(4);
			screen.PCF8574_LCDPrintCustomChar(3);
			
			break;
		
		case '-':
			screen.print("   ");
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberTwo, row);
			
			screen.PCF8574_LCDPrintCustomChar(7);
			screen.PCF8574_LCDPrintCustomChar(0);
			screen.PCF8574_LCDPrintCustomChar(1);
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberThree, row);
			
			screen.print("   ");
			screen.PCF8574_LCDGOTO(HD44780LCD::LCDLineNumberFour, row);
			
			screen.print("   ");
			
			break;
	}
	
}


//load offset and scale settings from eeprom
void load_settings(){
	long offset = 0;
	float scale = 0.0;
	
	EEPROM.get(0, offset);
	EEPROM.get(4, scale);
	
	hx711.set_offset(offset);
	hx711.set_scale(scale);
}


//store offset and scale settings to eeprom
void store_settings(){
	long offset = hx711.get_offset();
	float scale = hx711.get_scale();
	
	EEPROM.put(0, offset);
	EEPROM.put(4, scale);
}
