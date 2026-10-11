#include "internal_elf_linker.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

namespace {

#pragma pack(push, 1)
struct Elf64_Ehdr {
  uint8_t e_ident[16];
  uint16_t e_type;
  uint16_t e_machine;
  uint32_t e_version;
  uint64_t e_entry;
  uint64_t e_phoff;
  uint64_t e_shoff;
  uint32_t e_flags;
  uint16_t e_ehsize;
  uint16_t e_phentsize;
  uint16_t e_phnum;
  uint16_t e_shentsize;
  uint16_t e_shnum;
  uint16_t e_shstrndx;
};
static_assert(sizeof(Elf64_Ehdr) == 64, "Elf64_Ehdr must be 64 bytes");

struct Elf64_Phdr {
  uint32_t p_type;
  uint32_t p_flags;
  uint64_t p_offset;
  uint64_t p_vaddr;
  uint64_t p_paddr;
  uint64_t p_filesz;
  uint64_t p_memsz;
  uint64_t p_align;
};
static_assert(sizeof(Elf64_Phdr) == 56, "Elf64_Phdr must be 56 bytes");

struct Elf64_Shdr {
  uint32_t sh_name;
  uint32_t sh_type;
  uint64_t sh_flags;
  uint64_t sh_addr;
  uint64_t sh_offset;
  uint64_t sh_size;
  uint32_t sh_link;
  uint32_t sh_info;
  uint64_t sh_addralign;
  uint64_t sh_entsize;
};
static_assert(sizeof(Elf64_Shdr) == 64, "Elf64_Shdr must be 64 bytes");
#pragma pack(pop)

constexpr uint32_t ALIGN_PAGE = 0x1000;
constexpr uint64_t BASE_VADDR = 0x400000;
constexpr uint64_t TEXT_VADDR = BASE_VADDR + ALIGN_PAGE; // 0x401000

uint32_t align_up(uint32_t val, uint32_t alignment) {
  return (val + alignment - 1) & ~(alignment - 1);
}

void write_int32_le(std::vector<uint8_t> &dest, size_t offset, int32_t val) {
  uint32_t u = static_cast<uint32_t>(val);
  dest[offset] = static_cast<uint8_t>(u & 0xFF);
  dest[offset + 1] = static_cast<uint8_t>((u >> 8) & 0xFF);
  dest[offset + 2] = static_cast<uint8_t>((u >> 16) & 0xFF);
  dest[offset + 3] = static_cast<uint8_t>((u >> 24) & 0xFF);
}

} // namespace

TargetFileType
InternalElfLinker::identify_target_type(const std::vector<AssembledModule> &modules) {
  for (const auto &m : modules) {
    if (m.symbol_offsets.find("main") != m.symbol_offsets.end() ||
        m.global_symbols.count("main") > 0) {
      return TargetFileType::Executable;
    }
  }
  return TargetFileType::ObjectFile;
}

bool InternalElfLinker::link(const std::vector<AssembledModule> &modules,
                             const std::string &output_filepath,
                             std::string &error_msg,
                             const LinkerConfig &/*config*/,
                             TargetFileType target_file_type) {
  if (modules.empty()) {
    error_msg = "Linker error: no input modules provided";
    return false;
  }

  if (target_file_type == TargetFileType::ObjectFile ||
      target_file_type == TargetFileType::SharedLib) {
    error_msg = "Linker error: shared library and object file output are not supported yet for ELF";
    return false;
  }

  // 1. Scan external symbols and check for 'main'
  bool has_main = false;
  std::set<std::string> all_externals;
  std::set<std::string> all_defined_text;
  std::set<std::string> all_defined_data;

  for (const auto &m : modules) {
    for (const auto &p : m.symbol_offsets) all_defined_text.insert(p.first);
    for (const auto &p : m.data_offsets) all_defined_data.insert(p.first);
    for (const auto &s : m.global_symbols) all_defined_text.insert(s);
  }

  if (all_defined_text.count("main") > 0) {
    has_main = true;
  }

  if (!has_main) {
    error_msg = "Linker error: undefined symbol 'main'";
    return false;
  }

  for (const auto &m : modules) {
    for (const auto &ext : m.external_symbols) {
      if (all_defined_text.find(ext) == all_defined_text.end() &&
          all_defined_data.find(ext) == all_defined_data.end()) {
        all_externals.insert(ext);
      }
    }
    for (const auto &reloc : m.relocations) {
      if (all_defined_text.find(reloc.symbol) == all_defined_text.end() &&
          all_defined_data.find(reloc.symbol) == all_defined_data.end()) {
        all_externals.insert(reloc.symbol);
      }
    }
  }

  // 2. Generate Linux SysV ABI CRT runtime code (_start + built-in standard I/O)
  std::ostringstream runtime_asm;
  runtime_asm << ".text\n";
  runtime_asm << ".globl _start\n";
  runtime_asm << "_start:\n";
  runtime_asm << "    xorq %rbp, %rbp\n";
  runtime_asm << "    andq $-16, %rsp\n";
  runtime_asm << "    call main\n";
  runtime_asm << "    movq %rax, %rdi\n";
  runtime_asm << "    movq $60, %rax\n";
  runtime_asm << "    syscall\n";

  // Built-in puts: write string at %rdi to stdout (fd 1), then newline '\n'
  if (all_externals.count("puts") > 0) {
    runtime_asm << ".globl puts\n";
    runtime_asm << "puts:\n";
    runtime_asm << "    pushq %rbx\n";
    runtime_asm << "    pushq %r12\n";
    runtime_asm << "    movq %rdi, %r12\n";
    runtime_asm << "    xorq %rbx, %rbx\n";
    runtime_asm << ".L_puts_len:\n";
    runtime_asm << "    movsbq (%r12, %rbx, 1), %rax\n";
    runtime_asm << "    testq %rax, %rax\n";
    runtime_asm << "    je .L_puts_write\n";
    runtime_asm << "    addq $1, %rbx\n";
    runtime_asm << "    jmp .L_puts_len\n";
    runtime_asm << ".L_puts_write:\n";
    runtime_asm << "    movq $1, %rax\n";
    runtime_asm << "    movq $1, %rdi\n";
    runtime_asm << "    movq %r12, %rsi\n";
    runtime_asm << "    movq %rbx, %rdx\n";
    runtime_asm << "    syscall\n";
    runtime_asm << "    movq $10, %rax\n";
    runtime_asm << "    pushq %rax\n";
    runtime_asm << "    movq $1, %rax\n";
    runtime_asm << "    movq $1, %rdi\n";
    runtime_asm << "    movq %rsp, %rsi\n";
    runtime_asm << "    movq $1, %rdx\n";
    runtime_asm << "    syscall\n";
    runtime_asm << "    popq %rax\n";
    runtime_asm << "    xorq %rax, %rax\n";
    runtime_asm << "    popq %r12\n";
    runtime_asm << "    popq %rbx\n";
    runtime_asm << "    ret\n";
  }

  // Built-in printf: write null-terminated format/string at %rdi to stdout
  if (all_externals.count("printf") > 0) {
    runtime_asm << ".globl printf\n";
    runtime_asm << "printf:\n";
    runtime_asm << "    pushq %rbx\n";
    runtime_asm << "    pushq %r12\n";
    runtime_asm << "    movq %rdi, %r12\n";
    runtime_asm << "    xorq %rbx, %rbx\n";
    runtime_asm << ".L_printf_len:\n";
    runtime_asm << "    movsbq (%r12, %rbx, 1), %rax\n";
    runtime_asm << "    testq %rax, %rax\n";
    runtime_asm << "    je .L_printf_write\n";
    runtime_asm << "    addq $1, %rbx\n";
    runtime_asm << "    jmp .L_printf_len\n";
    runtime_asm << ".L_printf_write:\n";
    runtime_asm << "    movq $1, %rax\n";
    runtime_asm << "    movq $1, %rdi\n";
    runtime_asm << "    movq %r12, %rsi\n";
    runtime_asm << "    movq %rbx, %rdx\n";
    runtime_asm << "    syscall\n";
    runtime_asm << "    movq %rbx, %rax\n";
    runtime_asm << "    popq %r12\n";
    runtime_asm << "    popq %rbx\n";
    runtime_asm << "    ret\n";
  }

  // Built-in putchar: output 1 byte character in %rdi
  if (all_externals.count("putchar") > 0) {
    runtime_asm << ".globl putchar\n";
    runtime_asm << "putchar:\n";
    runtime_asm << "    pushq %rdi\n";
    runtime_asm << "    movq $1, %rax\n";
    runtime_asm << "    movq %rsp, %rsi\n";
    runtime_asm << "    movq $1, %rdi\n";
    runtime_asm << "    movq $1, %rdx\n";
    runtime_asm << "    syscall\n";
    runtime_asm << "    popq %rax\n";
    runtime_asm << "    ret\n";
  }

  // Built-in exit: sys_exit(status in %rdi)
  if (all_externals.count("exit") > 0) {
    runtime_asm << ".globl exit\n";
    runtime_asm << "exit:\n";
    runtime_asm << "    movq $60, %rax\n";
    runtime_asm << "    syscall\n";
    runtime_asm << "    ret\n";
  }

  // Built-in write: sys_write(fd=%rdi, buf=%rsi, count=%rdx)
  if (all_externals.count("write") > 0) {
    runtime_asm << ".globl write\n";
    runtime_asm << "write:\n";
    runtime_asm << "    movq $1, %rax\n";
    runtime_asm << "    syscall\n";
    runtime_asm << "    ret\n";
  }

  X86_64InternalAssembler asm_engine;
  AssembledModule rt_mod = asm_engine.assemble(runtime_asm.str());

  // 3. Assemble and combine all modules (Runtime CRT module first)
  std::vector<AssembledModule> all_modules;
  all_modules.push_back(std::move(rt_mod));
  for (const auto &m : modules) {
    all_modules.push_back(m);
  }

  AssembledModule combined;
  struct Placement {
    size_t text_offset{0};
    size_t data_offset{0};
  };
  std::vector<Placement> placements(all_modules.size());

  for (size_t i = 0; i < all_modules.size(); ++i) {
    const auto &m = all_modules[i];

    while (combined.text_bytes.size() % 16 != 0) {
      combined.text_bytes.push_back(0x90); // nop pad
    }
    placements[i].text_offset = combined.text_bytes.size();
    combined.text_bytes.insert(combined.text_bytes.end(), m.text_bytes.begin(), m.text_bytes.end());

    while (combined.data_bytes.size() % 16 != 0) {
      combined.data_bytes.push_back(0);
    }
    placements[i].data_offset = combined.data_bytes.size();
    combined.data_bytes.insert(combined.data_bytes.end(), m.data_bytes.begin(), m.data_bytes.end());

    auto is_global = [&](const std::string &sym) {
      return m.global_symbols.count(sym) > 0 || sym == "_start" || sym == "main" ||
             sym == "puts" || sym == "printf" || sym == "putchar" || sym == "exit" || sym == "write";
    };

    // Register text symbols
    for (const auto &pair : m.symbol_offsets) {
      const std::string &name = pair.first;
      uint32_t off = pair.second;
      if (is_global(name)) {
        if (combined.symbol_offsets.find(name) != combined.symbol_offsets.end()) {
          error_msg = "Linker error: multiple definition of symbol '" + name + "'";
          return false;
        }
        combined.symbol_offsets[name] = static_cast<uint32_t>(placements[i].text_offset + off);
        combined.global_symbols.insert(name);
      } else {
        std::string scoped = name + "@mod" + std::to_string(i);
        combined.symbol_offsets[scoped] = static_cast<uint32_t>(placements[i].text_offset + off);
      }
    }

    // Register data symbols
    for (const auto &pair : m.data_offsets) {
      const std::string &name = pair.first;
      uint32_t off = pair.second;
      if (is_global(name)) {
        if (combined.data_offsets.find(name) != combined.data_offsets.end()) {
          error_msg = "Linker error: multiple definition of data symbol '" + name + "'";
          return false;
        }
        combined.data_offsets[name] = static_cast<uint32_t>(placements[i].data_offset + off);
      } else {
        std::string scoped = name + "@mod" + std::to_string(i);
        combined.data_offsets[scoped] = static_cast<uint32_t>(placements[i].data_offset + off);
      }
    }

    // Map relocations
    for (const auto &reloc : m.relocations) {
      Relocation r = reloc;
      r.text_offset = placements[i].text_offset + reloc.text_offset;
      bool is_local_to_m = (m.symbol_offsets.find(reloc.symbol) != m.symbol_offsets.end() ||
                            m.data_offsets.find(reloc.symbol) != m.data_offsets.end()) &&
                           !is_global(reloc.symbol);
      if (is_local_to_m) {
        r.symbol = reloc.symbol + "@mod" + std::to_string(i);
      }
      combined.relocations.push_back(r);
    }
  }

  // 4. Resolve relocations
  uint32_t text_size = static_cast<uint32_t>(combined.text_bytes.size());
  uint32_t data_offset_in_segment = align_up(text_size, 16);
  uint64_t data_vaddr = TEXT_VADDR + data_offset_in_segment;

  for (const auto &reloc : combined.relocations) {
    if (reloc.type == RelocType::Rel32_Code) {
      auto it = combined.symbol_offsets.find(reloc.symbol);
      if (it == combined.symbol_offsets.end()) {
        error_msg = "Linker error: undefined code symbol '" + reloc.symbol + "'";
        return false;
      }
      uint64_t target_vaddr = TEXT_VADDR + it->second;
      uint64_t next_insn_vaddr = TEXT_VADDR + reloc.text_offset + 4;
      int32_t disp = static_cast<int32_t>(target_vaddr - next_insn_vaddr);
      write_int32_le(combined.text_bytes, reloc.text_offset, disp);
    } else if (reloc.type == RelocType::Rip_Data) {
      auto it = combined.data_offsets.find(reloc.symbol);
      if (it == combined.data_offsets.end()) {
        error_msg = "Linker error: unresolved data label '" + reloc.symbol + "'";
        return false;
      }
      uint64_t target_vaddr = data_vaddr + it->second;
      uint64_t next_insn_vaddr = TEXT_VADDR + reloc.text_offset + 4;
      int32_t disp = static_cast<int32_t>(target_vaddr - next_insn_vaddr);
      write_int32_le(combined.text_bytes, reloc.text_offset, disp);
    }
  }

  // 5. Build section header string table (.shstrtab)
  std::vector<uint8_t> shstrtab;
  shstrtab.push_back(0); // null name

  uint32_t name_text = static_cast<uint32_t>(shstrtab.size());
  for (char c : std::string(".text")) shstrtab.push_back(static_cast<uint8_t>(c));
  shstrtab.push_back(0);

  uint32_t name_rodata = static_cast<uint32_t>(shstrtab.size());
  for (char c : std::string(".rodata")) shstrtab.push_back(static_cast<uint8_t>(c));
  shstrtab.push_back(0);

  uint32_t name_shstrtab = static_cast<uint32_t>(shstrtab.size());
  for (char c : std::string(".shstrtab")) shstrtab.push_back(static_cast<uint8_t>(c));
  shstrtab.push_back(0);

  // 6. Layout binary data
  std::vector<uint8_t> file_bytes(ALIGN_PAGE, 0); // 4096-byte header area

  // Append .text
  size_t text_file_offset = file_bytes.size();
  file_bytes.insert(file_bytes.end(), combined.text_bytes.begin(), combined.text_bytes.end());

  // Pad to 16-byte alignment before data
  while ((file_bytes.size() - text_file_offset) < data_offset_in_segment) {
    file_bytes.push_back(0);
  }

  // Append .rodata / data
  size_t data_file_offset = file_bytes.size();
  file_bytes.insert(file_bytes.end(), combined.data_bytes.begin(), combined.data_bytes.end());

  // Align to 8-byte boundary for section headers
  while (file_bytes.size() % 8 != 0) {
    file_bytes.push_back(0);
  }

  size_t shoff = file_bytes.size();
  size_t num_sections = 4;
  std::vector<Elf64_Shdr> sections(num_sections);
  std::memset(sections.data(), 0, num_sections * sizeof(Elf64_Shdr));

  // Section 1: .text
  sections[1].sh_name = name_text;
  sections[1].sh_type = 1; // SHT_PROGBITS
  sections[1].sh_flags = 6; // SHF_ALLOC | SHF_EXECINSTR
  sections[1].sh_addr = TEXT_VADDR;
  sections[1].sh_offset = text_file_offset;
  sections[1].sh_size = text_size;
  sections[1].sh_addralign = 16;

  // Section 2: .rodata
  sections[2].sh_name = name_rodata;
  sections[2].sh_type = 1; // SHT_PROGBITS
  sections[2].sh_flags = 2; // SHF_ALLOC
  sections[2].sh_addr = data_vaddr;
  sections[2].sh_offset = data_file_offset;
  sections[2].sh_size = combined.data_bytes.size();
  sections[2].sh_addralign = 16;

  // Append section header table to file_bytes
  const uint8_t *shdr_bytes = reinterpret_cast<const uint8_t *>(sections.data());
  file_bytes.insert(file_bytes.end(), shdr_bytes, shdr_bytes + num_sections * sizeof(Elf64_Shdr));

  // Section 3: .shstrtab
  size_t shstrtab_file_offset = file_bytes.size();
  file_bytes.insert(file_bytes.end(), shstrtab.begin(), shstrtab.end());

  // Fix up Section 3 header in file_bytes
  Elf64_Shdr shstrtab_hdr{};
  shstrtab_hdr.sh_name = name_shstrtab;
  shstrtab_hdr.sh_type = 3; // SHT_STRTAB
  shstrtab_hdr.sh_flags = 0;
  shstrtab_hdr.sh_addr = 0;
  shstrtab_hdr.sh_offset = shstrtab_file_offset;
  shstrtab_hdr.sh_size = shstrtab.size();
  shstrtab_hdr.sh_addralign = 1;
  std::memcpy(&file_bytes[shoff + 3 * sizeof(Elf64_Shdr)], &shstrtab_hdr, sizeof(Elf64_Shdr));

  // 7. Fill ELF header and Program header
  Elf64_Ehdr ehdr{};
  ehdr.e_ident[0] = 0x7F;
  ehdr.e_ident[1] = 'E';
  ehdr.e_ident[2] = 'L';
  ehdr.e_ident[3] = 'F';
  ehdr.e_ident[4] = 2; // ELFCLASS64
  ehdr.e_ident[5] = 1; // ELFDATA2LSB
  ehdr.e_ident[6] = 1; // EV_CURRENT
  ehdr.e_ident[7] = 0; // ELFOSABI_NONE / System V
  ehdr.e_type = 2;     // ET_EXEC
  ehdr.e_machine = 62; // EM_X86_64
  ehdr.e_version = 1;
  ehdr.e_entry = TEXT_VADDR; // _start is at text offset 0
  ehdr.e_phoff = sizeof(Elf64_Ehdr);
  ehdr.e_shoff = shoff;
  ehdr.e_flags = 0;
  ehdr.e_ehsize = sizeof(Elf64_Ehdr);
  ehdr.e_phentsize = sizeof(Elf64_Phdr);
  ehdr.e_phnum = 1;
  ehdr.e_shentsize = sizeof(Elf64_Shdr);
  ehdr.e_shnum = static_cast<uint16_t>(num_sections);
  ehdr.e_shstrndx = 3;

  Elf64_Phdr phdr{};
  phdr.p_type = 1;  // PT_LOAD
  phdr.p_flags = 7; // PF_R | PF_W | PF_X
  phdr.p_offset = 0;
  phdr.p_vaddr = BASE_VADDR;
  phdr.p_paddr = BASE_VADDR;
  phdr.p_filesz = file_bytes.size();
  phdr.p_memsz = file_bytes.size();
  phdr.p_align = ALIGN_PAGE;

  std::memcpy(&file_bytes[0], &ehdr, sizeof(Elf64_Ehdr));
  std::memcpy(&file_bytes[sizeof(Elf64_Ehdr)], &phdr, sizeof(Elf64_Phdr));

  // 8. Write executable output file
  std::ofstream out(output_filepath, std::ios::binary);
  if (!out.is_open()) {
    error_msg = "Linker error: could not open output file '" + output_filepath + "' for writing";
    return false;
  }

  out.write(reinterpret_cast<const char *>(file_bytes.data()), file_bytes.size());
  if (!out.good()) {
    error_msg = "Linker error: failed writing bytes to output file '" + output_filepath + "'";
    return false;
  }

  return true;
}
