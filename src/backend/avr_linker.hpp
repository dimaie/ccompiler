#ifndef AVR_LINKER_HPP
#define AVR_LINKER_HPP

#include "avr_assembler.hpp"
#include <string>
#include <vector>

/**
 * @brief Standalone AVR (ATmega328P / Arduino Uno) Linker
 * Resolves symbol relocations across modules, prepends CRT0 / vector table,
 * and produces Intel HEX (.hex) flash images.
 */
class AVRLinker {
public:
  static bool link(const std::vector<AVRAssembledModule> &modules,
                   const std::string &output_filepath,
                   std::string &error_msg,
                   bool include_crt0 = true);

  static std::string generate_intel_hex(const std::vector<uint8_t> &flash_bytes, uint16_t start_address = 0x0000);
};

#endif // AVR_LINKER_HPP
