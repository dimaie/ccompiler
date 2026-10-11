#include "avr_linker.hpp"
#include <fstream>
#include <iomanip>
#include <sstream>

std::string AVRLinker::generate_intel_hex(const std::vector<uint8_t> &flash_bytes, uint16_t start_address) {
  std::ostringstream hex_out;
  size_t total = flash_bytes.size();
  size_t offset = 0;

  while (offset < total) {
    size_t chunk_len = std::min<size_t>(16, total - offset);
    uint16_t addr = static_cast<uint16_t>(start_address + offset);

    uint8_t sum = static_cast<uint8_t>(chunk_len) +
                  static_cast<uint8_t>((addr >> 8) & 0xFF) +
                  static_cast<uint8_t>(addr & 0xFF) +
                  0x00; // Type 00 (data)

    hex_out << ":"
            << std::uppercase << std::hex << std::setfill('0')
            << std::setw(2) << chunk_len
            << std::setw(4) << addr
            << "00";

    for (size_t i = 0; i < chunk_len; ++i) {
      uint8_t b = flash_bytes[offset + i];
      sum += b;
      hex_out << std::setw(2) << static_cast<int>(b);
    }

    uint8_t checksum = static_cast<uint8_t>((~sum + 1) & 0xFF);
    hex_out << std::setw(2) << static_cast<int>(checksum) << "\n";

    offset += chunk_len;
  }

  // End of file record
  hex_out << ":00000001FF\n";
  return hex_out.str();
}

bool AVRLinker::link(const std::vector<AVRAssembledModule> &input_modules,
                     const std::string &output_filepath,
                     std::string &error_msg,
                     bool include_crt0) {
  std::vector<AVRAssembledModule> modules;

  if (include_crt0) {
    const char *crt0_source =
        ".global __init\n"
        "__reset:\n"
        "    jmp __init\n"
        "__init:\n"
        "    cli\n"
        "    clr r1\n"
        "    ldi r16, 0xff\n"
        "    out 0x3d, r16\n"
        "    ldi r16, 0x08\n"
        "    out 0x3e, r16\n"
        "    call main\n"
        "__exit:\n"
        "    cli\n"
        "    rjmp __exit\n";

    AVRAssembler crt_asm;
    modules.push_back(crt_asm.assemble(crt0_source));
  }

  for (const auto &m : input_modules) {
    modules.push_back(m);
  }

  // ATmega328P Memory Map:
  // Flash: 0x0000 to 0x7FFF (32KB). Word addresses are byte_address / 2.
  // SRAM:  0x0100 to 0x08FF (2KB).
  const uint32_t sram_base = 0x0100;

  std::vector<uint8_t> linked_flash;
  std::vector<uint32_t> mod_text_bases(modules.size(), 0);
  std::vector<uint32_t> mod_data_bases(modules.size(), 0);
  std::vector<uint32_t> mod_bss_bases(modules.size(), 0);

  uint32_t current_text_offset = 0;
  uint32_t current_sram_offset = sram_base;

  // 1. Assign text bases (aligned to 2-byte word boundary)
  for (size_t i = 0; i < modules.size(); ++i) {
    if (current_text_offset % 2 != 0) current_text_offset++;
    mod_text_bases[i] = current_text_offset;
    current_text_offset += static_cast<uint32_t>(modules[i].text_bytes.size());
  }

  // 2. Assign SRAM data & bss bases
  for (size_t i = 0; i < modules.size(); ++i) {
    mod_data_bases[i] = current_sram_offset;
    current_sram_offset += static_cast<uint32_t>(modules[i].data_bytes.size());
    mod_bss_bases[i] = current_sram_offset;
    current_sram_offset += static_cast<uint32_t>(modules[i].bss_size);
  }

  // 3. Build global & local symbol lookup table
  std::unordered_map<std::string, uint32_t> global_symbols;
  std::vector<std::unordered_map<std::string, uint32_t>> local_symbols(modules.size());

  for (size_t i = 0; i < modules.size(); ++i) {
    const auto &m = modules[i];

    for (const auto &pair : m.symbol_offsets) {
      uint32_t abs_addr = mod_text_bases[i] + pair.second;
      bool is_local = (m.local_symbols.count(pair.first) > 0);
      if (is_local) {
        local_symbols[i][pair.first] = abs_addr;
      } else {
        if (global_symbols.count(pair.first)) {
          error_msg = "Linker error: multiple definition of symbol '" + pair.first + "'";
          return false;
        }
        global_symbols[pair.first] = abs_addr;
      }
    }

    for (const auto &pair : m.data_offsets) {
      uint32_t abs_addr = mod_data_bases[i] + pair.second;
      bool is_local = (m.local_symbols.count(pair.first) > 0);
      if (is_local) {
        local_symbols[i][pair.first] = abs_addr;
      } else {
        if (global_symbols.count(pair.first)) {
          error_msg = "Linker error: multiple definition of symbol '" + pair.first + "'";
          return false;
        }
        global_symbols[pair.first] = abs_addr;
      }
    }

    for (const auto &pair : m.bss_offsets) {
      uint32_t abs_addr = mod_bss_bases[i] + pair.second;
      bool is_local = (m.local_symbols.count(pair.first) > 0);
      if (is_local) {
        local_symbols[i][pair.first] = abs_addr;
      } else {
        if (global_symbols.count(pair.first)) {
          error_msg = "Linker error: multiple definition of symbol '" + pair.first + "'";
          return false;
        }
        global_symbols[pair.first] = abs_addr;
      }
    }
  }

  // Check main presence if linking standalone executable
  if (include_crt0 && global_symbols.find("main") == global_symbols.end()) {
    error_msg = "Linker error: undefined reference to 'main'";
    return false;
  }

  // 4. Concatenate and relocate text
  linked_flash.resize(current_text_offset, 0);

  for (size_t i = 0; i < modules.size(); ++i) {
    const auto &m = modules[i];
    uint32_t base = mod_text_bases[i];
    std::copy(m.text_bytes.begin(), m.text_bytes.end(), linked_flash.begin() + base);

    // Apply relocations
    for (const auto &rel : m.relocations) {
      size_t patch_offset = base + rel.offset;
      if (patch_offset + 1 >= linked_flash.size()) continue;

      uint32_t target_addr = 0;
      auto loc_it = local_symbols[i].find(rel.symbol);
      if (loc_it != local_symbols[i].end()) {
        target_addr = loc_it->second;
      } else {
        auto glob_it = global_symbols.find(rel.symbol);
        if (glob_it != global_symbols.end()) {
          target_addr = glob_it->second;
        } else {
          error_msg = "Linker error: undefined reference to symbol '" + rel.symbol + "'";
          return false;
        }
      }
      target_addr += rel.addend;

      uint16_t word1 = static_cast<uint16_t>(linked_flash[patch_offset] | (linked_flash[patch_offset + 1] << 8));

      switch (rel.type) {
      case AVRRelocType::RelativeJump: {
        int32_t pc_word = static_cast<int32_t>(patch_offset / 2);
        int32_t target_word = static_cast<int32_t>(target_addr / 2);
        int32_t rel_word = target_word - pc_word - 1;
        uint16_t patched = static_cast<uint16_t>((word1 & 0xF000) | (rel_word & 0x0FFF));
        linked_flash[patch_offset] = static_cast<uint8_t>(patched & 0xFF);
        linked_flash[patch_offset + 1] = static_cast<uint8_t>((patched >> 8) & 0xFF);
        break;
      }
      case AVRRelocType::RelativeBranch: {
        int32_t pc_word = static_cast<int32_t>(patch_offset / 2);
        int32_t target_word = static_cast<int32_t>(target_addr / 2);
        int32_t rel_word = target_word - pc_word - 1;
        uint16_t patched = static_cast<uint16_t>((word1 & 0xFC07) | ((rel_word & 0x7F) << 3));
        linked_flash[patch_offset] = static_cast<uint8_t>(patched & 0xFF);
        linked_flash[patch_offset + 1] = static_cast<uint8_t>((patched >> 8) & 0xFF);
        break;
      }
      case AVRRelocType::AbsoluteCall: {
        if (patch_offset + 3 < linked_flash.size()) {
          uint32_t target_word = target_addr / 2;
          uint16_t patched1 = static_cast<uint16_t>((word1 & 0xFE0E) | ((target_word & 0x3E0000) >> 13) | ((target_word & 0x010000) >> 16));
          uint16_t patched2 = static_cast<uint16_t>(target_word & 0xFFFF);
          linked_flash[patch_offset] = static_cast<uint8_t>(patched1 & 0xFF);
          linked_flash[patch_offset + 1] = static_cast<uint8_t>((patched1 >> 8) & 0xFF);
          linked_flash[patch_offset + 2] = static_cast<uint8_t>(patched2 & 0xFF);
          linked_flash[patch_offset + 3] = static_cast<uint8_t>((patched2 >> 8) & 0xFF);
        }
        break;
      }
      case AVRRelocType::LdsSts16: {
        if (patch_offset + 3 < linked_flash.size()) {
          uint16_t data_addr = static_cast<uint16_t>(target_addr & 0xFFFF);
          linked_flash[patch_offset + 2] = static_cast<uint8_t>(data_addr & 0xFF);
          linked_flash[patch_offset + 3] = static_cast<uint8_t>((data_addr >> 8) & 0xFF);
        }
        break;
      }
      case AVRRelocType::ImmLo8: {
        uint8_t k = static_cast<uint8_t>(target_addr & 0xFF);
        uint16_t patched = static_cast<uint16_t>((word1 & 0xF0F0) | ((k & 0xF0) << 4) | (k & 0x0F));
        linked_flash[patch_offset] = static_cast<uint8_t>(patched & 0xFF);
        linked_flash[patch_offset + 1] = static_cast<uint8_t>((patched >> 8) & 0xFF);
        break;
      }
      case AVRRelocType::ImmHi8: {
        uint8_t k = static_cast<uint8_t>((target_addr >> 8) & 0xFF);
        uint16_t patched = static_cast<uint16_t>((word1 & 0xF0F0) | ((k & 0xF0) << 4) | (k & 0x0F));
        linked_flash[patch_offset] = static_cast<uint8_t>(patched & 0xFF);
        linked_flash[patch_offset + 1] = static_cast<uint8_t>((patched >> 8) & 0xFF);
        break;
      }
      case AVRRelocType::ImmPmLo8: {
        uint8_t k = static_cast<uint8_t>((target_addr / 2) & 0xFF);
        uint16_t patched = static_cast<uint16_t>((word1 & 0xF0F0) | ((k & 0xF0) << 4) | (k & 0x0F));
        linked_flash[patch_offset] = static_cast<uint8_t>(patched & 0xFF);
        linked_flash[patch_offset + 1] = static_cast<uint8_t>((patched >> 8) & 0xFF);
        break;
      }
      case AVRRelocType::ImmPmHi8: {
        uint8_t k = static_cast<uint8_t>(((target_addr / 2) >> 8) & 0xFF);
        uint16_t patched = static_cast<uint16_t>((word1 & 0xF0F0) | ((k & 0xF0) << 4) | (k & 0x0F));
        linked_flash[patch_offset] = static_cast<uint8_t>(patched & 0xFF);
        linked_flash[patch_offset + 1] = static_cast<uint8_t>((patched >> 8) & 0xFF);
        break;
      }
      }
    }
  }

  // 5. Output Intel HEX format
  std::string hex_content = generate_intel_hex(linked_flash, 0x0000);
  std::ofstream out(output_filepath);
  if (!out.is_open()) {
    error_msg = "Could not open output file '" + output_filepath + "'";
    return false;
  }
  out << hex_content;
  return true;
}
