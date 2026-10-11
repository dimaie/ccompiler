#include "avr_assembler.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

void AVRAssembler::trim(std::string &s) const {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) {
    s.clear();
    return;
  }
  size_t end = s.find_last_not_of(" \t\r\n");
  s = s.substr(start, end - start + 1);
}

int AVRAssembler::parse_reg(const std::string &token) const {
  std::string s = token;
  trim(s);
  for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

  if (s == "x") return 26;
  if (s == "y") return 28;
  if (s == "z") return 30;

  if (s.size() >= 2 && s[0] == 'r') {
    try {
      int num = std::stoi(s.substr(1));
      if (num >= 0 && num <= 31) return num;
    } catch (...) {}
  }
  return -1;
}

int64_t AVRAssembler::parse_int(const std::string &token) const {
  std::string s = token;
  trim(s);
  if (s.empty()) return 0;

  int base = 10;
  if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
    base = 16;
    s = s.substr(2);
  } else if (s.size() > 1 && s[0] == '0') {
    base = 8;
    s = s.substr(1);
  }
  try {
    return std::stoll(s, nullptr, base);
  } catch (...) {
    return 0;
  }
}

std::vector<std::string> AVRAssembler::tokenize_operands(const std::string &op_str) const {
  std::vector<std::string> ops;
  std::string cur;
  int paren_depth = 0;

  for (char c : op_str) {
    if (c == '(') paren_depth++;
    else if (c == ')') paren_depth--;

    if (c == ',' && paren_depth == 0) {
      trim(cur);
      if (!cur.empty()) ops.push_back(cur);
      cur.clear();
    } else {
      cur += c;
    }
  }
  trim(cur);
  if (!cur.empty()) ops.push_back(cur);
  return ops;
}

void AVRAssembler::emit_word(std::vector<uint8_t> &dest, uint16_t word) {
  dest.push_back(static_cast<uint8_t>(word & 0xFF));
  dest.push_back(static_cast<uint8_t>((word >> 8) & 0xFF));
}

void AVRAssembler::emit_dword(std::vector<uint8_t> &dest, uint16_t word1, uint16_t word2) {
  emit_word(dest, word1);
  emit_word(dest, word2);
}

AVRAssembledModule AVRAssembler::assemble(const std::string &assembly_source) {
  AVRAssembledModule module;
  std::istringstream stream(assembly_source);
  std::string line;

  enum Section { Text, Data, Bss } current_section = Text;

  while (std::getline(stream, line)) {
    // Strip comments (; or //)
    size_t comment_pos = line.find(';');
    if (comment_pos != std::string::npos) line = line.substr(0, comment_pos);
    size_t slash_pos = line.find("//");
    if (slash_pos != std::string::npos) line = line.substr(0, slash_pos);

    trim(line);
    if (line.empty()) continue;

    // Check for labels (e.g. "main:", ".L1:")
    size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos && line.find(' ') > colon_pos) {
      std::string label = line.substr(0, colon_pos);
      trim(label);
      if (current_section == Text) {
        module.symbol_offsets[label] = static_cast<uint32_t>(module.text_bytes.size());
      } else if (current_section == Data) {
        module.data_offsets[label] = static_cast<uint32_t>(module.data_bytes.size());
      } else if (current_section == Bss) {
        module.bss_offsets[label] = static_cast<uint32_t>(module.bss_size);
      }
      line = line.substr(colon_pos + 1);
      trim(line);
      if (line.empty()) continue;
    }

    // Directives
    if (line[0] == '.') {
      std::istringstream liness(line);
      std::string dir;
      liness >> dir;
      for (char &c : dir) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

      if (dir == ".global" || dir == ".globl") {
        std::string sym;
        liness >> sym;
        trim(sym);
        module.global_symbols.insert(sym);
      } else if (dir == ".local") {
        std::string sym;
        liness >> sym;
        trim(sym);
        module.local_symbols.insert(sym);
      } else if (dir == ".section") {
        std::string sec;
        liness >> sec;
        for (char &c : sec) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (sec.find(".text") != std::string::npos) current_section = Text;
        else if (sec.find(".data") != std::string::npos || sec.find(".rodata") != std::string::npos) current_section = Data;
        else if (sec.find(".bss") != std::string::npos) current_section = Bss;
      } else if (dir == ".text") {
        current_section = Text;
      } else if (dir == ".data") {
        current_section = Data;
      } else if (dir == ".bss") {
        current_section = Bss;
      } else if (dir == ".zero" || dir == ".space") {
        std::string count_str;
        liness >> count_str;
        int count = static_cast<int>(parse_int(count_str));
        if (current_section == Bss) {
          module.bss_size += count;
        } else if (current_section == Data) {
          for (int i = 0; i < count; ++i) module.data_bytes.push_back(0);
        }
      } else if (dir == ".byte") {
        std::string rest;
        std::getline(liness, rest);
        auto bytes = tokenize_operands(rest);
        for (const auto &b : bytes) {
          uint8_t val = static_cast<uint8_t>(parse_int(b));
          if (current_section == Text) module.text_bytes.push_back(val);
          else module.data_bytes.push_back(val);
        }
      } else if (dir == ".word") {
        std::string rest;
        std::getline(liness, rest);
        auto words = tokenize_operands(rest);
        for (const auto &w : words) {
          uint16_t val = static_cast<uint16_t>(parse_int(w));
          if (current_section == Text) emit_word(module.text_bytes, val);
          else {
            module.data_bytes.push_back(static_cast<uint8_t>(val & 0xFF));
            module.data_bytes.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
          }
        }
      }
      continue;
    }

    // Instructions
    std::string mnemonic;
    size_t space_pos = line.find_first_of(" \t");
    std::string operand_str;
    if (space_pos != std::string::npos) {
      mnemonic = line.substr(0, space_pos);
      operand_str = line.substr(space_pos + 1);
    } else {
      mnemonic = line;
    }
    trim(mnemonic);
    trim(operand_str);
    for (char &c : mnemonic) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    auto ops = tokenize_operands(operand_str);

    // Encode instructions
    if (mnemonic == "nop") {
      emit_word(module.text_bytes, 0x0000);
    } else if (mnemonic == "ret") {
      emit_word(module.text_bytes, 0x9508);
    } else if (mnemonic == "reti") {
      emit_word(module.text_bytes, 0x9518);
    } else if (mnemonic == "cli") {
      emit_word(module.text_bytes, 0x94F8);
    } else if (mnemonic == "sei") {
      emit_word(module.text_bytes, 0x9478);
    } else if (mnemonic == "push" && ops.size() == 1) {
      int r = parse_reg(ops[0]);
      if (r >= 0) emit_word(module.text_bytes, static_cast<uint16_t>(0x920F | ((r & 0x1F) << 4)));
    } else if (mnemonic == "pop" && ops.size() == 1) {
      int d = parse_reg(ops[0]);
      if (d >= 0) emit_word(module.text_bytes, static_cast<uint16_t>(0x900F | ((d & 0x1F) << 4)));
    } else if (mnemonic == "clr" && ops.size() == 1) {
      int d = parse_reg(ops[0]);
      if (d >= 0) {
        uint16_t word = static_cast<uint16_t>(0x2400 | ((d & 0x10) << 5) | ((d & 0x1F) << 4) | (d & 0x0F));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "com" && ops.size() == 1) {
      int d = parse_reg(ops[0]);
      if (d >= 0) emit_word(module.text_bytes, static_cast<uint16_t>(0x9400 | ((d & 0x1F) << 4)));
    } else if (mnemonic == "neg" && ops.size() == 1) {
      int d = parse_reg(ops[0]);
      if (d >= 0) emit_word(module.text_bytes, static_cast<uint16_t>(0x9401 | ((d & 0x1F) << 4)));
    } else if (mnemonic == "inc" && ops.size() == 1) {
      int d = parse_reg(ops[0]);
      if (d >= 0) emit_word(module.text_bytes, static_cast<uint16_t>(0x9403 | ((d & 0x1F) << 4)));
    } else if (mnemonic == "dec" && ops.size() == 1) {
      int d = parse_reg(ops[0]);
      if (d >= 0) emit_word(module.text_bytes, static_cast<uint16_t>(0x940A | ((d & 0x1F) << 4)));
    } else if (mnemonic == "mov" && ops.size() == 2) {
      int d = parse_reg(ops[0]);
      int r = parse_reg(ops[1]);
      if (d >= 0 && r >= 0) {
        uint16_t word = static_cast<uint16_t>(0x2C00 | ((r & 0x10) << 5) | ((d & 0x1F) << 4) | (r & 0x0F));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "movw" && ops.size() == 2) {
      int d = parse_reg(ops[0]);
      int r = parse_reg(ops[1]);
      if (d >= 0 && r >= 0 && (d % 2 == 0) && (r % 2 == 0)) {
        uint16_t word = static_cast<uint16_t>(0x0100 | (((d / 2) & 0x0F) << 4) | ((r / 2) & 0x0F));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "add" && ops.size() == 2) {
      int d = parse_reg(ops[0]);
      int r = parse_reg(ops[1]);
      if (d >= 0 && r >= 0) {
        uint16_t word = static_cast<uint16_t>(0x0C00 | ((r & 0x10) << 5) | ((d & 0x1F) << 4) | (r & 0x0F));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "adc" && ops.size() == 2) {
      int d = parse_reg(ops[0]);
      int r = parse_reg(ops[1]);
      if (d >= 0 && r >= 0) {
        uint16_t word = static_cast<uint16_t>(0x1C00 | ((r & 0x10) << 5) | ((d & 0x1F) << 4) | (r & 0x0F));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "sub" && ops.size() == 2) {
      int d = parse_reg(ops[0]);
      int r = parse_reg(ops[1]);
      if (d >= 0 && r >= 0) {
        uint16_t word = static_cast<uint16_t>(0x1800 | ((r & 0x10) << 5) | ((d & 0x1F) << 4) | (r & 0x0F));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "sbc" && ops.size() == 2) {
      int d = parse_reg(ops[0]);
      int r = parse_reg(ops[1]);
      if (d >= 0 && r >= 0) {
        uint16_t word = static_cast<uint16_t>(0x0800 | ((r & 0x10) << 5) | ((d & 0x1F) << 4) | (r & 0x0F));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "and" && ops.size() == 2) {
      int d = parse_reg(ops[0]);
      int r = parse_reg(ops[1]);
      if (d >= 0 && r >= 0) {
        uint16_t word = static_cast<uint16_t>(0x2000 | ((r & 0x10) << 5) | ((d & 0x1F) << 4) | (r & 0x0F));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "or" && ops.size() == 2) {
      int d = parse_reg(ops[0]);
      int r = parse_reg(ops[1]);
      if (d >= 0 && r >= 0) {
        uint16_t word = static_cast<uint16_t>(0x2800 | ((r & 0x10) << 5) | ((d & 0x1F) << 4) | (r & 0x0F));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "eor" && ops.size() == 2) {
      int d = parse_reg(ops[0]);
      int r = parse_reg(ops[1]);
      if (d >= 0 && r >= 0) {
        uint16_t word = static_cast<uint16_t>(0x2400 | ((r & 0x10) << 5) | ((d & 0x1F) << 4) | (r & 0x0F));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "cp" && ops.size() == 2) {
      int d = parse_reg(ops[0]);
      int r = parse_reg(ops[1]);
      if (d >= 0 && r >= 0) {
        uint16_t word = static_cast<uint16_t>(0x1400 | ((r & 0x10) << 5) | ((d & 0x1F) << 4) | (r & 0x0F));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "cpc" && ops.size() == 2) {
      int d = parse_reg(ops[0]);
      int r = parse_reg(ops[1]);
      if (d >= 0 && r >= 0) {
        uint16_t word = static_cast<uint16_t>(0x0400 | ((r & 0x10) << 5) | ((d & 0x1F) << 4) | (r & 0x0F));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "subi" && ops.size() == 2) {
      int d = parse_reg(ops[0]);
      int k = static_cast<int>(parse_int(ops[1]));
      if (d >= 16 && d <= 31) {
        uint16_t word = static_cast<uint16_t>(0x5000 | ((k & 0xF0) << 4) | (((d - 16) & 0x0F) << 4) | (k & 0x0F));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "sbci" && ops.size() == 2) {
      int d = parse_reg(ops[0]);
      int k = static_cast<int>(parse_int(ops[1]));
      if (d >= 16 && d <= 31) {
        uint16_t word = static_cast<uint16_t>(0x4000 | ((k & 0xF0) << 4) | (((d - 16) & 0x0F) << 4) | (k & 0x0F));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "andi" && ops.size() == 2) {
      int d = parse_reg(ops[0]);
      int k = static_cast<int>(parse_int(ops[1]));
      if (d >= 16 && d <= 31) {
        uint16_t word = static_cast<uint16_t>(0x7000 | ((k & 0xF0) << 4) | (((d - 16) & 0x0F) << 4) | (k & 0x0F));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "ori" && ops.size() == 2) {
      int d = parse_reg(ops[0]);
      int k = static_cast<int>(parse_int(ops[1]));
      if (d >= 16 && d <= 31) {
        uint16_t word = static_cast<uint16_t>(0x6000 | ((k & 0xF0) << 4) | (((d - 16) & 0x0F) << 4) | (k & 0x0F));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "cpi" && ops.size() == 2) {
      int d = parse_reg(ops[0]);
      int k = static_cast<int>(parse_int(ops[1]));
      if (d >= 16 && d <= 31) {
        uint16_t word = static_cast<uint16_t>(0x3000 | ((k & 0xF0) << 4) | (((d - 16) & 0x0F) << 4) | (k & 0x0F));
        emit_word(module.text_bytes, word);
      }
    } else if ((mnemonic == "adiw" || mnemonic == "sbiw") && ops.size() == 2) {
      int d = parse_reg(ops[0]);
      int k = static_cast<int>(parse_int(ops[1]));
      int d_idx = -1;
      if (d == 24) d_idx = 0;
      else if (d == 26) d_idx = 1;
      else if (d == 28) d_idx = 2;
      else if (d == 30) d_idx = 3;
      if (d_idx >= 0 && k >= 0 && k <= 63) {
        uint16_t base = (mnemonic == "adiw") ? 0x9600 : 0x9700;
        uint16_t word = static_cast<uint16_t>(base | ((k & 0x30) << 2) | ((d_idx & 0x03) << 4) | (k & 0x0F));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "in" && ops.size() == 2) {
      int d = parse_reg(ops[0]);
      int a = static_cast<int>(parse_int(ops[1]));
      if (d >= 0 && a >= 0 && a <= 63) {
        uint16_t word = static_cast<uint16_t>(0xB000 | ((a & 0x30) << 5) | ((d & 0x1F) << 4) | (a & 0x0F));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "out" && ops.size() == 2) {
      int a = static_cast<int>(parse_int(ops[0]));
      int r = parse_reg(ops[1]);
      if (r >= 0 && a >= 0 && a <= 63) {
        uint16_t word = static_cast<uint16_t>(0xB800 | ((a & 0x30) << 5) | ((r & 0x1F) << 4) | (a & 0x0F));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "sbi" && ops.size() == 2) {
      int a = static_cast<int>(parse_int(ops[0]));
      int b = static_cast<int>(parse_int(ops[1]));
      if (a >= 0 && a <= 31 && b >= 0 && b <= 7) {
        uint16_t word = static_cast<uint16_t>(0x9A00 | ((a & 0x1F) << 3) | (b & 0x07));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "cbi" && ops.size() == 2) {
      int a = static_cast<int>(parse_int(ops[0]));
      int b = static_cast<int>(parse_int(ops[1]));
      if (a >= 0 && a <= 31 && b >= 0 && b <= 7) {
        uint16_t word = static_cast<uint16_t>(0x9800 | ((a & 0x1F) << 3) | (b & 0x07));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "sbic" && ops.size() == 2) {
      int a = static_cast<int>(parse_int(ops[0]));
      int b = static_cast<int>(parse_int(ops[1]));
      if (a >= 0 && a <= 31 && b >= 0 && b <= 7) {
        uint16_t word = static_cast<uint16_t>(0x9900 | ((a & 0x1F) << 3) | (b & 0x07));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "sbis" && ops.size() == 2) {
      int a = static_cast<int>(parse_int(ops[0]));
      int b = static_cast<int>(parse_int(ops[1]));
      if (a >= 0 && a <= 31 && b >= 0 && b <= 7) {
        uint16_t word = static_cast<uint16_t>(0x9B00 | ((a & 0x1F) << 3) | (b & 0x07));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "ldi" && ops.size() == 2) {
      int d = parse_reg(ops[0]);
      std::string val_expr = ops[1];
      trim(val_expr);

      if (d >= 16 && d <= 31) {
        if (val_expr.rfind("lo8(", 0) == 0 && val_expr.back() == ')') {
          std::string sym = val_expr.substr(4, val_expr.size() - 5);
          trim(sym);
          size_t cur_off = module.text_bytes.size();
          emit_word(module.text_bytes, static_cast<uint16_t>(0xE000 | (((d - 16) & 0x0F) << 4)));
          module.relocations.push_back({cur_off, sym, 0, AVRRelocType::ImmLo8});
        } else if (val_expr.rfind("hi8(", 0) == 0 && val_expr.back() == ')') {
          std::string sym = val_expr.substr(4, val_expr.size() - 5);
          trim(sym);
          size_t cur_off = module.text_bytes.size();
          emit_word(module.text_bytes, static_cast<uint16_t>(0xE000 | (((d - 16) & 0x0F) << 4)));
          module.relocations.push_back({cur_off, sym, 0, AVRRelocType::ImmHi8});
        } else if (val_expr.rfind("pm_lo8(", 0) == 0 && val_expr.back() == ')') {
          std::string sym = val_expr.substr(7, val_expr.size() - 8);
          trim(sym);
          size_t cur_off = module.text_bytes.size();
          emit_word(module.text_bytes, static_cast<uint16_t>(0xE000 | (((d - 16) & 0x0F) << 4)));
          module.relocations.push_back({cur_off, sym, 0, AVRRelocType::ImmPmLo8});
        } else if (val_expr.rfind("pm_hi8(", 0) == 0 && val_expr.back() == ')') {
          std::string sym = val_expr.substr(7, val_expr.size() - 8);
          trim(sym);
          size_t cur_off = module.text_bytes.size();
          emit_word(module.text_bytes, static_cast<uint16_t>(0xE000 | (((d - 16) & 0x0F) << 4)));
          module.relocations.push_back({cur_off, sym, 0, AVRRelocType::ImmPmHi8});
        } else {
          int k = static_cast<int>(parse_int(val_expr));
          uint16_t word = static_cast<uint16_t>(0xE000 | ((k & 0xF0) << 4) | (((d - 16) & 0x0F) << 4) | (k & 0x0F));
          emit_word(module.text_bytes, word);
        }
      }
    } else if (mnemonic == "lds" && ops.size() == 2) {
      int d = parse_reg(ops[0]);
      std::string sym = ops[1];
      trim(sym);
      int addend = 0;
      size_t plus_pos = sym.find('+');
      if (plus_pos != std::string::npos) {
        addend = static_cast<int>(parse_int(sym.substr(plus_pos + 1)));
        sym = sym.substr(0, plus_pos);
        trim(sym);
      }
      size_t cur_off = module.text_bytes.size();
      emit_dword(module.text_bytes, static_cast<uint16_t>(0x9000 | ((d & 0x1F) << 4)), 0x0000);
      module.relocations.push_back({cur_off, sym, addend, AVRRelocType::LdsSts16});
    } else if (mnemonic == "sts" && ops.size() == 2) {
      std::string sym = ops[0];
      int r = parse_reg(ops[1]);
      trim(sym);
      int addend = 0;
      size_t plus_pos = sym.find('+');
      if (plus_pos != std::string::npos) {
        addend = static_cast<int>(parse_int(sym.substr(plus_pos + 1)));
        sym = sym.substr(0, plus_pos);
        trim(sym);
      }
      size_t cur_off = module.text_bytes.size();
      emit_dword(module.text_bytes, static_cast<uint16_t>(0x9200 | ((r & 0x1F) << 4)), 0x0000);
      module.relocations.push_back({cur_off, sym, addend, AVRRelocType::LdsSts16});
    } else if ((mnemonic == "ld" || mnemonic == "ldd") && ops.size() == 2) {
      int d = parse_reg(ops[0]);
      std::string ptr = ops[1];
      trim(ptr);
      for (char &c : ptr) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

      if (ptr == "x") emit_word(module.text_bytes, static_cast<uint16_t>(0x900C | ((d & 0x1F) << 4)));
      else if (ptr == "x+") emit_word(module.text_bytes, static_cast<uint16_t>(0x900D | ((d & 0x1F) << 4)));
      else if (ptr == "-x") emit_word(module.text_bytes, static_cast<uint16_t>(0x900E | ((d & 0x1F) << 4)));
      else if (ptr == "y") emit_word(module.text_bytes, static_cast<uint16_t>(0x8008 | ((d & 0x1F) << 4)));
      else if (ptr == "z") emit_word(module.text_bytes, static_cast<uint16_t>(0x8000 | ((d & 0x1F) << 4)));
      else if (ptr.rfind("y+", 0) == 0) {
        int q = static_cast<int>(parse_int(ptr.substr(2)));
        uint16_t word = static_cast<uint16_t>(0x8008 | ((q & 0x20) << 8) | ((q & 0x18) << 7) | ((d & 0x1F) << 4) | (q & 0x07));
        emit_word(module.text_bytes, word);
      } else if (ptr.rfind("z+", 0) == 0) {
        int q = static_cast<int>(parse_int(ptr.substr(2)));
        uint16_t word = static_cast<uint16_t>(0x8000 | ((q & 0x20) << 8) | ((q & 0x18) << 7) | ((d & 0x1F) << 4) | (q & 0x07));
        emit_word(module.text_bytes, word);
      }
    } else if ((mnemonic == "st" || mnemonic == "std") && ops.size() == 2) {
      std::string ptr = ops[0];
      int r = parse_reg(ops[1]);
      trim(ptr);
      for (char &c : ptr) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

      if (ptr == "x") emit_word(module.text_bytes, static_cast<uint16_t>(0x920C | ((r & 0x1F) << 4)));
      else if (ptr == "x+") emit_word(module.text_bytes, static_cast<uint16_t>(0x920D | ((r & 0x1F) << 4)));
      else if (ptr == "-x") emit_word(module.text_bytes, static_cast<uint16_t>(0x920E | ((r & 0x1F) << 4)));
      else if (ptr == "y") emit_word(module.text_bytes, static_cast<uint16_t>(0x8208 | ((r & 0x1F) << 4)));
      else if (ptr == "z") emit_word(module.text_bytes, static_cast<uint16_t>(0x8200 | ((r & 0x1F) << 4)));
      else if (ptr.rfind("y+", 0) == 0) {
        int q = static_cast<int>(parse_int(ptr.substr(2)));
        uint16_t word = static_cast<uint16_t>(0x8208 | ((q & 0x20) << 8) | ((q & 0x18) << 7) | ((r & 0x1F) << 4) | (q & 0x07));
        emit_word(module.text_bytes, word);
      } else if (ptr.rfind("z+", 0) == 0) {
        int q = static_cast<int>(parse_int(ptr.substr(2)));
        uint16_t word = static_cast<uint16_t>(0x8200 | ((q & 0x20) << 8) | ((q & 0x18) << 7) | ((r & 0x1F) << 4) | (q & 0x07));
        emit_word(module.text_bytes, word);
      }
    } else if (mnemonic == "rjmp" && ops.size() == 1) {
      size_t cur_off = module.text_bytes.size();
      emit_word(module.text_bytes, 0xC000);
      module.relocations.push_back({cur_off, ops[0], 0, AVRRelocType::RelativeJump});
    } else if (mnemonic == "rcall" && ops.size() == 1) {
      size_t cur_off = module.text_bytes.size();
      emit_word(module.text_bytes, 0xD000);
      module.relocations.push_back({cur_off, ops[0], 0, AVRRelocType::RelativeJump});
    } else if (mnemonic == "jmp" && ops.size() == 1) {
      size_t cur_off = module.text_bytes.size();
      emit_dword(module.text_bytes, 0x940C, 0x0000);
      module.relocations.push_back({cur_off, ops[0], 0, AVRRelocType::AbsoluteCall});
    } else if (mnemonic == "call" && ops.size() == 1) {
      size_t cur_off = module.text_bytes.size();
      emit_dword(module.text_bytes, 0x940E, 0x0000);
      module.relocations.push_back({cur_off, ops[0], 0, AVRRelocType::AbsoluteCall});
    } else if (ops.size() == 1 &&
               (mnemonic == "breq" || mnemonic == "brne" || mnemonic == "brcs" ||
                mnemonic == "brlo" || mnemonic == "brcc" || mnemonic == "brsh" ||
                mnemonic == "brlt" || mnemonic == "brge")) {
      uint16_t base = 0xF000;
      if (mnemonic == "breq") base = 0xF001;
      else if (mnemonic == "brne") base = 0xF401;
      else if (mnemonic == "brcs" || mnemonic == "brlo") base = 0xF000;
      else if (mnemonic == "brcc" || mnemonic == "brsh") base = 0xF400;
      else if (mnemonic == "brlt") base = 0xF004;
      else if (mnemonic == "brge") base = 0xF404;

      size_t cur_off = module.text_bytes.size();
      emit_word(module.text_bytes, base);
      module.relocations.push_back({cur_off, ops[0], 0, AVRRelocType::RelativeBranch});
    }
  }

  return module;
}
