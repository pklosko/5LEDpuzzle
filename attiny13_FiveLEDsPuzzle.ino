/**
 * 
 * Five LEDs Puzzle 
 * 
 * see https://www.klosko.net/blog/=article=
 * Petr KLOSKO - www.klosko.net - 24th January 2021
 * 
 * Version : 20210124
 *      - Port David's sketch to ATtiny13 
 *      - Add some fetures
 *          - DeepSleep
 *          - Random/Last init value of LEDs
 *          - Time limit for the solution
 *          - Bargraph at Start
 *          
 *   Arduino IDE settings [Microcore]:
 *          - ATtiny13
 *          - 128kHz internal
 *          - LTO enabled
 *          - BOD disabled
 *          - Micros disabled
 *          - EEPROM not retained
 *   
 *   Fuse settings:
 *          -U lfuse:w:0x7b:m -U hfuse:w:0xff:m 
 *   
 * CC BY 4.0
 * Licensed under a Creative Commons Attribution 4.0 International license: 
 * http://creativecommons.org/licenses/by/4.0/
 * 
 * Oringinal puzzle sketch sketch & schematics : 
 * Five LEDs Puzzle v4 - see http://www.technoblogy.com/show?3D8S
 * David Johnson-Davies - www.technoblogy.com - 9th January 2021
 * ATtiny85 @ 1 MHz (internal oscillator; BOD disabled)
*/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

#ifndef F_CPU
# define        F_CPU           (128000UL) // 128 kHz
#endif  /* !F_CPU */

#define DELAY  50
unsigned long Timeout = 15000; // 15 seconds default
uint8_t limit = false;         // no limit solution time

unsigned long Start;


volatile uint8_t ddr __attribute__((section(".noinit")));
volatile uint8_t dir __attribute__((section(".noinit")));

// Pin change interrupt is just used to wake us up
ISR (INT0_vect) {
  GIMSK &= ~(1<<INT0);                    // Disable Pin Change Interrupts
  inic();
}

// Calculate CRC - based on Dallas crc8 code
static inline uint8_t _crc8(uint8_t crc, uint8_t data){
  uint8_t i;
  crc = crc ^ data;
  for (i = 0; i < 8; i++) {
    if (crc & 0x01) {
      crc = (crc >> 1) ^ 0x8C;
    } else {
      crc >>= 1;
    }
  }
  return crc;
}

void bargraph_L(){
  for (uint8_t i=0;i<6;i++){
    _delay_ms(DELAY);
    DDRB = 1<<i;
    PORTB = 0;   
  }
  DDRB = 0;
  PORTB = 0;
}

void bargraph_R(){
  for (uint8_t i=0;i<7;i++){
    _delay_ms(DELAY);
    DDRB = 1<<(5-i);
    PORTB = 0;   
  }
  DDRB = 0;
  PORTB = 0;
}

int sleepIn(void) {
  if (DDRB != 0){
    ddr = DDRB;
  }
  if (DIDR0 != 0x3F){
    dir = DIDR0; 
  }
  DDRB = 0;                           // All lights off
  PORTB = 0x1F;                       // Pullups on
  DIDR0 = 0x3F;            //Disable digital input buffers on all unused ADC0-ADC5 pins.
  ADCSRA &= ~(1<<ADEN);    //Disable ADC
  ACSR = (1<<ACD);         //Disable the analog comparator
  cli();          //Deactivate Interrupts as long as I change Bits      
  GIMSK |= (1<<INT0);                    // Enable Pin Change Interrupts
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);    // go to deep sleep
  MCUCR &= ~(1 << ISC00); //Set low level Interrupt
  MCUCR &= ~(1 << ISC01); //Set low level Interrupt
  sei();          //Activate Interrupts
 //go to sleep
  sleep_mode();
  //and wake up
  cli();          //Deactivate Interrupts
  return 1;
}

void inic(void) {
  uint8_t ddrb_init;
  DDRB = 0;
  DIDR0 = 0;
  PORTB = 0;
  ADCSRA &= ~(1<<ADEN);    //Disable ADC
  ACSR = (1<<ACD);         //Disable the analog comparator  
  GIFR = (1<<PCIF);        // Clear flag
  _delay_ms(10);
  bargraph_L();
// 0b - beacuse i want see switch position
  limit = ((PINB & 0b00010000) == 0);                                        // Button 5: time limit [timeout] for find the solution, Default=15000 [15sec]
  if      ((PINB & 0b00001000) == 0){Timeout = 30000;}                       // Button 4: longer timeout/limit [30sec]
  if      ((PINB & 0b00000100) == 0){Timeout = 10000;}                       // Button 3: longer timeout/limit [10sec]
  if      ((PINB & 0b00000010) == 0){ddrb_init = ((millis() / 100) & 0x1F);  // Button 2: pseudo random LEDs value
  }else if((PINB & 0b00000001) == 0){ddrb_init = ddr;                        // Button 1: last LEDs value
                               }else{ddrb_init = 0;}                         // zero LEDS value - default
  bargraph_R();
  DIDR0 = 0; 
  DDRB = ddrb_init;
  PORTB = 0;  
  Start = millis();
}

int main (void)
{
// SETUP
  inic();

// LOOP
  while(1){
    while (millis() - Start < Timeout) {
      for (uint8_t b=0; b<5; b++) {
        uint8_t d = DDRB;
        DDRB = d & ~(1<<b);
        PORTB |= 1<<b;
        _delay_ms(1);
        if (!(PINB & 1<<b)) {
          while (!(PINB & 1<<b));
          PORTB &= ~(1<<b);
          DDRB = d ^ ((!b || (d & ((1<<b)-1)) == 1<<(b-1))<<b);
          if (!limit){
            Start = millis();
          }
        } else {
          PORTB &= ~(1<<b);
          DDRB = d;
        }
        _delay_ms(10);
      }
    }
    // Achieve lowest possible power in sleep
    sleepIn();       
  }
}
