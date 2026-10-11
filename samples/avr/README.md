# Arduino Uno AVR Blink Sample

This sample demonstrates compiling bare-metal C code for the **Microchip ATmega328P** (the microcontroller powering the **Arduino Uno**), assembling it, linking it, and outputting an **Intel HEX (`.hex`)** firmware file using **CCompiler**.

---

## 1. Hardware Details

- **Target Microcontroller**: ATmega328P (8-bit AVR RISC architecture)
- **Clock Speed**: 16 MHz
- **Built-in LED**: Digital Pin 13 is connected to **Port B, Bit 5 (PB5)**.
- **I/O Registers**:
  - `DDRB` (Data Direction Register B): I/O port `0x04`
  - `PORTB` (Data Register B): I/O port `0x05`
  - `PINB` (Input Pins B): I/O port `0x03` (writing 1 to PINB toggles PORTB bit on ATmega328P)

---

## 2. Compiling to Intel HEX

Run the compiler specifying `-t avr`:

```bash
# From repository root:
./build/c_compiler.exe -t avr -I samples/avr samples/avr/blink.c -o samples/avr/blink.hex
```

This performs the complete embedded toolchain sequence:
1. **Frontend**: Preprocesses `blink.c` and parses AST.
2. **Codegen**: Generates ATmega328P assembly instructions.
3. **AVR Assembler**: Encodes AVR machine instructions into Flash words.
4. **AVR Linker**:
   - Prepends the reset vector at Flash address `0x0000` (`jmp __init`).
   - Inserts CRT0 startup runtime (sets stack pointer to `RAMEND` `0x08FF`, clears `r1`, and calls `main`).
   - Resolves all symbol relocations.
   - Emits an **Intel HEX (`.hex`)** file.

---

## 3. Uploading to Arduino Uno

Connect your Arduino Uno via USB, determine its COM port (e.g. `COM7` on Windows, or `/dev/ttyACM0` on Linux), and flash using `avrdude`.

When using `avrdude` bundled with Arduino IDE, pass the config file with `-C`:

```powershell
# Using avrdude from Arduino installation (e.g. C:\Arduino_1.8):
& "C:\Arduino_1.8\hardware\tools\avr\bin\avrdude.exe" `
    -C "C:\Arduino_1.8\hardware\tools\avr\etc\avrdude.conf" `
    -v -p m328p -c arduino -P COM7 -b 115200 -D `
    -U flash:w:samples/avr/blink.hex:i
```

Once uploaded, the built-in "L" LED on Digital Pin 13 will start blinking!
