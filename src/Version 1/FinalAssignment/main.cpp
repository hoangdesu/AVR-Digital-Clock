/*
 * FinalAssignment.cpp
 *
 * RMIT University - School of Science and Technology
 * Assignment: Final Group Assignment
 * Submission date: Jan 26, 2021
 * Version 1: Basic Clock
 * Extra functionality: Temperature sensor
 * 
 * Author: Hoang Nguyen
 * Student ID: s3697305
 * 
 */ 

#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>

// For common cathode LED
unsigned char number[10] = {
	0b00111111, // 0
	0b00000110, // 1
	0b01011011, // 2
	0b01001111, // 3
	0b01100110, // 4
	0b01101101, // 5
	0b01111101, // 6
	0b00000111, // 7
	0b01111111, // 8
	0b01101111, // 9
};

// Declare modes
enum clock_mode {SET_hour, SET_minute, TIME};
enum clock_mode mode;

// Global variables to keep track of current time
int hour = 0;
int minute = 0;
volatile int second = 0;
volatile int tick = 0;

bool clock_init = true;
bool alarm_on = true;
bool display_second = false;

/* Set the alarm here */
int setAlarm = 7;				// set alarm time in seconds. Multiply by 60 to set alarm in minutes
int alarmTime = setAlarm + 5;	// activate alarm for 5 seconds

// Function headers
void Timer_Frequency(uint8_t freq);
void select_mode();
void set_hour_mode();
void set_minute_mode();
void time_mode();
void activate_alarm();
void init_temp_sensor();

int main(void)
{
    DDRB = 0xFF;	// set port B for digit and misc output
	DDRD = 0xFF;	// set port D for LED output
	DDRC = 0x00;	// set port C for input
	
	init_temp_sensor();
	
	Timer_Frequency(1); // set the timer frequency to 1Hz
	
    while (1) 
    {
		select_mode(); // press button 1 to cycle through the modes
		
		switch (mode)
		{
			case SET_hour:
				set_hour_mode();
				break;
				
			case SET_minute:
				set_minute_mode();
				break;
				
			case TIME:
				if (clock_init)
				{
					cli();
					_delay_ms(100);
					sei();
					clock_init = false;
				} 
				else
				{
					time_mode();
				}
				
				activate_alarm();
				break;
		}
	}
}

ISR(TIMER1_COMPA_vect)
{
	tick = 1;
	alarmTime--;
	PORTB ^= 1<<PORTB5;
}

void init_temp_sensor()
{
	ADCSRA |= (1<<ADPS2) | (1<<ADPS1) | (1<<ADPS0);	// Set ADC prescaler to 128 - 125KHz sample rate @16MHz
	ADMUX |= (1<<REFS0);							// Set ADC reference to AVCC
	ADMUX |= (1<<ADLAR);							// Adjust ADC to be 8-bit
	ADCSRA |= (1<<ADATE);							// Enable ADC Auto Trigger Enable
	ADCSRB = 0x00;									// Set ADC to Free-Running mode
	ADCSRA |= (1<<ADEN);							// Enable ADC
	ADCSRA |= (1<<ADSC);							// Start A2D Conversion
	ADCSRA |= (1<<ADIE);							// Enable ADC Interrupt
	sei();											// Enable Global Interrupt
	ADCSRA |= (1<<ADSC);							// Start A2D Conversion
}

ISR(ADC_vect)
{
	// Activate alarm when temperature exceeds 40 degrees
	if (ADCH > 40)
	{
		PORTB |= 1<<PORTB4;
	}
	else
	{
		PORTB &= ~(1<<PORTB4);
	}
}

void select_mode()
{
	// press button 1 to cycle through different modes
	if (!(PINC & (1<<PINC3)))
	{
		switch (mode)
		{
			case SET_hour:
				mode = SET_minute;
				_delay_ms(200);
			break;
			case SET_minute:
				mode = TIME;
				_delay_ms(200);
			break;
			case TIME:
				mode = SET_hour;
				_delay_ms(200);
			break;
		}
	}
}

void Timer_Frequency(uint8_t freq)
{
	//uint8_t freq = 1;
	TCCR1B |= (1<<WGM12) | (1<<CS12);	// CTC mode, prescaler 256
	TIMSK1 = 1<<OCIE1A;					// enable output compare A match interrupt
	OCR1A = (F_CPU/(freq*2*128)-1);		// 1Hz
}

void set_hour_mode()
{
	cli();					// clear interrupt bit
	clock_init = true;		// set clock back to init state
	second = 0;				// reset the second counter
	display_second = false;
	
	// press button 2 to decrease the hour. Allow cycling through 24 hours
	if (!(PINC & (1<<PINC1)))
	{
		hour--;
		if (hour < 0)
		{
			hour = 24;
		}
		_delay_ms(200);
	}
	
	// press button 3 to increase the hour. Allow cycling through 24 hours
	else if (!(PINC & (1<<PINC2)))
	{
		hour++;
		if (hour > 24)
		{
			hour = 0;
		}
		_delay_ms(200);
	}
	
	// Blink LEDs to indicate its current mode
	if (TCNT1 >= 0 && TCNT1 < OCR1A/2)
	{
		// Digit 1
		PORTB |= (1<<PORTB0) | (1<<PORTB1) | (1<<PORTB2) | (1<<PORTB3);		// turn off the digits (reset ports B0 to B3 as OUTPUT)
		PORTB &= ~(1<<PORTB0);												// turn on port B0 specifically for digit 1
		PORTD = number[hour/10];											// set the number to display on first digit
		_delay_ms(5);														// displaying digit 1 for 5ms
		PORTB |= 1<<PORTB0;													// turn off digit 1 after displaying

		// Digit 2
		PORTB |= (1<<PORTB0) | (1<<PORTB1) | (1<<PORTB2) | (1<<PORTB3);		// displaying any digit with the same principle above
		PORTB &= ~(1<<PORTB1);												// (multiplex)
		PORTD = number[hour%10];
		_delay_ms(5);
		PORTB |= 1<<PORTB1;
	}
	else if (TCNT1 >= OCR1A) {
		PORTB |= (1<<PORTB0) | (1<<PORTB1) | (1<<PORTB2) | (1<<PORTB3);		
	}
}

void set_minute_mode()
{
	// press button 2 to decrease the minute. Allow cycling through 60 minutes
	if (!(PINC & (1<<PINC1)))
	{
		minute--;
		if (minute < 0)
		{
			minute = 59;
		}
		_delay_ms(200);
	}
	
	// press button 3 to increase the minute. Allow cycling through 60 minutes
	else if (!(PINC & (1<<PINC2)))
	{
		minute++;
		if (minute > 59)
		{
			minute = 0;
		}
		_delay_ms(200);
	}
	
	// Blink digit 3 and 4
	if (TCNT1 >= 0 && TCNT1 < OCR1A/2)
	{
		// Digit 3
		PORTB |= (1<<PORTB0) | (1<<PORTB1) | (1<<PORTB2) | (1<<PORTB3);
		PORTB &= ~(1<<PORTB2);
		PORTD = number[minute/10];
		_delay_ms(5);
		PORTB |= 1<<PORTB2;
		
		// Digit 4
		PORTB |= (1<<PORTB0) | (1<<PORTB1) | (1<<PORTB2) | (1<<PORTB3);
		PORTB &= ~(1<<PORTB3);
		PORTD = number[minute%10];
		_delay_ms(5);
		PORTB |= 1<<PORTB3;
	}
	else if (TCNT1 >= OCR1A) {
		PORTB |= (1<<PORTB0) | (1<<PORTB1) | (1<<PORTB2) | (1<<PORTB3);
	}
}

void time_mode()
{
	// Detect if 1 second has elapsed from the timer interrupt
	if (tick == 1)
	{
		second++;
		tick = 0;
	}
	
	// if (second == 1)		// <- Use this condition to observe quicker. 1 minute = 1 second
	if (second > 59)		// increment minute
	{
		minute++;
		second = 0;
	}
	
	if (minute > 59)		// increment hour
	{
		hour++;
		minute = 0;
	}
	
	if (hour > 24)			// reset timer
	{
		hour = 0;
		minute = 0;
		second = 0;
	}
	
	// Switch between display modes
	if (!(PINC & (1<<PINC2)))
	{
		display_second = !display_second;
		_delay_ms(200);
	}
	
	// Normal mode: Display Hour:Minute
	// Second mode: Display elapsed seconds
	
	if (display_second == false)
	{
		// Digit 1
		PORTB |= (1<<PORTB0) | (1<<PORTB1) | (1<<PORTB2) | (1<<PORTB3);
		PORTB &= ~(1<<PORTB0);
		PORTD = number[hour/10];
		_delay_ms(5);
		PORTB |= 1<<PORTB0;
		
		// Digit 2
		PORTB |= (1<<PORTB0) | (1<<PORTB1) | (1<<PORTB2) | (1<<PORTB3);
		PORTB &= ~(1<<PORTB1);
		
		// Display the dot at digit 2 to observe the seconds
		if (TCNT1 <= OCR1A / 2)
		//if (second % 2 == 0) // Blink twice as slow ¯\_(?)_/¯
		{
			PORTD = number[hour%10] | (1<<PORTD7);
		}
		else
		{
			PORTD = number[hour%10];
		}
		_delay_ms(5);
		PORTB |= 1<<PORTB1;
		
		// Digit 3
		PORTB |= (1<<PORTB0) | (1<<PORTB1) | (1<<PORTB2) | (1<<PORTB3);
		PORTB &= ~(1<<PORTB2);
		PORTD = number[minute/10];
		_delay_ms(5);
		PORTB |= 1<<PORTB2;
		
		// Digit 4
		PORTB |= (1<<PORTB0) | (1<<PORTB1) | (1<<PORTB2) | (1<<PORTB3);
		PORTB &= ~(1<<PORTB3);
		PORTD = number[minute%10];
		_delay_ms(5);
		PORTB |= 1<<PORTB3;
	}
	else
	{
		// Digit 3
		PORTB |= (1<<PORTB0) | (1<<PORTB1) | (1<<PORTB2) | (1<<PORTB3);
		PORTB &= ~(1<<PORTB2);
		PORTD = number[second/10];
		_delay_ms(5);
		PORTB |= 1<<PORTB2;
		
		// Digit 4
		PORTB |= (1<<PORTB0) | (1<<PORTB1) | (1<<PORTB2) | (1<<PORTB3);
		PORTB &= ~(1<<PORTB3);
		PORTD = number[second%10];
		// Display the dot at digit 4
		if (TCNT1 <= OCR1A / 2)
		{
			PORTD |= (1<<PORTD7);
		}
		_delay_ms(5);
		PORTB |= 1<<PORTB3;
	}
}


void activate_alarm()
{	
	if (alarmTime == 0)
	{
		PORTB &= ~(1<<PORTB4);
		alarmTime = -1;
	}
	else if (alarmTime > 0 && alarmTime < 6)
	{
		// press button 2 to quickly disable the alarm
		if (!(PINC & (1<<PINC1)))
		{
			alarm_on = false;
		}
		
		if (alarm_on)
		{
			PORTB |= 1<<PORTB4;
		}
		else
		{
			PORTB &= ~(1<<PORTB4);
		}
	}
	
	// Press button 1 to toggle the alarm
	if (!(PINC & (1<<PINC1)))
	{
		PORTB ^= (1<<PORTB4);
		_delay_ms(100);
	}
}