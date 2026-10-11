/* =========================================================================
 * Arduino Uno LED Blink Example
 * Target: ATmega328P (16 MHz)
 * Hardware: Built-in LED on Digital Pin 13 (Port B, Bit 5)
 * ========================================================================= */

#include "arduino_uno.h"

/* Simple software delay calibrated for 16 MHz ATmega328P */
void delay_ms(int count) {
  int i;
  int j;
  for (i = 0; i < count; i = i + 1) {
    for (j = 0; j < 120; j = j + 1) {
      asm("nop");
    }
  }
}

int main() {
  /* 1. Configure Digital Pin 13 (PB5) as OUTPUT.
   *    In ATmega328P, DDRB is I/O address 0x04.
   *    The 'sbi' instruction sets bit 5 in I/O port 0x04. */
  asm("sbi 0x04, 5");

  /* 2. Main loop: toggle LED and delay */
  while (1) {
    /* In ATmega328P, writing a 1 to PINB (I/O port 0x03) toggles the PORTB bit!
     */
    asm("sbi 0x03, 5");

    delay_ms(500);
  }

  return 0;
}
