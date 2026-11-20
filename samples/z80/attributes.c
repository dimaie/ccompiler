/* ZX Spectrum 48K: fill the 32 x 24 attribute cells with coloured stripes. */
int main(void) {
  char *attributes = 22528;
  for (int row = 0; row < 24; row++) {
    for (int column = 0; column < 32; column++) {
      attributes[row * 32 + column] = (row & 7) * 8 + 7;
    }
  }
  asm("ld a,1\nout (254),a");
  return 0;
}
