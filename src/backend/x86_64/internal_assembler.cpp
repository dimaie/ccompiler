#include "internal_assembler.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>

namespace {

std::string trim(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

std::vector<std::string> split_operands(const std::string &op_str) {
  std::vector<std::string> ops;
  std::string cur;
  int paren_depth = 0;
  for (char c : op_str) {
    if (c == '(') paren_depth++;
    else if (c == ')') paren_depth--;
    if (c == ',' && paren_depth == 0) {
      ops.push_back(trim(cur));
      cur.clear();
    } else {
      cur += c;
    }
  }
  if (!cur.empty()) {
    ops.push_back(trim(cur));
  }
  return ops;
}

int64_t parse_immediate(const std::string &imm_str) {
  std::string s = imm_str;
  if (!s.empty() && s[0] == '$') s = s.substr(1);
  return std::stoll(s, nullptr, 0);
}

} // namespace

int X86_64InternalAssembler::parse_reg(const std::string &r) const {
  if (r == "%rax" || r == "%eax" || r == "%ax" || r == "%al") return 0;
  if (r == "%rcx" || r == "%ecx" || r == "%cx" || r == "%cl") return 1;
  if (r == "%rdx" || r == "%edx" || r == "%dx" || r == "%dl") return 2;
  if (r == "%rbx" || r == "%ebx" || r == "%bx" || r == "%bl") return 3;
  if (r == "%rsp" || r == "%esp") return 4;
  if (r == "%rbp" || r == "%ebp") return 5;
  if (r == "%rsi" || r == "%esi") return 6;
  if (r == "%rdi" || r == "%edi") return 7;
  if (r == "%r8"  || r == "%r8d") return 8;
  if (r == "%r9"  || r == "%r9d") return 9;
  if (r == "%r10" || r == "%r10d") return 10;
  if (r == "%r11" || r == "%r11d") return 11;
  if (r == "%r12" || r == "%r12d") return 12;
  if (r == "%r13" || r == "%r13d") return 13;
  if (r == "%r14" || r == "%r14d") return 14;
  if (r == "%r15" || r == "%r15d") return 15;
  return -1;
}

uint8_t X86_64InternalAssembler::make_rex(bool w, int r, int x, int b) const {
  return 0x40 | ((w ? 1 : 0) << 3) | (((r >= 8) ? 1 : 0) << 2) |
         (((x >= 8) ? 1 : 0) << 1) | (((b >= 8) ? 1 : 0));
}

uint8_t X86_64InternalAssembler::make_modrm(int mod, int reg, int rm) const {
  return static_cast<uint8_t>(((mod & 3) << 6) | ((reg & 7) << 3) | (rm & 7));
}

uint8_t X86_64InternalAssembler::make_sib(int scale, int index, int base) const {
  int scale_code = 0;
  if (scale == 2) scale_code = 1;
  else if (scale == 4) scale_code = 2;
  else if (scale == 8) scale_code = 3;
  return static_cast<uint8_t>(((scale_code & 3) << 6) | ((index & 7) << 3) | (base & 7));
}

void X86_64InternalAssembler::emit_bytes(std::vector<uint8_t> &dest,
                                         std::initializer_list<uint8_t> bytes) {
  for (uint8_t b : bytes) dest.push_back(b);
}

void X86_64InternalAssembler::emit_int32(std::vector<uint8_t> &dest, int32_t val) {
  uint32_t u = static_cast<uint32_t>(val);
  dest.push_back(static_cast<uint8_t>(u & 0xFF));
  dest.push_back(static_cast<uint8_t>((u >> 8) & 0xFF));
  dest.push_back(static_cast<uint8_t>((u >> 16) & 0xFF));
  dest.push_back(static_cast<uint8_t>((u >> 24) & 0xFF));
}

void X86_64InternalAssembler::emit_int64(std::vector<uint8_t> &dest, int64_t val) {
  uint64_t u = static_cast<uint64_t>(val);
  for (int i = 0; i < 8; ++i) {
    dest.push_back(static_cast<uint8_t>((u >> (i * 8)) & 0xFF));
  }
}

AssembledModule X86_64InternalAssembler::assemble(const std::string &assembly_source) {
  AssembledModule module;
  enum Section { SecText, SecData } current_sec = SecText;

  std::istringstream stream(assembly_source);
  std::string line;

  while (std::getline(stream, line)) {
    // Strip comments
    size_t comment_pos = line.find_first_of(";#");
    if (comment_pos != std::string::npos) {
      line = line.substr(0, comment_pos);
    }
    line = trim(line);
    if (line.empty()) continue;

    // Label declaration: e.g. "main:", ".L1:", ".LC_1:"
    if (line.back() == ':') {
      std::string label = line.substr(0, line.length() - 1);
      if (current_sec == SecText) {
        module.symbol_offsets[label] = static_cast<uint32_t>(module.text_bytes.size());
      } else {
        module.data_offsets[label] = static_cast<uint32_t>(module.data_bytes.size());
      }
      continue;
    }

    // Directives
    if (line[0] == '.') {
      std::istringstream ls(line);
      std::string dir;
      ls >> dir;
      if (dir == ".globl" || dir == ".global") {
        std::string sym;
        ls >> sym;
        if (!sym.empty()) {
          module.global_symbols.insert(sym);
        }
        continue;
      } else if (dir == ".file") {
        uint32_t fid = 1;
        std::string next_token;
        ls >> next_token;
        if (!next_token.empty() && std::isdigit(static_cast<unsigned char>(next_token[0]))) {
          fid = static_cast<uint32_t>(std::stoul(next_token));
        }
        size_t q1 = line.find('"');
        size_t q2 = line.rfind('"');
        if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
          module.file_table[fid] = line.substr(q1 + 1, q2 - q1 - 1);
        }
        continue;
      } else if (dir == ".loc") {
        uint32_t fid = 1;
        uint32_t line_num = 0;
        uint32_t col = 0;
        ls >> fid >> line_num;
        if (ls >> col) {}
        if (line_num > 0) {
          module.source_locs.push_back({module.text_bytes.size(), fid, line_num, col});
        }
        continue;
      } else if (dir == ".local") {
        std::string sym;
        ls >> sym;
        if (!sym.empty()) {
          module.local_symbols.insert(sym);
        }
        continue;
      } else if (dir == ".text") {
        current_sec = SecText;
      } else if (dir == ".section") {
        std::string sec_name;
        ls >> sec_name;
        if (sec_name.find(".rodata") != std::string::npos || sec_name.find(".rdata") != std::string::npos) {
          current_sec = SecData;
        } else {
          current_sec = SecText;
        }
      } else if (dir == ".string" || dir == ".asciz" || dir == ".ascii") {
        size_t q1 = line.find('"');
        size_t q2 = line.rfind('"');
        if (q1 != std::string::npos && q2 != std::string::npos && q2 > q1) {
          std::string raw = line.substr(q1 + 1, q2 - q1 - 1);
          for (size_t i = 0; i < raw.length(); ++i) {
            if (raw[i] == '\\' && i + 1 < raw.length()) {
              i++;
              if (raw[i] == 'n') module.data_bytes.push_back('\n');
              else if (raw[i] == 't') module.data_bytes.push_back('\t');
              else if (raw[i] == 'r') module.data_bytes.push_back('\r');
              else if (raw[i] == '0') module.data_bytes.push_back('\0');
              else if (raw[i] == '\\') module.data_bytes.push_back('\\');
              else if (raw[i] == '"') module.data_bytes.push_back('"');
              else if (std::isdigit(static_cast<unsigned char>(raw[i]))) {
                int val = raw[i] - '0';
                if (i + 1 < raw.length() && std::isdigit(static_cast<unsigned char>(raw[i + 1]))) {
                  val = val * 8 + (raw[++i] - '0');
                  if (i + 1 < raw.length() && std::isdigit(static_cast<unsigned char>(raw[i + 1]))) {
                    val = val * 8 + (raw[++i] - '0');
                  }
                }
                module.data_bytes.push_back(static_cast<uint8_t>(val));
              } else {
                module.data_bytes.push_back(static_cast<uint8_t>(raw[i]));
              }
            } else {
              module.data_bytes.push_back(static_cast<uint8_t>(raw[i]));
            }
          }
          if (dir != ".ascii") {
            module.data_bytes.push_back(0); // NUL terminator
          }
        }
      }
      continue;
    }

    // Must be an instruction in .text
    size_t space_pos = line.find_first_of(" \t");
    std::string mnemonic = (space_pos != std::string::npos) ? line.substr(0, space_pos) : line;
    std::string op_str = (space_pos != std::string::npos) ? trim(line.substr(space_pos + 1)) : "";
    auto ops = split_operands(op_str);

    // 1. pushq %reg / popq %reg
    if (mnemonic == "pushq" && ops.size() == 1) {
      int reg = parse_reg(ops[0]);
      if (reg >= 8) emit_bytes(module.text_bytes, {0x41, static_cast<uint8_t>(0x50 + (reg - 8))});
      else emit_bytes(module.text_bytes, {static_cast<uint8_t>(0x50 + reg)});
    } else if (mnemonic == "popq" && ops.size() == 1) {
      int reg = parse_reg(ops[0]);
      if (reg >= 8) emit_bytes(module.text_bytes, {0x41, static_cast<uint8_t>(0x58 + (reg - 8))});
      else emit_bytes(module.text_bytes, {static_cast<uint8_t>(0x58 + reg)});
    }
    // 2. leave / ret / cqto / nop / syscall / int3 / pause / ud2
    else if (mnemonic == "leave") {
      emit_bytes(module.text_bytes, {0xC9});
    } else if (mnemonic == "ret") {
      emit_bytes(module.text_bytes, {0xC3});
    } else if (mnemonic == "cqto") {
      emit_bytes(module.text_bytes, {0x48, 0x99});
    } else if (mnemonic == "nop") {
      emit_bytes(module.text_bytes, {0x90});
    } else if (mnemonic == "int3") {
      emit_bytes(module.text_bytes, {0xCC});
    } else if (mnemonic == "syscall") {
      emit_bytes(module.text_bytes, {0x0F, 0x05});
    } else if (mnemonic == "pause") {
      emit_bytes(module.text_bytes, {0xF3, 0x90});
    } else if (mnemonic == "ud2") {
      emit_bytes(module.text_bytes, {0x0F, 0x0B});
    } else if (mnemonic == "clc") {
      emit_bytes(module.text_bytes, {0xF8});
    } else if (mnemonic == "stc") {
      emit_bytes(module.text_bytes, {0xF9});
    } else if (mnemonic == "cld") {
      emit_bytes(module.text_bytes, {0xFC});
    } else if (mnemonic == "std") {
      emit_bytes(module.text_bytes, {0xFD});
    } else if (mnemonic == "hlt") {
      emit_bytes(module.text_bytes, {0xF4});
    }
    // 3. movzbq %al, %rax
    else if (mnemonic == "movzbq" && ops.size() == 2) {
      emit_bytes(module.text_bytes, {0x48, 0x0F, 0xB6, 0xC0});
    }
    // 4. setcc %al
    else if (mnemonic.rfind("set", 0) == 0 && ops.size() == 1) {
      uint8_t cc = 0x94; // sete
      if (mnemonic == "sete") cc = 0x94;
      else if (mnemonic == "setne") cc = 0x95;
      else if (mnemonic == "setl") cc = 0x9C;
      else if (mnemonic == "setge") cc = 0x9D;
      else if (mnemonic == "setle") cc = 0x9E;
      else if (mnemonic == "setg") cc = 0x9F;
      emit_bytes(module.text_bytes, {0x0F, cc, 0xC0});
    }
    // 5. negq / notq / idivq %reg
    else if ((mnemonic == "negq" || mnemonic == "notq" || mnemonic == "idivq") && ops.size() == 1) {
      int reg = parse_reg(ops[0]);
      uint8_t rex = make_rex(true, 0, 0, reg);
      uint8_t reg_code = (mnemonic == "negq") ? 3 : (mnemonic == "notq" ? 2 : 7);
      emit_bytes(module.text_bytes, {rex, 0xF7, make_modrm(3, reg_code, reg)});
    }
    // 6. salq / sarq / shlq / shrq %cl, %reg
    else if ((mnemonic == "salq" || mnemonic == "sarq" || mnemonic == "shlq" || mnemonic == "shrq") && ops.size() == 2) {
      int dst = parse_reg(ops[1]);
      uint8_t rex = make_rex(true, 0, 0, dst);
      uint8_t reg_code = (mnemonic == "shrq") ? 5 : ((mnemonic == "sarq") ? 7 : 4);
      emit_bytes(module.text_bytes, {rex, 0xD3, make_modrm(3, reg_code, dst)});
    }
    // 7. subq / addq / andq with immediate ($imm, %reg)
    else if ((mnemonic == "subq" || mnemonic == "addq" || mnemonic == "andq" || mnemonic == "cmpq") &&
             ops.size() == 2 && ops[0][0] == '$' && ops[1][0] == '%') {
      int64_t imm = parse_immediate(ops[0]);
      int reg = parse_reg(ops[1]);
      uint8_t rex = make_rex(true, 0, 0, reg);
      uint8_t reg_code = (mnemonic == "addq") ? 0 : (mnemonic == "andq" ? 4 : (mnemonic == "subq" ? 5 : 7));
      if (imm >= -128 && imm <= 127) {
        emit_bytes(module.text_bytes, {rex, 0x83, make_modrm(3, reg_code, reg), static_cast<uint8_t>(imm & 0xFF)});
      } else {
        emit_bytes(module.text_bytes, {rex, 0x81, make_modrm(3, reg_code, reg)});
        emit_int32(module.text_bytes, static_cast<int32_t>(imm));
      }
    }
    // 8. imulq $imm, %reg
    else if (mnemonic == "imulq" && ops.size() == 2 && ops[0][0] == '$' && ops[1][0] == '%') {
      int64_t imm = parse_immediate(ops[0]);
      int reg = parse_reg(ops[1]);
      uint8_t rex = make_rex(true, reg, 0, reg);
      emit_bytes(module.text_bytes, {rex, 0x69, make_modrm(3, reg, reg)});
      emit_int32(module.text_bytes, static_cast<int32_t>(imm));
    }
    // 9. Binary register-register ops: addq, subq, imulq, andq, orq, xorq, cmpq, testq
    else if (ops.size() == 2 && ops[0][0] == '%' && ops[1][0] == '%') {
      int src = parse_reg(ops[0]);
      int dst = parse_reg(ops[1]);
      if (mnemonic == "movq") {
        emit_bytes(module.text_bytes, {make_rex(true, src, 0, dst), 0x89, make_modrm(3, src, dst)});
      } else if (mnemonic == "addq") {
        emit_bytes(module.text_bytes, {make_rex(true, src, 0, dst), 0x01, make_modrm(3, src, dst)});
      } else if (mnemonic == "subq") {
        emit_bytes(module.text_bytes, {make_rex(true, src, 0, dst), 0x29, make_modrm(3, src, dst)});
      } else if (mnemonic == "imulq") {
        emit_bytes(module.text_bytes, {make_rex(true, dst, 0, src), 0x0F, 0xAF, make_modrm(3, dst, src)});
      } else if (mnemonic == "andq") {
        emit_bytes(module.text_bytes, {make_rex(true, src, 0, dst), 0x21, make_modrm(3, src, dst)});
      } else if (mnemonic == "orq") {
        emit_bytes(module.text_bytes, {make_rex(true, src, 0, dst), 0x09, make_modrm(3, src, dst)});
      } else if (mnemonic == "xorq") {
        emit_bytes(module.text_bytes, {make_rex(true, src, 0, dst), 0x31, make_modrm(3, src, dst)});
      } else if (mnemonic == "cmpq") {
        emit_bytes(module.text_bytes, {make_rex(true, src, 0, dst), 0x39, make_modrm(3, src, dst)});
      } else if (mnemonic == "testq") {
        emit_bytes(module.text_bytes, {make_rex(true, src, 0, dst), 0x85, make_modrm(3, src, dst)});
      }
    }
    // 10. movq $imm, %reg
    else if (mnemonic == "movq" && ops.size() == 2 && ops[0][0] == '$' && ops[1][0] == '%') {
      int64_t imm = parse_immediate(ops[0]);
      int reg = parse_reg(ops[1]);
      emit_bytes(module.text_bytes, {make_rex(true, 0, 0, reg), static_cast<uint8_t>(0xB8 + (reg & 7))});
      emit_int64(module.text_bytes, imm);
    }
    // 11. Memory load/store: e.g. movq -16(%rbp), %rax or movq %rax, -16(%rbp)
    else if ((mnemonic == "movq" || mnemonic == "movb" || mnemonic == "movw" ||
              mnemonic == "movsbq" || mnemonic == "movswq" || mnemonic == "leaq") && ops.size() == 2) {
      bool is_load = (ops[0].find('(') != std::string::npos);
      std::string mem_str = is_load ? ops[0] : ops[1];
      std::string reg_str = is_load ? ops[1] : ops[0];
      int reg = parse_reg(reg_str);

      // Check RIP-relative: label(%rip)
      size_t rip_pos = mem_str.find("(%rip)");
      if (rip_pos != std::string::npos) {
        std::string label = mem_str.substr(0, rip_pos);
        if (mnemonic == "leaq") {
          emit_bytes(module.text_bytes, {make_rex(true, reg, 0, 0), 0x8D, make_modrm(0, reg, 5)});
          module.relocations.push_back({module.text_bytes.size(), label, RelocType::Rip_Data});
          emit_int32(module.text_bytes, 0);
        }
        continue;
      }

      // SIB indexing: (%base, %idx, scale)
      size_t comma_pos = mem_str.find(',');
      if (comma_pos != std::string::npos) {
        size_t open_p = mem_str.find('(');
        size_t close_p = mem_str.find(')');
        if (open_p != std::string::npos && close_p != std::string::npos) {
          std::string inner = mem_str.substr(open_p + 1, close_p - open_p - 1);
          auto sib_parts = split_operands(inner);
          if (sib_parts.size() == 3) {
            int base = parse_reg(sib_parts[0]);
            int index = parse_reg(sib_parts[1]);
            int scale = std::stoi(sib_parts[2]);
            uint8_t rex = make_rex(mnemonic != "movb" && mnemonic != "movw", reg, index, base);
            uint8_t sib = make_sib(scale, index, base);

            if (mnemonic == "leaq") {
              emit_bytes(module.text_bytes, {rex, 0x8D, make_modrm(0, reg, 4), sib});
            } else if (is_load) {
              if (mnemonic == "movsbq") emit_bytes(module.text_bytes, {rex, 0x0F, 0xBE, make_modrm(0, reg, 4), sib});
              else if (mnemonic == "movswq") emit_bytes(module.text_bytes, {rex, 0x0F, 0xBF, make_modrm(0, reg, 4), sib});
              else emit_bytes(module.text_bytes, {rex, 0x8B, make_modrm(0, reg, 4), sib});
            } else {
              if (mnemonic == "movb") emit_bytes(module.text_bytes, {0x88, make_modrm(0, reg, 4), sib});
              else if (mnemonic == "movw") emit_bytes(module.text_bytes, {0x66, 0x89, make_modrm(0, reg, 4), sib});
              else emit_bytes(module.text_bytes, {rex, 0x89, make_modrm(0, reg, 4), sib});
            }
            continue;
          }
        }
      }

      // Base + displacement: disp(%reg) or (%reg)
      size_t paren_open = mem_str.find('(');
      size_t paren_close = mem_str.find(')');
      if (paren_open != std::string::npos && paren_close != std::string::npos) {
        int64_t disp = 0;
        std::string disp_str = mem_str.substr(0, paren_open);
        if (!disp_str.empty()) disp = std::stoll(disp_str, nullptr, 0);
        std::string base_str = mem_str.substr(paren_open + 1, paren_close - paren_open - 1);
        int base = parse_reg(base_str);

        int mod = (disp == 0 && (base & 7) != 5) ? 0 : ((disp >= -128 && disp <= 127) ? 1 : 2);
        uint8_t rex = make_rex(mnemonic != "movb" && mnemonic != "movw", reg, 0, base);

        if (mnemonic == "leaq") {
          emit_bytes(module.text_bytes, {rex, 0x8D, make_modrm(mod, reg, base)});
        } else if (is_load) {
          if (mnemonic == "movsbq") emit_bytes(module.text_bytes, {rex, 0x0F, 0xBE, make_modrm(mod, reg, base)});
          else if (mnemonic == "movswq") emit_bytes(module.text_bytes, {rex, 0x0F, 0xBF, make_modrm(mod, reg, base)});
          else emit_bytes(module.text_bytes, {rex, 0x8B, make_modrm(mod, reg, base)});
        } else {
          if (mnemonic == "movb") emit_bytes(module.text_bytes, {0x88, make_modrm(mod, reg, base)});
          else if (mnemonic == "movw") emit_bytes(module.text_bytes, {0x66, 0x89, make_modrm(mod, reg, base)});
          else emit_bytes(module.text_bytes, {rex, 0x89, make_modrm(mod, reg, base)});
        }

        if ((base & 7) == 4) {
          emit_bytes(module.text_bytes, {0x24});
        }

        if (mod == 1) emit_bytes(module.text_bytes, {static_cast<uint8_t>(disp & 0xFF)});
        else if (mod == 2) emit_int32(module.text_bytes, static_cast<int32_t>(disp));
      }
    }
    // 12. call <sym> / call *%reg
    else if (mnemonic == "call" && ops.size() == 1) {
      if (ops[0] == "*%r11") {
        emit_bytes(module.text_bytes, {0x41, 0xFF, 0xD3});
      } else if (ops[0] == "*%rax") {
        emit_bytes(module.text_bytes, {0xFF, 0xD0});
      } else {
        emit_bytes(module.text_bytes, {0xE8});
        module.relocations.push_back({module.text_bytes.size(), ops[0], RelocType::Rel32_Code});
        emit_int32(module.text_bytes, 0);
      }
    }
    // 13. jmp / jcc <label>
    else if (ops.size() == 1 && (mnemonic == "jmp" || mnemonic.rfind("j", 0) == 0)) {
      std::string target = ops[0];
      if (mnemonic == "jmp") {
        emit_bytes(module.text_bytes, {0xE9});
      } else {
        uint8_t cc = 0x84; // je
        if (mnemonic == "je") cc = 0x84;
        else if (mnemonic == "jne") cc = 0x85;
        else if (mnemonic == "jl") cc = 0x8C;
        else if (mnemonic == "jge") cc = 0x8D;
        else if (mnemonic == "jle") cc = 0x8E;
        else if (mnemonic == "jg") cc = 0x8F;
        emit_bytes(module.text_bytes, {0x0F, cc});
      }
      module.relocations.push_back({module.text_bytes.size(), target, RelocType::Rel32_Code});
      emit_int32(module.text_bytes, 0);
    }
  }

  // Identify external symbols (called but not defined locally)
  for (const auto &reloc : module.relocations) {
    if (reloc.type == RelocType::Rel32_Code) {
      if (module.symbol_offsets.find(reloc.symbol) == module.symbol_offsets.end()) {
        module.external_symbols.insert(reloc.symbol);
      }
    }
  }

  return module;
}
