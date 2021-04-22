#include <avr/io.h>
#define F_CPU 16000000UL
#include <LiquidCrystal.h>
#include <util/delay.h>
#include <math.h>

#define LCD_REFRESH_RATE 6249
#define second_write_command 0x80
#define second_read_command 0x81
#define minute_write_command 0x82
#define minute_read_command 0x83
#define hour_write_command 0x84
#define hour_read_command 0x85
#define minute 42
#define SCLK 3
#define CE 1
#define IO 2

LiquidCrystal lcd(7, 6, 5, 4, 3, 2);

enum clock_state {SET, TIME, RESET};
enum clock_state state = RESET;

enum set_options {HOUR, MINUTE};
enum set_options option = HOUR;

int hour_digits[2] = {0,0};
int minute_digits[2] = {0,0};

void write_time_keeper_data(int command, int data) {
	
	PORTC &= ~(1 << SCLK);
	PORTC |= (1 << CE);
	
	//Write "write" command to first 8 bits
	for (int i = 0; i < 8; i++) {
		if ((command & (1 << i)) > 0) {
			PORTC |= (1 << IO);
			} else {
			PORTC &= ~(1 << IO);
		}
		PORTC |= (1 << SCLK); //SCLK rising edge
		PORTC &= ~(1 << SCLK); //SCLK falling edge
	}
	
	//Write data on rising edge
	for (int i = 0; i < 8; i++) {
		if ((data & (1 << i))) {
			PORTC |= (1 << IO);
			} else {
			PORTC &= ~(1 << IO);
		}
		PORTC |= (1 << SCLK); //SCLK rising edge
		PORTC &= ~(1 << SCLK); //SCLK falling edge
	}
	
	PORTC &= ~(1 << IO);
	PORTC &= ~(1 << CE);
}

int read_time_keeper_data(int command) {
	PORTC &= ~(1 << SCLK); //SCLK = 0
	PORTC |= (1 << CE); //CE = 1
	
	//Write "read" command to first 8 bits
	for (int i = 0; i < 8; i++) {
		if ((command & (1 << i)) > 0) {
			PORTC |= (1 << IO);
			} else {
			PORTC &= ~(1 << IO);
		}
		PORTC |= (1 << SCLK); //SCLK rising edge
		if (i == 7) {
			DDRC &= ~(1 << IO);
		}
		PORTC &= ~(1 << SCLK); //SCLK falling edge
	}
	
	//Read data on falling edge
	int data = 0;
	for (int i = 0; i < 8; i++) {
		if ((PINC & (1 << IO))) {
			data |= (1 << i);
		}
		PORTC |= (1 << SCLK); //SCLK rising edge
		PORTC &= ~(1 << SCLK); //SCLK falling edge
	}
	
	DDRC |= (1 << IO);
	PORTC &= ~(1 << IO);
	PORTC &= ~(1 << CE);
	
	return data;
}

void write_time() {
	//Stop time keeper
	write_time_keeper_data(second_write_command,1 << 7);
	//Write hour
	int hour_data = 0 | (hour_digits[0] << 4) | (hour_digits[1] << 0);
	write_time_keeper_data(hour_write_command, hour_data);
	//Write minute
	int minute_data = 0 | (minute_digits[0] << 4) | (minute_digits[1] << 0);
	write_time_keeper_data(minute_write_command, minute_data);
	//Reset second to 0 and start time keeper
	write_time_keeper_data(second_write_command, 0);
}

void read_time() {
	//Read hour data
	int hour_data = read_time_keeper_data(hour_read_command);
	hour_digits[0] = (hour_data & (7 << 4)) >> 4;
	hour_digits[1] = hour_data & (15 << 0);
	
	//Read minute data
	int minute_data = read_time_keeper_data(minute_read_command);
	minute_digits[0] = (minute_data & (7 << 4)) >> 4;
	minute_digits[1] = minute_data & (15 << 0);
}

void print_LCD() {
	lcd.clear();
	if (state == SET) {
		lcd.print("Set Mode: ");
		if (option == HOUR) {
			lcd.print("hour");
			} else {
			lcd.print("minute");
		}
		} else {
		lcd.print("Time Mode");
	}
	lcd.setCursor(0,1);
	lcd.print(hour_digits[0]);
	lcd.print(hour_digits[1]);
	lcd.print(":");
	lcd.print(minute_digits[0]);
	lcd.print(minute_digits[1]);
}

void change_state() {
	if (state == SET) {
		write_time();
		state = TIME;
		} else {
		state = SET;
	}
}

void change_option() {
	if (option == HOUR) {
		option = MINUTE;
		} else {
		option = HOUR;
	}
}

void adjust_light() {
	switch(ADCH) {
		case 0:
		OCR2A = 4;
		break;
		case 1:
		OCR2A = 8;
		break;
		case 2:
		OCR2A = 16;
		break;
		case 3:
		OCR2A = 32;
		break;
		case 4:
		OCR2A = 64;
		break;
		case 5:
		OCR2A = 128;
		break;
		default:
		OCR2A = 192;
	}
}

int main(void)
{
	/* Setting Up I/O ports for LCD and time keeper */
	
	lcd.begin(16,2);
	DDRC |= 7 << 1;
	DDRB &= ~(1 << DDB0); //Increase button
	DDRB &= ~(1 << DDB1); //Change SET/TIME mode button
	DDRB &= ~(1 << DDB1); //Change HOUR/MINUTE button

	/* Setting Up Timer1 for refreshing LCD */

	TCCR1B |= (1 << CS12); //TIMER 1 with prescale 256
	TCNT1 = 0;
	
	/* Setting up PWM */
	
	DDRB |= (1 << DDB3); //PB3 is output
	OCR2A = 128; // set PWM for 50% duty cycle
	TCCR2A |= (1 << COM2A1);  //clear OC2A on compare
	TCCR2A |= (1 << WGM21) | (1 << WGM20); // set fast PWM Mode
	TCCR2B |= (1 << CS21); // set prescaler to 8 and starts PWM
	
	/* Setting up ADC */
	
	ADCSRA |= (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Set ADC prescalar to 128 - 125KHz sample rate @ 16MHz
	ADMUX |= (1 << REFS0); // Set ADC reference to AVCC
	ADMUX |= (1 << ADLAR); // Adjust ADC to be 8-bit
	ADCSRA |= (1 << ADATE);  // Enable ADC Auto Triggle Enable
	ADCSRB = 0x00;	    // Set ADC to Free-Running Mode
	ADCSRA |= (1 << ADEN);  // Enable ADC
	ADCSRA |= (1 << ADSC);  // Start A2D Conversions
	
	while (1) {
		adjust_light();
		
		switch(state) {
			case SET:
			//Increase time button
			if (!(PINB & (1 << PINB0))) {
				if (option == HOUR) {
					hour_digits[1]++;
					if (hour_digits[1] > 9) {
						hour_digits[0]++;
						hour_digits[1] = 0;
					}
					if (hour_digits[0] * 10 + hour_digits[1] > 23) {
						hour_digits[0] = 0;
						hour_digits[1] = 0;
					}
					} else {
					minute_digits[1]++;
					if (minute_digits[1] > 9) {
						minute_digits[0]++;
						minute_digits[1] = 0;
					}
					if (minute_digits[0] * 10 + hour_digits[1] > 59) {
						minute_digits[0] = 0;
						minute_digits[1] = 0;
					}
				}
				print_LCD();
				_delay_ms(200);
			}
			
			//change option (HOUR/MINUTE) button
			if (!(PINB & (1 << PINB2))) {
				change_option();
				_delay_ms(200);
			}
			break;
			case TIME:
			read_time();
			break;
			case RESET:
			state = TIME;
			break;
			default:
			state = RESET;
		}
		
		//change mode (SET/TIME) button
		if (!(PINB & (1 << PINB1))) {
			change_state();
			_delay_ms(200);
		}
		
		//refresh LCD display
		if (TCNT1 >= LCD_REFRESH_RATE) {
			print_LCD();
			TCNT1 = 0;
		}
	}
}

