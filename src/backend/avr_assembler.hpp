#ifndef AVR_ASSEMBLER_HPP
#define AVR_ASSEMBLER_HPP

#include "internal_assembler.hpp"
#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

enum class AVRRelocType {
  RelativeJump,     // rjmp / rcall: 12-bit signed relative word offset
  RelativeBranch,   // breq, brne, brlt, brge, etc.: 7-bit signed relative word offset
  AbsoluteCall,     // call / jmp: 22-bit absolute word address in 32-bit instruction
  LdsSts16,         // lds / sts: 16-bit SRAM data address in word 2
  ImmLo8,           // ldi lo8(sym): 8-bit low byte
  ImmHi8,           // ldi hi8(sym): 8-bit high byte
  ImmPmLo8,         // ldi pm_lo8(sym): 8-bit low byte of word address
  ImmPmHi8          // ldi pm_hi8(sym): 8-bit high byte of word address
};

struct AVRRelocation {
  size_t offset{0};              // Byte offset in text_bytes
  std::string symbol;            // Target label
  int addend{0};                 // Constant offset
  AVRRelocType type{AVRRelocType::RelativeJump};
};

struct AVRAssembledModule {
  std::vector<uint8_t> text_bytes;  // Program memory (Flash) code bytes (little-endian 16-bit words)
  std::vector<uint8_t> data_bytes;  // Initialized data bytes
  size_t bss_size{0};               // Uninitialized static RAM size

  std::unordered_map<std::string, uint32_t> symbol_offsets; // label in text_bytes
  std::unordered_map<std::string, uint32_t> data_offsets;   // label in data_bytes
  std::unordered_map<std::string, uint32_t> bss_offsets;    // label in bss

  std::vector<AVRRelocation> relocations;
  std::set<std::string> global_symbols;
  std::set<std::string> local_symbols;
};

/**
 * @brief Standalone AVR (ATmega328P / avr5) Assembler
 * Encodes GNU as / AT&T style AVR assembly instructions into native machine code.
 */
class AVRAssembler {
public:
  AVRAssembler() = default;

  AVRAssembledModule assemble(const std::string &assembly_source);

private:
  int parse_reg(const std::string &token) const;
  int64_t parse_int(const std::string &token) const;
  void trim(std::string &s) const;
  std::vector<std::string> tokenize_operands(const std::string &op_str) const;

  void emit_word(std::vector<uint8_t> &dest, uint16_t word);
  void emit_dword(std::vector<uint8_t> &dest, uint16_t word1, uint16_t word2);
};

#endif // AVR_ASSEMBLER_HPP
