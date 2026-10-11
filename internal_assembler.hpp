#ifndef INTERNAL_ASSEMBLER_HPP
#define INTERNAL_ASSEMBLER_HPP

#include <cstdint>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

enum class RelocType {
  Rel32_Code,
  Rip_Data,
};

struct Relocation {
  size_t text_offset{0};
  std::string symbol;
  RelocType type{RelocType::Rel32_Code};
};

struct AssembledModule {
  std::vector<uint8_t> text_bytes;
  std::vector<uint8_t> data_bytes;
  std::unordered_map<std::string, uint32_t> symbol_offsets; // label in text_bytes
  std::unordered_map<std::string, uint32_t> data_offsets;   // label in data_bytes
  std::vector<Relocation> relocations;
  std::set<std::string> external_symbols;
};

class X86_64InternalAssembler {
public:
  AssembledModule assemble(const std::string &assembly_source);

private:
  int parse_reg(const std::string &r) const;
  uint8_t make_rex(bool w, int r, int x, int b) const;
  uint8_t make_modrm(int mod, int reg, int rm) const;
  uint8_t make_sib(int scale, int index, int base) const;
  void emit_bytes(std::vector<uint8_t> &dest, std::initializer_list<uint8_t> bytes);
  void emit_int32(std::vector<uint8_t> &dest, int32_t val);
  void emit_int64(std::vector<uint8_t> &dest, int64_t val);
};

#endif // INTERNAL_ASSEMBLER_HPP
