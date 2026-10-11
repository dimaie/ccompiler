#ifndef ARDUINO_UNO_H
#define ARDUINO_UNO_H

/* =========================================================================
 * Arduino Uno (ATmega328P) Hardware Definitions
 * ========================================================================= */

/* I/O Register Addresses in Data Space (SRAM Memory-Mapped) */

/* Port B (Digital Pins 8 - 13): PB5 is Pin 13 / Built-in LED */
#define PINB_REG   0x23
#define DDRB_REG   0x24
#define PORTB_REG  0x25

/* Port C (Analog Pins A0 - A5) */
#define PINC_REG   0x26
#define DDRC_REG   0x27
#define PORTC_REG  0x28

/* Port D (Digital Pins 0 - 7) */
#define PIND_REG   0x29
#define DDRD_REG   0x2A
#define PORTD_REG  0x2B

/* Built-in LED on Arduino Uno is PB5 (Digital Pin 13) */
#define LED_PIN 5

#endif /* ARDUINO_UNO_H */
