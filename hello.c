#include <stdio.h>

#define MESSAGE "Hello, World from Antigravity C Compiler!"
#define DEBUG
#define DEBUG_MESSAGE "This is a debug message!"

int main() {
  puts(MESSAGE);
#ifdef DEBUG
  puts(DEBUG_MESSAGE);
  __asm__ volatile ("nop");
#endif
  return 0;
}
