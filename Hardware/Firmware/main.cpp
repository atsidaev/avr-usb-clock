#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/delay.h>

#include "SevenSegmentArray/SevenSegmentArray.h"

#define bool char
#define TRUE 1
#define FALSE 0

#define USART_BAUDRATE 9600
#define BAUD_PRESCALE (((F_CPU / (USART_BAUDRATE * 16UL))) - 1)

#define START_SYMBOL '{'
#define END_SYMBOL '}'

#define HELL_KNOWS_WHY_THIS_DELTA_IS_NEEDED 7000
#define WAIT_FOR_HALF_SECOND (0x1000 - 15625*2 - HELL_KNOWS_WHY_THIS_DELTA_IS_NEEDED)

#define BRIGHTNESS 12 // 0 for maximum
#define EMPTY 0xFF

#define DOT_UPPER 0x02
#define DOT_BOTTOM 0x01

#define MESSAGES_COUNT 5
#define TIMESLOT_COUNT 10

unsigned char pins[] = { 3, 4, 5, 6 };
SevenSegmentArray seg(COMMON_CATHODE, &PORTD, &PORTB, pins, sizeof(pins));

volatile char messages[MESSAGES_COUNT][4];

volatile char time[] = { 2, 3, 5, 9 };
const char digit_limits[] = { 3, 10, 6, 10 };
volatile unsigned char seconds = 0;
volatile unsigned char is_full_second = 0;

volatile unsigned char timeline[TIMESLOT_COUNT] = { 0, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY, EMPTY };
volatile unsigned char timeline_delay[TIMESLOT_COUNT] = { 10, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
volatile char wait_for;

#define COMMAND_BUFFER_LENGTH 16
volatile unsigned char commandBuffer[COMMAND_BUFFER_LENGTH];
volatile unsigned char commandBufferPos = 0;
volatile unsigned char commandCompleted = 0;

ISR(TIMER1_OVF_vect)
{
	unsigned char i;
	
	TCNT1=WAIT_FOR_HALF_SECOND;
	is_full_second = !is_full_second;
	if (is_full_second)
	{
		if (++seconds >= 60)
		{
			seconds = 0;
			for (i = 3; i >= 0; i--)
			{
				time[i]++;
				if (time[i] >= digit_limits[i])
					time[i] = 0;
				else
					break;
			}
			
			if (time[0] == 2 && time[1] >= 4) // Time became 24:00
				time[0] = time[1] = time[2] = time[3] = 0;
		}
	}
	// Blinking dots
	PORTA ^= DOT_UPPER | DOT_BOTTOM;
	if (wait_for > 0)
		wait_for--;
}

ISR(USART_RX_vect){
	unsigned char value = UDR;
	
	// If previous command not executed
	if (commandCompleted == 1)
		return;
		
	if (value == START_SYMBOL)
		commandBufferPos = 0;
	
	if (value == END_SYMBOL)
		commandCompleted = 1;
		
	commandBuffer[commandBufferPos++] = value;
	
	if (commandBufferPos == COMMAND_BUFFER_LENGTH)
		commandBufferPos = 0;
	
	// Echo
	UDR = value;
}

void init_timer()
{
	TCNT1=WAIT_FOR_HALF_SECOND;
	TCCR1B=0x03;
	TIMSK=_BV(TOIE1);
}

void init_usart(){
   // Set baud rate
   UBRRL = BAUD_PRESCALE;
   UBRRH = (BAUD_PRESCALE >> 8); 

  // Enable receiver and transmitter and receive complete interrupt 
  UCSRA = 0;
  UCSRB = ((1<<TXEN) | (1<<RXEN) | (1<<RXCIE));
  UCSRC = (1<<UCSZ0)|(1<<UCSZ1);  // Set frame format: 8data, 1stop bit 
}

// when is_num is TRUE then array contains 0..9 numbers instead of ascii codes
void print(volatile char* str, bool is_num, bool show_dots)
{
	for (unsigned char i = 0; i < 4; i++)
	{
		char ascii = is_num ? (str[i] < 10 && str[i] >= 0 ? str[i] + '0' : ' ') : (str[i] >= 0x20 && str[i] < 0x60 ? str[i] : ' ');
		seg.output_segments(ascii);
		for (int j = 0; j < 100; j++)
		{
			for (int k = 0; k < BRIGHTNESS; k++) // loop for reducing led intensivity. Set BRIGHTNESS to 0 to avoid.
			{
				// TODO: reimplement this magic in 7seg lib:
				//PORTD |= letters_anc[j % 4] ^ 0xFF; // set one of anchors to 1. 
			}
			seg.select_digit(i);
			
			if (!show_dots)
				PORTA &= ~(DOT_UPPER | DOT_BOTTOM);
		}
	}
}

void check_and_exec_command()
{
	if (commandCompleted && commandBuffer[0] == START_SYMBOL)
	{
		// Disable interrupts - we are modifying volatile variables.
		cli();
		switch (commandBuffer[1])
		{
			case 'S': // Set time
				time[0] = commandBuffer[2] - '0';
				time[1] = commandBuffer[3] - '0';
				time[2] = commandBuffer[4] - '0';
				time[3] = commandBuffer[5] - '0';
				seconds = (commandBuffer[6] - '0') * 10 + (commandBuffer[7] - '0');
				break;
			case 'R': // Reset all settings
				time[0] = 2; time[1] = 3; time[2] = 5; time[3] = 9;
				seconds = 0;
				for (int i = 0; i < TIMESLOT_COUNT; i++)
				{
					timeline[i] = EMPTY;
					timeline_delay[i] = 0;
				}
				timeline[0] = 0;
				timeline_delay[0] = 10;
				for (int i = 0; i < MESSAGES_COUNT; i++)
				{
					messages[i][0] = ' '; messages[i][1] = ' '; messages[i][2] = ' '; messages[i][3] = ' ';
				}
				wait_for = 0;
				break;
			case 'D': // Set delay
			case 'T': // Set time slot value
			{
				unsigned char slot = 0, value = 0;
				int pos = 2;
				while (commandBuffer[pos] != ',' && commandBuffer[pos] >= '0' && commandBuffer[pos] <= '9')
				{
					slot *= 10;
					slot += commandBuffer[pos] - '0';
					pos++;
				}
				if (commandBuffer[pos++] != ',') break; // Invalid command
				while (commandBuffer[pos] != END_SYMBOL && commandBuffer[pos] >= '0' && commandBuffer[pos] <= '9')
				{
					value *= 10;
					value += commandBuffer[pos] - '0';
					pos++;
				}
				if (commandBuffer[pos] != END_SYMBOL) break; // Invalid command
				
				if (commandBuffer[1] == 'D')
					timeline_delay[slot] = value;

				if (commandBuffer[1] == 'T')
					timeline[slot] = value;
					
				break;
			}
			case 'M': // Set message
			{
				unsigned char slot = 0;
				int pos = 2;
				while (commandBuffer[pos] != ',' && commandBuffer[pos] >= '0' && commandBuffer[pos] <= '9')
				{
					slot *= 10;
					slot += commandBuffer[pos] - '0';
					pos++;
				}
				if (commandBuffer[pos] != ',' || commandBuffer[pos + 5] != END_SYMBOL || slot > MESSAGES_COUNT || slot == 0) break; // Invalid command
				pos++;
				
				for (int i = 0; i < 4; i++)
					messages[slot-1][i] = commandBuffer[pos + i];
				
				break;
			}
		}
		commandCompleted = 0;
		sei();
	}
}

int main(void)
{
	init_timer();
	init_usart();
	sei();
	
	DDRA = DOT_BOTTOM | DOT_UPPER; // PA0&PA1 - outputs

	while (1) {
		int i = 0;
		for (char i = 0; i < TIMESLOT_COUNT; i++)
		{
			if (timeline[i] != EMPTY && timeline[i] <= MESSAGES_COUNT)
			{
				wait_for = timeline_delay[i];
				while (wait_for > 0)
				{
					if (timeline[i] == 0)
						print(time, TRUE, TRUE);
					else
						print(messages[timeline[i] - 1], FALSE, FALSE);
					
					check_and_exec_command();
				}
			}
		}
	}
}
