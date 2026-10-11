#ifndef DWARF_EMITTER_HPP
#define DWARF_EMITTER_HPP

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

struct SourceLoc {
  size_t text_offset{0};
  uint32_t file_id{1};
  uint32_t line{0};
  uint32_t col{0};
};

struct DwarfFunction {
  std::string name;
  uint32_t offset{0};
  uint32_t size{0};
};

struct DwarfArtifacts {
  std::vector<uint8_t> debug_line;
  std::vector<uint8_t> debug_info;
  std::vector<uint8_t> debug_abbrev;
};

class DwarfEmitter {
public:
  static void emit_uleb128(std::vector<uint8_t> &dest, uint64_t val) {
    do {
      uint8_t byte = val & 0x7F;
      val >>= 7;
      if (val != 0) byte |= 0x80;
      dest.push_back(byte);
    } while (val != 0);
  }

  static void emit_sleb128(std::vector<uint8_t> &dest, int64_t val) {
    bool more = true;
    while (more) {
      uint8_t byte = val & 0x7F;
      val >>= 7;
      bool sign_bit = (byte & 0x40) != 0;
      if ((val == 0 && !sign_bit) || (val == -1 && sign_bit)) {
        more = false;
      } else {
        byte |= 0x80;
      }
      dest.push_back(byte);
    }
  }

  static void emit_u16(std::vector<uint8_t> &dest, uint16_t val) {
    dest.push_back(static_cast<uint8_t>(val & 0xFF));
    dest.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
  }

  static void emit_u32(std::vector<uint8_t> &dest, uint32_t val) {
    dest.push_back(static_cast<uint8_t>(val & 0xFF));
    dest.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
    dest.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
    dest.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
  }

  static void emit_u64(std::vector<uint8_t> &dest, uint64_t val) {
    for (int i = 0; i < 8; ++i) {
      dest.push_back(static_cast<uint8_t>((val >> (i * 8)) & 0xFF));
    }
  }

  static DwarfArtifacts emit(
      const std::string &primary_filename,
      const std::unordered_map<uint32_t, std::string> &file_table,
      const std::vector<SourceLoc> &locs,
      uint64_t text_vma,
      uint32_t text_size,
      const std::vector<DwarfFunction> &functions = {}) {
    DwarfArtifacts artifacts;

    // 1. Build .debug_abbrev
    auto &abbrev = artifacts.debug_abbrev;
    abbrev.push_back(1);    // Abbrev code 1
    abbrev.push_back(0x11); // DW_TAG_compile_unit
    abbrev.push_back(functions.empty() ? 0x00 : 0x01); // DW_CHILDREN
    // Attribute 1: DW_AT_producer, DW_FORM_string
    abbrev.push_back(0x25); abbrev.push_back(0x08);
    // Attribute 2: DW_AT_language, DW_FORM_data2
    abbrev.push_back(0x13); abbrev.push_back(0x05);
    // Attribute 3: DW_AT_name, DW_FORM_string
    abbrev.push_back(0x03); abbrev.push_back(0x08);
    // Attribute 4: DW_AT_stmt_list, DW_FORM_data4
    abbrev.push_back(0x10); abbrev.push_back(0x06);
    // Attribute 5: DW_AT_low_pc, DW_FORM_addr
    abbrev.push_back(0x11); abbrev.push_back(0x01);
    // Attribute 6: DW_AT_high_pc, DW_FORM_addr
    abbrev.push_back(0x12); abbrev.push_back(0x01);
    // End of attributes
    abbrev.push_back(0x00); abbrev.push_back(0x00);

    if (!functions.empty()) {
      abbrev.push_back(2);    // Abbrev code 2
      abbrev.push_back(0x2e); // DW_TAG_subprogram
      abbrev.push_back(0x00); // DW_CHILDREN_no
      // Attribute 1: DW_AT_name, DW_FORM_string
      abbrev.push_back(0x03); abbrev.push_back(0x08);
      // Attribute 2: DW_AT_low_pc, DW_FORM_addr
      abbrev.push_back(0x11); abbrev.push_back(0x01);
      // Attribute 3: DW_AT_high_pc, DW_FORM_addr
      abbrev.push_back(0x12); abbrev.push_back(0x01);
      // End of attributes
      abbrev.push_back(0x00); abbrev.push_back(0x00);
    }
    // End of abbreviations
    abbrev.push_back(0x00);

    // 2. Build .debug_info
    auto &info = artifacts.debug_info;
    emit_u32(info, 0); // Placeholder for unit_length
    emit_u16(info, 4); // DWARF version 4
    emit_u32(info, 0); // debug_abbrev_offset = 0
    info.push_back(8); // address_size = 8 (64-bit)

    emit_uleb128(info, 1); // Abbrev code 1
    // DW_AT_producer
    const char producer[] = "Antigravity CCompiler";
    for (char c : producer) info.push_back(static_cast<uint8_t>(c));
    // DW_AT_language (DW_LANG_C99 = 0x000c)
    emit_u16(info, 0x000c);
    // DW_AT_name
    for (char c : primary_filename) info.push_back(static_cast<uint8_t>(c));
    info.push_back(0);
    // DW_AT_stmt_list = offset 0 into .debug_line
    emit_u32(info, 0);
    // DW_AT_low_pc
    emit_u64(info, text_vma);
    // DW_AT_high_pc
    emit_u64(info, text_vma + text_size);

    for (const auto &fn : functions) {
      emit_uleb128(info, 2); // Abbrev code 2 (DW_TAG_subprogram)
      for (char c : fn.name) info.push_back(static_cast<uint8_t>(c));
      info.push_back(0);
      emit_u64(info, text_vma + fn.offset);
      emit_u64(info, text_vma + fn.offset + fn.size);
    }
    if (!functions.empty()) {
      info.push_back(0); // End of children
    }

    // Fix up info unit_length
    uint32_t info_len = static_cast<uint32_t>(info.size() - 4);
    info[0] = static_cast<uint8_t>(info_len & 0xFF);
    info[1] = static_cast<uint8_t>((info_len >> 8) & 0xFF);
    info[2] = static_cast<uint8_t>((info_len >> 16) & 0xFF);
    info[3] = static_cast<uint8_t>((info_len >> 24) & 0xFF);

    // 3. Build .debug_line
    auto &line = artifacts.debug_line;
    emit_u32(line, 0); // Placeholder for unit_length
    emit_u16(line, 4); // Version 4

    size_t hdr_len_pos = line.size();
    emit_u32(line, 0); // Placeholder for header_length
    size_t hdr_start = line.size();

    line.push_back(1); // minimum_instruction_length = 1
    line.push_back(1); // maximum_operations_per_instruction = 1
    line.push_back(1); // default_is_stmt = 1
    line.push_back(static_cast<uint8_t>(-5)); // line_base = -5 (0xFB)
    line.push_back(14); // line_range = 14
    line.push_back(13); // opcode_base = 13 (opcodes 1..12)

    // Standard opcode lengths:
    line.push_back(0); // 1: DW_LNS_copy
    line.push_back(1); // 2: DW_LNS_advance_pc
    line.push_back(1); // 3: DW_LNS_advance_line
    line.push_back(1); // 4: DW_LNS_set_file
    line.push_back(1); // 5: DW_LNS_set_column
    line.push_back(0); // 6: DW_LNS_negate_stmt
    line.push_back(0); // 7: DW_LNS_set_basic_block
    line.push_back(0); // 8: DW_LNS_const_add_pc
    line.push_back(1); // 9: DW_LNS_fixed_advance_pc
    line.push_back(0); // 10: DW_LNS_set_prologue_end
    line.push_back(0); // 11: DW_LNS_set_epilogue_begin
    line.push_back(1); // 12: DW_LNS_set_isa

    // include_directories: empty (1 null byte)
    line.push_back(0);

    // file_names:
    // Ensure primary_filename is file 1 if file_table is empty
    if (file_table.empty()) {
      for (char c : primary_filename) line.push_back(static_cast<uint8_t>(c));
      line.push_back(0);
      emit_uleb128(line, 0); // dir_idx
      emit_uleb128(line, 0); // mod_time
      emit_uleb128(line, 0); // length
    } else {
      uint32_t max_fid = 1;
      for (const auto &pair : file_table) {
        if (pair.first > max_fid) max_fid = pair.first;
      }
      for (uint32_t fid = 1; fid <= max_fid; ++fid) {
        auto it = file_table.find(fid);
        std::string fname = (it != file_table.end()) ? it->second : primary_filename;
        for (char c : fname) line.push_back(static_cast<uint8_t>(c));
        line.push_back(0);
        emit_uleb128(line, 0); // dir_idx
        emit_uleb128(line, 0); // mod_time
        emit_uleb128(line, 0); // length
      }
    }
    line.push_back(0); // End of file list

    // Fix up header_length
    uint32_t hdr_len = static_cast<uint32_t>(line.size() - hdr_start);
    line[hdr_len_pos] = static_cast<uint8_t>(hdr_len & 0xFF);
    line[hdr_len_pos + 1] = static_cast<uint8_t>((hdr_len >> 8) & 0xFF);
    line[hdr_len_pos + 2] = static_cast<uint8_t>((hdr_len >> 16) & 0xFF);
    line[hdr_len_pos + 3] = static_cast<uint8_t>((hdr_len >> 24) & 0xFF);

    // Opcode state machine stream:
    uint64_t cur_address = text_vma;
    uint32_t cur_line = 1;
    uint32_t cur_file = 1;

    // Set initial address via DW_LNE_set_address
    uint64_t init_addr = locs.empty() ? text_vma : (text_vma + locs[0].text_offset);
    line.push_back(0x00);
    emit_uleb128(line, 9); // length = 9 (1 byte opcode + 8 bytes address)
    line.push_back(0x02); // DW_LNE_set_address
    emit_u64(line, init_addr);
    cur_address = init_addr;

    for (const auto &loc : locs) {
      if (loc.file_id != cur_file) {
        line.push_back(0x04); // DW_LNS_set_file
        emit_uleb128(line, loc.file_id);
        cur_file = loc.file_id;
      }

      uint64_t target_addr = text_vma + loc.text_offset;
      int64_t addr_advance = static_cast<int64_t>(target_addr - cur_address);
      int64_t line_advance = static_cast<int64_t>(loc.line) - cur_line;

      // Special opcode check:
      int8_t line_base = -5;
      uint8_t line_range = 14;
      uint8_t opcode_base = 13;
      int64_t special_op = (line_advance - line_base) + (line_range * addr_advance) + opcode_base;

      if (line_advance >= line_base && line_advance < line_base + line_range &&
          special_op >= opcode_base && special_op <= 255) {
        line.push_back(static_cast<uint8_t>(special_op));
      } else {
        if (addr_advance > 0) {
          line.push_back(0x02); // DW_LNS_advance_pc
          emit_uleb128(line, static_cast<uint64_t>(addr_advance));
        }
        if (line_advance != 0) {
          line.push_back(0x03); // DW_LNS_advance_line
          emit_sleb128(line, line_advance);
        }
        line.push_back(0x01); // DW_LNS_copy
      }

      cur_address = target_addr;
      cur_line = loc.line;
    }

    // Advance to end of text and close sequence:
    uint64_t end_addr = text_vma + text_size;
    if (end_addr > cur_address) {
      line.push_back(0x02); // DW_LNS_advance_pc
      emit_uleb128(line, end_addr - cur_address);
    }
    // DW_LNE_end_sequence
    line.push_back(0x00);
    emit_uleb128(line, 1);
    line.push_back(0x01); // DW_LNE_end_sequence

    // Fix up line unit_length
    uint32_t line_len = static_cast<uint32_t>(line.size() - 4);
    line[0] = static_cast<uint8_t>(line_len & 0xFF);
    line[1] = static_cast<uint8_t>((line_len >> 8) & 0xFF);
    line[2] = static_cast<uint8_t>((line_len >> 16) & 0xFF);
    line[3] = static_cast<uint8_t>((line_len >> 24) & 0xFF);

    return artifacts;
  }
};

#endif // DWARF_EMITTER_HPP
