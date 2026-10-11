#include "internal_pe_linker.hpp"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <vector>

namespace {

#pragma pack(push, 1)

struct DosHeader {
  uint16_t e_magic{0x5A4D};
  uint16_t e_cblp{0x0090};
  uint16_t e_cp{0x0003};
  uint16_t e_crlc{0};
  uint16_t e_cparhdr{0x0004};
  uint16_t e_minalloc{0};
  uint16_t e_maxalloc{0xFFFF};
  uint16_t e_ss{0};
  uint16_t e_sp{0x00B8};
  uint16_t e_csum{0};
  uint16_t e_ip{0};
  uint16_t e_cs{0};
  uint16_t e_lfarlc{0x0040};
  uint16_t e_ovno{0};
  uint16_t e_res[4]{0};
  uint16_t e_oemid{0};
  uint16_t e_oeminfo{0};
  uint16_t e_res2[10]{0};
  uint32_t e_lfanew{0x0080};
};

struct FileHeader {
  uint16_t Machine{0x8664}; // AMD64
  uint16_t NumberOfSections{2};
  uint32_t TimeDateStamp{0};
  uint32_t PointerToSymbolTable{0};
  uint32_t NumberOfSymbols{0};
  uint16_t SizeOfOptionalHeader{240};
  uint16_t Characteristics{0x0022}; // EXECUTABLE_IMAGE | LARGE_ADDRESS_AWARE
};

struct DataDirectory {
  uint32_t VirtualAddress{0};
  uint32_t Size{0};
};

struct OptionalHeader64 {
  uint16_t Magic{0x020B}; // PE32+
  uint8_t MajorLinkerVersion{1};
  uint8_t MinorLinkerVersion{0};
  uint32_t SizeOfCode{0};
  uint32_t SizeOfInitializedData{0};
  uint32_t SizeOfUninitializedData{0};
  uint32_t AddressOfEntryPoint{0x1000};
  uint32_t BaseOfCode{0x1000};
  uint64_t ImageBase{0x140000000ULL};
  uint32_t SectionAlignment{0x1000};
  uint32_t FileAlignment{0x200};
  uint16_t MajorOperatingSystemVersion{6};
  uint16_t MinorOperatingSystemVersion{0};
  uint16_t MajorImageVersion{0};
  uint16_t MinorImageVersion{0};
  uint16_t MajorSubsystemVersion{6};
  uint16_t MinorSubsystemVersion{0};
  uint32_t Win32VersionValue{0};
  uint32_t SizeOfImage{0};
  uint32_t SizeOfHeaders{0x400};
  uint32_t CheckSum{0};
  uint16_t Subsystem{3}; // Windows CUI (Console)
  uint16_t DllCharacteristics{0};
  uint64_t SizeOfStackReserve{0x100000};
  uint64_t SizeOfStackCommit{0x1000};
  uint64_t SizeOfHeapReserve{0x100000};
  uint64_t SizeOfHeapCommit{0x1000};
  uint32_t LoaderFlags{0};
  uint32_t NumberOfRvaAndSizes{16};
  DataDirectory DataDirectory[16]{};
};

struct SectionHeader {
  char Name[8]{};
  uint32_t VirtualSize{0};
  uint32_t VirtualAddress{0};
  uint32_t SizeOfRawData{0};
  uint32_t PointerToRawData{0};
  uint32_t PointerToRelocations{0};
  uint32_t PointerToLinenumbers{0};
  uint16_t NumberOfRelocations{0};
  uint16_t NumberOfLinenumbers{0};
  uint32_t Characteristics{0};
};

struct ImportDescriptor {
  uint32_t OriginalFirstThunk{0};
  uint32_t TimeDateStamp{0};
  uint32_t ForwarderChain{0};
  uint32_t Name{0};
  uint32_t FirstThunk{0};
};

#pragma pack(pop)

uint32_t align_up(uint32_t val, uint32_t align) {
  return (val + align - 1) & ~(align - 1);
}

void write_int32_le(std::vector<uint8_t> &vec, size_t offset, int32_t val) {
  uint32_t u = static_cast<uint32_t>(val);
  vec[offset + 0] = static_cast<uint8_t>(u & 0xFF);
  vec[offset + 1] = static_cast<uint8_t>((u >> 8) & 0xFF);
  vec[offset + 2] = static_cast<uint8_t>((u >> 16) & 0xFF);
  vec[offset + 3] = static_cast<uint8_t>((u >> 24) & 0xFF);
}

uint32_t read_u32_be(const uint8_t *p) {
  return (static_cast<uint32_t>(p[0]) << 24) |
         (static_cast<uint32_t>(p[1]) << 16) |
         (static_cast<uint32_t>(p[2]) << 8) |
         static_cast<uint32_t>(p[3]);
}

} // namespace

bool InternalPELinker::read_dll_exports(const std::string &dll_path, std::set<std::string> &exports) {
  std::ifstream file(dll_path, std::ios::binary);
  if (!file.is_open()) return false;

  uint16_t mz = 0;
  file.read(reinterpret_cast<char *>(&mz), 2);
  if (mz != 0x5A4D) return false;

  file.seekg(0x3C);
  uint32_t pe_offset = 0;
  file.read(reinterpret_cast<char *>(&pe_offset), 4);

  file.seekg(pe_offset);
  uint32_t pe_sig = 0;
  file.read(reinterpret_cast<char *>(&pe_sig), 4);
  if (pe_sig != 0x00004550) return false;

  // File header
  file.seekg(pe_offset + 6);
  uint16_t num_sections = 0;
  file.read(reinterpret_cast<char *>(&num_sections), 2);

  file.seekg(pe_offset + 20);
  uint16_t opt_hdr_size = 0;
  file.read(reinterpret_cast<char *>(&opt_hdr_size), 2);

  file.seekg(pe_offset + 24);
  uint16_t opt_magic = 0;
  file.read(reinterpret_cast<char *>(&opt_magic), 2);
  if (opt_magic != 0x020B) return false; // Must be PE32+ (64-bit)

  // DataDirectory[0] (Export Directory) is at offset: 24 + 112 = 136 in OptionalHeader64
  file.seekg(pe_offset + 24 + 112);
  uint32_t export_rva = 0;
  uint32_t export_size = 0;
  file.read(reinterpret_cast<char *>(&export_rva), 4);
  file.read(reinterpret_cast<char *>(&export_size), 4);
  if (export_rva == 0 || export_size == 0) return true;

  struct SectionInfo {
    uint32_t va{0};
    uint32_t vsize{0};
    uint32_t raw_ptr{0};
    uint32_t raw_size{0};
  };

  std::vector<SectionInfo> sections;
  file.seekg(pe_offset + 24 + opt_hdr_size);
  for (int i = 0; i < num_sections; ++i) {
    char s_name[8];
    uint32_t s_vsize, s_va, s_raw_size, s_raw_ptr;
    file.read(s_name, 8);
    file.read(reinterpret_cast<char *>(&s_vsize), 4);
    file.read(reinterpret_cast<char *>(&s_va), 4);
    file.read(reinterpret_cast<char *>(&s_raw_size), 4);
    file.read(reinterpret_cast<char *>(&s_raw_ptr), 4);
    file.seekg(static_cast<std::streamoff>(file.tellg()) + 16);

    sections.push_back({s_va, s_vsize, s_raw_ptr, s_raw_size});
  }

  auto rva_to_offset = [&](uint32_t rva) -> uint32_t {
    for (const auto &s : sections) {
      uint32_t span = std::max(s.vsize, s.raw_size);
      if (rva >= s.va && rva < s.va + span) {
        return s.raw_ptr + (rva - s.va);
      }
    }
    return 0;
  };

  uint32_t export_file_offset = rva_to_offset(export_rva);
  if (export_file_offset == 0) return false;

  // IMAGE_EXPORT_DIRECTORY
  // NumberOfNames is at +24, AddressOfNames is at +32
  file.seekg(export_file_offset + 24);
  uint32_t num_names = 0;
  file.read(reinterpret_cast<char *>(&num_names), 4);

  file.seekg(export_file_offset + 32);
  uint32_t names_rva = 0;
  file.read(reinterpret_cast<char *>(&names_rva), 4);

  uint32_t names_file_offset = rva_to_offset(names_rva);
  if (names_file_offset == 0) return false;

  file.seekg(names_file_offset);
  std::vector<uint32_t> name_rvas(num_names);
  file.read(reinterpret_cast<char *>(name_rvas.data()), num_names * 4);

  for (uint32_t nrva : name_rvas) {
    uint32_t str_offset = rva_to_offset(nrva);
    if (str_offset == 0) continue;
    file.seekg(str_offset);
    std::string name;
    char c;
    while (file.get(c) && c != '\0') {
      name += c;
    }
    if (!name.empty()) {
      exports.insert(name);
    }
  }

  return true;
}

bool InternalPELinker::read_coff_archive(const std::string &lib_path,
                                         std::unordered_map<std::string, std::string> &import_symbols,
                                         std::string &error_msg) {
  std::ifstream file(lib_path, std::ios::binary);
  if (!file.is_open()) {
    error_msg = "Cannot open archive file: " + lib_path;
    return false;
  }

  // Check magic "!<arch>\n"
  char magic[8];
  file.read(magic, 8);
  if (std::memcmp(magic, "!<arch>\n", 8) != 0) {
    error_msg = "Not a valid COFF archive: " + lib_path;
    return false;
  }

  file.seekg(0, std::ios::end);
  size_t file_len = static_cast<size_t>(file.tellg());

  std::vector<uint8_t> buffer(file_len);
  file.seekg(0);
  file.read(reinterpret_cast<char *>(buffer.data()), file_len);

  auto check_short_import = [&](size_t data_off) {
    if (data_off + 20 > file_len) return;
    const uint8_t *mem_data = buffer.data() + data_off;
    uint16_t sig1 = *reinterpret_cast<const uint16_t *>(mem_data);
    uint16_t sig2 = *reinterpret_cast<const uint16_t *>(mem_data + 2);
    if (sig1 == 0 && sig2 == 0xFFFF) {
      size_t p = data_off + 20;
      std::string s_name;
      while (p < file_len && buffer[p] != '\0') s_name += static_cast<char>(buffer[p++]);
      p++;
      std::string dll_name;
      while (p < file_len && buffer[p] != '\0') dll_name += static_cast<char>(buffer[p++]);

      if (!s_name.empty() && !dll_name.empty()) {
        import_symbols[s_name] = dll_name;
        if (s_name.rfind("__imp_", 0) == 0) {
          import_symbols[s_name.substr(6)] = dll_name;
        }
      }
    }
  };

  size_t offset = 8;
  while (offset + 60 <= file_len) {
    const char *hdr = reinterpret_cast<const char *>(buffer.data() + offset);
    std::string name_str(hdr, 16);
    std::string size_str(hdr + 48, 10);
    size_t member_size = 0;
    try {
      member_size = std::stoul(size_str);
    } catch (...) {
      break;
    }

    size_t data_offset = offset + 60;
    if (data_offset + member_size > file_len) break;

    // Check member for Short Import Header directly
    check_short_import(data_offset);

    // If First Linker Member ("/" or "/ ")
    if (name_str.rfind("/ ", 0) == 0 || name_str.rfind("/", 0) == 0) {
      if (member_size >= 4) {
        uint32_t num_syms = read_u32_be(buffer.data() + data_offset);
        if (member_size >= 4 + num_syms * 4) {
          for (uint32_t i = 0; i < num_syms; ++i) {
            uint32_t mem_off = read_u32_be(buffer.data() + data_offset + 4 + i * 4);
            if (mem_off + 60 <= file_len) {
              check_short_import(mem_off + 60);
            }
          }
        }
      }
    }

    offset = data_offset + align_up(static_cast<uint32_t>(member_size), 2);
  }

  return true;
}

TargetFileType InternalPELinker::identify_target_type(const std::vector<AssembledModule> &modules) {
  for (const auto &m : modules) {
    if (m.symbol_offsets.find("main") != m.symbol_offsets.end()) {
      return TargetFileType::Executable;
    }
  }
  return TargetFileType::ObjectFile;
}

bool InternalPELinker::link(const std::vector<AssembledModule> &modules,
                            const std::string &output_filepath,
                            std::string &error_msg,
                            const LinkerConfig &config,
                            TargetFileType target_file_type) {
  if (modules.empty()) {
    error_msg = "Linker error: no modules provided to link";
    return false;
  }

  TargetFileType resolved_type = target_file_type;
  if (resolved_type == TargetFileType::Auto) {
    resolved_type = identify_target_type(modules);
  }

  if (resolved_type == TargetFileType::ObjectFile) {
    error_msg = "Target file type identified as ObjectFile (no 'main' entry point function found). Cannot produce standalone executable.";
    return false;
  }

  AssembledModule module;
  struct ModulePlacement {
    size_t text_offset{0};
    size_t data_offset{0};
  };
  std::vector<ModulePlacement> placements(modules.size());

  for (size_t i = 0; i < modules.size(); ++i) {
    const auto &m = modules[i];

    // Align text section to 16 bytes
    while (module.text_bytes.size() % 16 != 0) {
      module.text_bytes.push_back(0x90); // nop
    }
    placements[i].text_offset = module.text_bytes.size();

    // Align data section to 16 bytes
    while (module.data_bytes.size() % 16 != 0) {
      module.data_bytes.push_back(0x00);
    }
    placements[i].data_offset = module.data_bytes.size();

    // Append bytes
    module.text_bytes.insert(module.text_bytes.end(), m.text_bytes.begin(), m.text_bytes.end());
    module.data_bytes.insert(module.data_bytes.end(), m.data_bytes.begin(), m.data_bytes.end());

    auto is_global = [&](const std::string &name) -> bool {
      if (m.local_symbols.count(name)) return false;
      if (m.global_symbols.count(name)) return true;
      if (!m.global_symbols.empty()) return false;
      return !name.empty() && name[0] != '.';
    };

    // Register text symbols
    for (const auto &pair : m.symbol_offsets) {
      const std::string &name = pair.first;
      uint32_t off = pair.second;
      if (is_global(name)) {
        if (module.symbol_offsets.find(name) != module.symbol_offsets.end()) {
          error_msg = "Linker error: multiple definition of symbol '" + name + "'";
          return false;
        }
        module.symbol_offsets[name] = static_cast<uint32_t>(placements[i].text_offset + off);
        module.global_symbols.insert(name);
      } else {
        std::string scoped = name + "@mod" + std::to_string(i);
        module.symbol_offsets[scoped] = static_cast<uint32_t>(placements[i].text_offset + off);
      }
    }

    // Register data symbols
    for (const auto &pair : m.data_offsets) {
      const std::string &name = pair.first;
      uint32_t off = pair.second;
      if (is_global(name)) {
        if (module.data_offsets.find(name) != module.data_offsets.end()) {
          error_msg = "Linker error: multiple definition of data symbol '" + name + "'";
          return false;
        }
        module.data_offsets[name] = static_cast<uint32_t>(placements[i].data_offset + off);
      } else {
        std::string scoped = name + "@mod" + std::to_string(i);
        module.data_offsets[scoped] = static_cast<uint32_t>(placements[i].data_offset + off);
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
      module.relocations.push_back(r);
    }

    // Merge source_locs and file_table
    for (const auto &pair : m.file_table) {
      module.file_table[pair.first] = pair.second;
    }
    for (const auto &loc : m.source_locs) {
      SourceLoc mapped = loc;
      mapped.text_offset += placements[i].text_offset;
      module.source_locs.push_back(mapped);
    }
  }

  // Identify external symbols
  for (const auto &m : modules) {
    for (const auto &ext : m.external_symbols) {
      if (module.symbol_offsets.find(ext) == module.symbol_offsets.end() &&
          module.data_offsets.find(ext) == module.data_offsets.end()) {
        module.external_symbols.insert(ext);
      }
    }
  }
  for (const auto &reloc : module.relocations) {
    if (module.symbol_offsets.find(reloc.symbol) == module.symbol_offsets.end() &&
        module.data_offsets.find(reloc.symbol) == module.data_offsets.end()) {
      module.external_symbols.insert(reloc.symbol);
    }
  }

  // Ensure main exists
  if (module.symbol_offsets.find("main") == module.symbol_offsets.end()) {
    error_msg = "Linker error: undefined symbol 'main'";
    return false;
  }

  // Always import ExitProcess for clean exit
  module.external_symbols.insert("ExitProcess");

  // Map of external symbol -> target DLL
  std::unordered_map<std::string, std::string> sym_to_dll;
  sym_to_dll["ExitProcess"] = "KERNEL32.dll";

  // 1. Explicit imports (--import dll:func)
  for (const auto &pair : config.explicit_imports) {
    sym_to_dll[pair.first] = pair.second;
  }

  // 2. Build library search directories
  std::vector<std::string> search_dirs = config.lib_dirs;
  search_dirs.push_back(".");
  search_dirs.push_back("C:\\Windows\\System32");

  // 3. Process COFF .lib archives from config.lib_files
  for (const auto &lib_file : config.lib_files) {
    std::string err;
    std::unordered_map<std::string, std::string> archive_imports;
    if (read_coff_archive(lib_file, archive_imports, err)) {
      for (const auto &pair : archive_imports) {
        if (module.external_symbols.count(pair.first)) {
          sym_to_dll[pair.first] = pair.second;
        }
      }
    }
  }

  // Process -l<name> archives (looking for <name>.lib or lib<name>.a)
  for (const auto &lib_name : config.lib_names) {
    for (const auto &dir : search_dirs) {
      std::filesystem::path p1 = std::filesystem::path(dir) / (lib_name + ".lib");
      std::filesystem::path p2 = std::filesystem::path(dir) / ("lib" + lib_name + ".a");
      std::string resolved;
      if (std::filesystem::exists(p1)) resolved = p1.string();
      else if (std::filesystem::exists(p2)) resolved = p2.string();

      if (!resolved.empty()) {
        std::string err;
        std::unordered_map<std::string, std::string> archive_imports;
        if (read_coff_archive(resolved, archive_imports, err)) {
          for (const auto &pair : archive_imports) {
            if (module.external_symbols.count(pair.first)) {
              sym_to_dll[pair.first] = pair.second;
            }
          }
        }
        break;
      }
    }
  }

  // 4. Process Direct DLLs (config.explicit_dlls and -l<name> mapped to <name>.dll)
  std::vector<std::string> dlls_to_check = config.explicit_dlls;
  for (const auto &name : config.lib_names) {
    dlls_to_check.push_back(name + ".dll");
  }

  for (const auto &dll_name : dlls_to_check) {
    std::string resolved_dll;
    for (const auto &dir : search_dirs) {
      std::filesystem::path p = std::filesystem::path(dir) / dll_name;
      if (std::filesystem::exists(p)) {
        resolved_dll = p.string();
        break;
      }
    }

    if (!resolved_dll.empty()) {
      std::set<std::string> exports;
      if (read_dll_exports(resolved_dll, exports)) {
        std::string base_dll = std::filesystem::path(resolved_dll).filename().string();
        for (const auto &exp : exports) {
          if (module.external_symbols.count(exp) && sym_to_dll.find(exp) == sym_to_dll.end()) {
            sym_to_dll[exp] = base_dll;
          }
        }
      }
    }
  }

  // 5. Default fallback for any remaining symbol: msvcrt.dll (standard CRT functions)
  for (const auto &sym : module.external_symbols) {
    if (sym_to_dll.find(sym) == sym_to_dll.end()) {
      sym_to_dll[sym] = "msvcrt.dll";
    }
  }

  // Structure to track import thunks in .text and IAT offsets in .rdata
  struct ImportInfo {
    std::string name;
    size_t thunk_offset{0};
    uint32_t iat_rdata_offset{0};
  };

  struct DllGroup {
    std::string dll_name;
    std::vector<ImportInfo> symbols;
    uint32_t name_rdata_offset{0};
    std::vector<uint32_t> hint_rvas;
    uint32_t ilt_rdata_offset{0};
    uint32_t iat_rdata_offset{0};
  };

  std::map<std::string, DllGroup> groups_map;
  for (const auto &sym : module.external_symbols) {
    std::string target_dll = sym_to_dll[sym];
    groups_map[target_dll].dll_name = target_dll;
    groups_map[target_dll].symbols.push_back({sym, 0, 0});
  }

  std::vector<DllGroup> dll_groups;
  for (auto &pair : groups_map) {
    dll_groups.push_back(std::move(pair.second));
  }

  // 1. Emit startup code at entry: mainCRTStartup
  size_t entry_offset = module.text_bytes.size();
  module.symbol_offsets["mainCRTStartup"] = static_cast<uint32_t>(entry_offset);

  // subq $40, %rsp
  module.text_bytes.push_back(0x48);
  module.text_bytes.push_back(0x83);
  module.text_bytes.push_back(0xEC);
  module.text_bytes.push_back(0x28);

  // call main
  module.text_bytes.push_back(0xE8);
  module.relocations.push_back({module.text_bytes.size(), "main", RelocType::Rel32_Code});
  for (int i = 0; i < 4; ++i) module.text_bytes.push_back(0);

  // movq %rax, %rcx (exit code from main is arg 0 to ExitProcess)
  module.text_bytes.push_back(0x48);
  module.text_bytes.push_back(0x89);
  module.text_bytes.push_back(0xC1);

  // call ExitProcess
  module.text_bytes.push_back(0xE8);
  module.relocations.push_back({module.text_bytes.size(), "ExitProcess", RelocType::Rel32_Code});
  for (int i = 0; i < 4; ++i) module.text_bytes.push_back(0);

  // ret
  module.text_bytes.push_back(0xC3);

  // 2. Emit import thunks in .text for every external symbol across all DLLs
  // Each thunk: jmp *IAT_entry(%rip) (0xFF 0x25 <disp32>)
  for (auto &group : dll_groups) {
    for (auto &imp : group.symbols) {
      imp.thunk_offset = module.text_bytes.size();
      module.symbol_offsets[imp.name] = static_cast<uint32_t>(imp.thunk_offset);
      module.text_bytes.push_back(0xFF);
      module.text_bytes.push_back(0x25);
      for (int i = 0; i < 4; ++i) module.text_bytes.push_back(0); // placeholder disp32
    }
  }

  // 3. Layout .rdata section
  uint32_t text_rva = 0x1000;
  uint32_t text_vsize = static_cast<uint32_t>(module.text_bytes.size());
  uint32_t rdata_rva = text_rva + align_up(text_vsize, 0x1000);

  std::vector<uint8_t> rdata = module.data_bytes;
  while (rdata.size() % 16 != 0) rdata.push_back(0); // align

  // Store DLL names in .rdata
  for (auto &group : dll_groups) {
    while (rdata.size() % 16 != 0) rdata.push_back(0);
    group.name_rdata_offset = static_cast<uint32_t>(rdata.size());
    for (char c : group.dll_name) rdata.push_back(static_cast<uint8_t>(c));
    rdata.push_back(0);
  }

  // Store Hint/Name entries for each symbol
  for (auto &group : dll_groups) {
    group.hint_rvas.clear();
    for (const auto &imp : group.symbols) {
      while (rdata.size() % 2 != 0) rdata.push_back(0);
      group.hint_rvas.push_back(rdata_rva + static_cast<uint32_t>(rdata.size()));
      rdata.push_back(0); rdata.push_back(0); // hint = 0
      for (char c : imp.name) rdata.push_back(static_cast<uint8_t>(c));
      rdata.push_back(0);
    }
  }

  // ILT (Import Lookup Tables)
  for (auto &group : dll_groups) {
    while (rdata.size() % 8 != 0) rdata.push_back(0);
    group.ilt_rdata_offset = static_cast<uint32_t>(rdata.size());
    for (uint32_t hrva : group.hint_rvas) {
      uint64_t val = hrva;
      for (int b = 0; b < 8; ++b) rdata.push_back(static_cast<uint8_t>((val >> (b * 8)) & 0xFF));
    }
    for (int b = 0; b < 8; ++b) rdata.push_back(0); // null terminator
  }

  // IAT (Import Address Tables)
  while (rdata.size() % 8 != 0) rdata.push_back(0);
  uint32_t iat_start_offset = static_cast<uint32_t>(rdata.size());

  for (auto &group : dll_groups) {
    while (rdata.size() % 8 != 0) rdata.push_back(0);
    group.iat_rdata_offset = static_cast<uint32_t>(rdata.size());
    for (size_t i = 0; i < group.symbols.size(); ++i) {
      group.symbols[i].iat_rdata_offset = static_cast<uint32_t>(rdata.size());
      uint64_t val = group.hint_rvas[i];
      for (int b = 0; b < 8; ++b) rdata.push_back(static_cast<uint8_t>((val >> (b * 8)) & 0xFF));
    }
    for (int b = 0; b < 8; ++b) rdata.push_back(0); // null terminator
  }
  uint32_t iat_size = static_cast<uint32_t>(rdata.size() - iat_start_offset);

  // Import Directory Table (array of IMAGE_IMPORT_DESCRIPTOR)
  while (rdata.size() % 16 != 0) rdata.push_back(0);
  uint32_t import_dir_offset = static_cast<uint32_t>(rdata.size());

  std::vector<ImportDescriptor> descriptors(dll_groups.size() + 1); // +1 null descriptor
  for (size_t i = 0; i < dll_groups.size(); ++i) {
    descriptors[i].OriginalFirstThunk = rdata_rva + dll_groups[i].ilt_rdata_offset;
    descriptors[i].Name = rdata_rva + dll_groups[i].name_rdata_offset;
    descriptors[i].FirstThunk = rdata_rva + dll_groups[i].iat_rdata_offset;
  }
  uint32_t import_dir_size = static_cast<uint32_t>(descriptors.size() * sizeof(ImportDescriptor));

  for (const auto &desc : descriptors) {
    const uint8_t *p = reinterpret_cast<const uint8_t *>(&desc);
    for (size_t b = 0; b < sizeof(ImportDescriptor); ++b) {
      rdata.push_back(p[b]);
    }
  }

  // 4. Resolve relocations
  for (const auto &reloc : module.relocations) {
    if (reloc.type == RelocType::Rel32_Code) {
      auto it = module.symbol_offsets.find(reloc.symbol);
      if (it == module.symbol_offsets.end()) {
        error_msg = "Linker error: undefined code symbol '" + reloc.symbol + "'";
        return false;
      }
      uint32_t target_rva = text_rva + it->second;
      uint32_t next_rva = text_rva + static_cast<uint32_t>(reloc.text_offset + 4);
      int32_t disp = static_cast<int32_t>(target_rva - next_rva);
      write_int32_le(module.text_bytes, reloc.text_offset, disp);
    } else if (reloc.type == RelocType::Rip_Data) {
      auto it = module.data_offsets.find(reloc.symbol);
      if (it == module.data_offsets.end()) {
        error_msg = "Linker error: unresolved data label '" + reloc.symbol + "'";
        return false;
      }
      uint32_t data_rva = rdata_rva + it->second;
      uint32_t next_rva = text_rva + static_cast<uint32_t>(reloc.text_offset + 4);
      int32_t disp = static_cast<int32_t>(data_rva - next_rva);
      write_int32_le(module.text_bytes, reloc.text_offset, disp);
    }
  }

  // Fix up import thunks (jmp *IAT(%rip))
  for (const auto &group : dll_groups) {
    for (const auto &imp : group.symbols) {
      uint32_t iat_rva = rdata_rva + imp.iat_rdata_offset;
      uint32_t next_rva = text_rva + static_cast<uint32_t>(imp.thunk_offset + 6);
      int32_t disp = static_cast<int32_t>(iat_rva - next_rva);
      write_int32_le(module.text_bytes, imp.thunk_offset + 2, disp);
    }
  }

  // 5. Build PE file structures
  uint32_t text_raw_size = align_up(static_cast<uint32_t>(module.text_bytes.size()), 0x200);
  uint32_t rdata_raw_size = align_up(static_cast<uint32_t>(rdata.size()), 0x200);
  uint32_t headers_size = 0x400;

  module.text_bytes.resize(text_raw_size, 0);
  rdata.resize(rdata_raw_size, 0);

  DosHeader dos;
  FileHeader file_hdr;

  OptionalHeader64 opt_hdr;
  opt_hdr.SizeOfCode = text_raw_size;
  opt_hdr.SizeOfInitializedData = rdata_raw_size;
  opt_hdr.AddressOfEntryPoint = text_rva + static_cast<uint32_t>(entry_offset);
  opt_hdr.BaseOfCode = text_rva;
  opt_hdr.SizeOfHeaders = headers_size;

  // Data Directory: Import Table (Index 1)
  opt_hdr.DataDirectory[1].VirtualAddress = rdata_rva + import_dir_offset;
  opt_hdr.DataDirectory[1].Size = import_dir_size;

  // Data Directory: IAT (Index 12)
  opt_hdr.DataDirectory[12].VirtualAddress = rdata_rva + iat_start_offset;
  opt_hdr.DataDirectory[12].Size = iat_size;

  // Section Headers
  SectionHeader sec_text{};
  std::memcpy(sec_text.Name, ".text", 5);
  sec_text.VirtualSize = static_cast<uint32_t>(module.text_bytes.size());
  sec_text.VirtualAddress = text_rva;
  sec_text.SizeOfRawData = text_raw_size;
  sec_text.PointerToRawData = 0x400;
  sec_text.Characteristics = 0x60000020; // CNT_CODE | MEM_EXECUTE | MEM_READ

  SectionHeader sec_rdata{};
  std::memcpy(sec_rdata.Name, ".rdata", 6);
  sec_rdata.VirtualSize = static_cast<uint32_t>(rdata.size());
  sec_rdata.VirtualAddress = rdata_rva;
  sec_rdata.SizeOfRawData = rdata_raw_size;
  sec_rdata.PointerToRawData = 0x400 + text_raw_size;
  sec_rdata.Characteristics = 0x40000040; // CNT_INITIALIZED_DATA | MEM_READ

  // DWARF Debug Information (.debug_line, .debug_info, .debug_abbrev)
  bool has_debug = !module.source_locs.empty() && !config.strip_debug_info;
  DwarfArtifacts dwarf;
  std::vector<uint8_t> string_table;
  SectionHeader sec_dbg_line{};
  SectionHeader sec_dbg_info{};
  SectionHeader sec_dbg_abbrev{};

  if (has_debug) {
    std::string primary_filename = "main.c";
    for (const auto &pair : module.file_table) {
      if (!pair.second.empty()) {
        primary_filename = pair.second;
        break;
      }
    }

    std::vector<DwarfFunction> dwarf_funcs;
    std::vector<std::pair<uint32_t, std::string>> sorted_syms;
    for (const auto &pair : module.symbol_offsets) {
      if (!pair.first.empty() && pair.first[0] != '.') {
        sorted_syms.push_back({pair.second, pair.first});
      }
    }
    std::sort(sorted_syms.begin(), sorted_syms.end());
    uint32_t full_text_size = static_cast<uint32_t>(module.text_bytes.size());
    for (size_t i = 0; i < sorted_syms.size(); ++i) {
      uint32_t cur_off = sorted_syms[i].first;
      uint32_t next_off = (i + 1 < sorted_syms.size()) ? sorted_syms[i + 1].first : full_text_size;
      uint32_t fsize = (next_off > cur_off) ? (next_off - cur_off) : 16;
      dwarf_funcs.push_back({sorted_syms[i].second, cur_off, fsize});
    }

    uint64_t full_text_vma = opt_hdr.ImageBase + text_rva;
    dwarf = DwarfEmitter::emit(primary_filename, module.file_table, module.source_locs,
                               full_text_vma, full_text_size, dwarf_funcs);

    uint32_t dbg_line_virt_size = static_cast<uint32_t>(dwarf.debug_line.size());
    uint32_t dbg_info_virt_size = static_cast<uint32_t>(dwarf.debug_info.size());
    uint32_t dbg_abbrev_virt_size = static_cast<uint32_t>(dwarf.debug_abbrev.size());

    uint32_t dbg_line_raw_size = align_up(dbg_line_virt_size, 0x200);
    uint32_t dbg_info_raw_size = align_up(dbg_info_virt_size, 0x200);
    uint32_t dbg_abbrev_raw_size = align_up(dbg_abbrev_virt_size, 0x200);
    dwarf.debug_line.resize(dbg_line_raw_size, 0);
    dwarf.debug_info.resize(dbg_info_raw_size, 0);
    dwarf.debug_abbrev.resize(dbg_abbrev_raw_size, 0);

    uint32_t dbg_line_rva = rdata_rva + align_up(static_cast<uint32_t>(rdata.size()), 0x1000);
    uint32_t dbg_info_rva = dbg_line_rva + align_up(static_cast<uint32_t>(dwarf.debug_line.size()), 0x1000);
    uint32_t dbg_abbrev_rva = dbg_info_rva + align_up(static_cast<uint32_t>(dwarf.debug_info.size()), 0x1000);

    uint32_t dbg_line_file_off = 0x400 + text_raw_size + rdata_raw_size;
    uint32_t dbg_info_file_off = dbg_line_file_off + dbg_line_raw_size;
    uint32_t dbg_abbrev_file_off = dbg_info_file_off + dbg_info_raw_size;

    // Build COFF Symbol Table and String Table
#pragma pack(push, 1)
    struct CoffSymbolRecord {
      union {
        char ShortName[8];
        struct {
          uint32_t Zeroes;
          uint32_t Offset;
        };
      } Name;
      uint32_t Value;
      int16_t SectionNumber;
      uint16_t Type;
      uint8_t StorageClass;
      uint8_t NumberOfAuxSymbols;
    };
#pragma pack(pop)

    std::vector<CoffSymbolRecord> coff_symbols;
    string_table.assign(4, 0);

    for (const auto &fn : dwarf_funcs) {
      CoffSymbolRecord rec{};
      rec.Value = fn.offset;
      rec.SectionNumber = 1; // .text is section 1
      rec.Type = 0x20;       // DT_FCN = function
      rec.StorageClass = 2;  // IMAGE_SYM_CLASS_EXTERNAL
      rec.NumberOfAuxSymbols = 0;
      if (fn.name.size() <= 8) {
        std::memcpy(rec.Name.ShortName, fn.name.data(), fn.name.size());
      } else {
        rec.Name.Zeroes = 0;
        rec.Name.Offset = static_cast<uint32_t>(string_table.size());
        for (char c : fn.name) string_table.push_back(static_cast<uint8_t>(c));
        string_table.push_back(0);
      }
      coff_symbols.push_back(rec);
    }

    uint32_t str_off_line = static_cast<uint32_t>(string_table.size());
    const char s_line[] = ".debug_line";
    string_table.insert(string_table.end(), s_line, s_line + sizeof(s_line));

    uint32_t str_off_info = static_cast<uint32_t>(string_table.size());
    const char s_info[] = ".debug_info";
    string_table.insert(string_table.end(), s_info, s_info + sizeof(s_info));

    uint32_t str_off_abbrev = static_cast<uint32_t>(string_table.size());
    const char s_abbrev[] = ".debug_abbrev";
    string_table.insert(string_table.end(), s_abbrev, s_abbrev + sizeof(s_abbrev));

    uint32_t str_table_len = static_cast<uint32_t>(string_table.size());
    string_table[0] = static_cast<uint8_t>(str_table_len & 0xFF);
    string_table[1] = static_cast<uint8_t>((str_table_len >> 8) & 0xFF);
    string_table[2] = static_cast<uint8_t>((str_table_len >> 16) & 0xFF);
    string_table[3] = static_cast<uint8_t>((str_table_len >> 24) & 0xFF);

    file_hdr.PointerToSymbolTable = dbg_abbrev_file_off + dbg_abbrev_raw_size;
    file_hdr.NumberOfSymbols = static_cast<uint32_t>(coff_symbols.size());
    file_hdr.NumberOfSections = 5;

    std::string line_sec_name = "/" + std::to_string(str_off_line);
    std::string info_sec_name = "/" + std::to_string(str_off_info);
    std::string abbrev_sec_name = "/" + std::to_string(str_off_abbrev);

    std::memcpy(sec_dbg_line.Name, line_sec_name.c_str(), line_sec_name.size());
    sec_dbg_line.VirtualSize = dbg_line_virt_size;
    sec_dbg_line.VirtualAddress = dbg_line_rva;
    sec_dbg_line.SizeOfRawData = dbg_line_raw_size;
    sec_dbg_line.PointerToRawData = dbg_line_file_off;
    sec_dbg_line.Characteristics = 0x42000040; // MEM_READ | MEM_DISCARDABLE | CNT_INITIALIZED_DATA

    std::memcpy(sec_dbg_info.Name, info_sec_name.c_str(), info_sec_name.size());
    sec_dbg_info.VirtualSize = dbg_info_virt_size;
    sec_dbg_info.VirtualAddress = dbg_info_rva;
    sec_dbg_info.SizeOfRawData = dbg_info_raw_size;
    sec_dbg_info.PointerToRawData = dbg_info_file_off;
    sec_dbg_info.Characteristics = 0x42000040;

    std::memcpy(sec_dbg_abbrev.Name, abbrev_sec_name.c_str(), abbrev_sec_name.size());
    sec_dbg_abbrev.VirtualSize = dbg_abbrev_virt_size;
    sec_dbg_abbrev.VirtualAddress = dbg_abbrev_rva;
    sec_dbg_abbrev.SizeOfRawData = dbg_abbrev_raw_size;
    sec_dbg_abbrev.PointerToRawData = dbg_abbrev_file_off;
    sec_dbg_abbrev.Characteristics = 0x42000040;

    opt_hdr.SizeOfImage = dbg_abbrev_rva + align_up(static_cast<uint32_t>(dwarf.debug_abbrev.size()), 0x1000);
  } else {
    file_hdr.NumberOfSections = 2;
    file_hdr.PointerToSymbolTable = 0;
    file_hdr.NumberOfSymbols = 0;
    opt_hdr.SizeOfImage = rdata_rva + align_up(static_cast<uint32_t>(rdata.size()), 0x1000);
  }

  // Write out binary
  std::ofstream out(output_filepath, std::ios::binary);
  if (!out.is_open()) {
    error_msg = "Failed to open output file for writing: " + output_filepath;
    return false;
  }

  std::vector<uint8_t> header_bytes(headers_size, 0);

  // DOS Header at 0x00
  std::memcpy(header_bytes.data(), &dos, sizeof(dos));

  // PE Header at 0x80
  uint32_t pe_sig = 0x00004550; // "PE\0\0"
  std::memcpy(header_bytes.data() + 0x80, &pe_sig, 4);
  std::memcpy(header_bytes.data() + 0x84, &file_hdr, sizeof(file_hdr));
  std::memcpy(header_bytes.data() + 0x98, &opt_hdr, sizeof(opt_hdr));

  // Section headers at 0x98 + sizeof(OptionalHeader64)
  size_t sec_offset = 0x98 + sizeof(OptionalHeader64);
  std::memcpy(header_bytes.data() + sec_offset, &sec_text, sizeof(sec_text));
  std::memcpy(header_bytes.data() + sec_offset + sizeof(sec_text), &sec_rdata, sizeof(sec_rdata));
  if (has_debug) {
    std::memcpy(header_bytes.data() + sec_offset + 2 * sizeof(SectionHeader), &sec_dbg_line, sizeof(sec_dbg_line));
    std::memcpy(header_bytes.data() + sec_offset + 3 * sizeof(SectionHeader), &sec_dbg_info, sizeof(sec_dbg_info));
    std::memcpy(header_bytes.data() + sec_offset + 4 * sizeof(SectionHeader), &sec_dbg_abbrev, sizeof(sec_dbg_abbrev));
  }

  out.write(reinterpret_cast<const char *>(header_bytes.data()), header_bytes.size());
  out.write(reinterpret_cast<const char *>(module.text_bytes.data()), module.text_bytes.size());
  out.write(reinterpret_cast<const char *>(rdata.data()), rdata.size());
  if (has_debug) {
    out.write(reinterpret_cast<const char *>(dwarf.debug_line.data()), dwarf.debug_line.size());
    out.write(reinterpret_cast<const char *>(dwarf.debug_info.data()), dwarf.debug_info.size());
    out.write(reinterpret_cast<const char *>(dwarf.debug_abbrev.data()), dwarf.debug_abbrev.size());
    if (file_hdr.NumberOfSymbols > 0) {
      // Reconstruct and write symbol records
      #pragma pack(push, 1)
      struct CoffSymbolRecord {
        union {
          char ShortName[8];
          struct {
            uint32_t Zeroes;
            uint32_t Offset;
          };
        } Name;
        uint32_t Value;
        int16_t SectionNumber;
        uint16_t Type;
        uint8_t StorageClass;
        uint8_t NumberOfAuxSymbols;
      };
      #pragma pack(pop)
      std::vector<CoffSymbolRecord> coff_symbols;
      std::vector<std::pair<uint32_t, std::string>> sorted_syms;
      for (const auto &pair : module.symbol_offsets) {
        if (!pair.first.empty() && pair.first[0] != '.') {
          sorted_syms.push_back({pair.second, pair.first});
        }
      }
      std::sort(sorted_syms.begin(), sorted_syms.end());
      uint32_t str_pos = 4;
      for (const auto &p : sorted_syms) {
        CoffSymbolRecord rec{};
        rec.Value = p.first;
        rec.SectionNumber = 1;
        rec.Type = 0x20;
        rec.StorageClass = 2;
        rec.NumberOfAuxSymbols = 0;
        if (p.second.size() <= 8) {
          std::memcpy(rec.Name.ShortName, p.second.data(), p.second.size());
        } else {
          rec.Name.Zeroes = 0;
          rec.Name.Offset = str_pos;
          str_pos += static_cast<uint32_t>(p.second.size() + 1);
        }
        coff_symbols.push_back(rec);
      }
      out.write(reinterpret_cast<const char *>(coff_symbols.data()), coff_symbols.size() * sizeof(CoffSymbolRecord));
    }
    out.write(reinterpret_cast<const char *>(string_table.data()), string_table.size());
  }

  if (!out.good()) {
    error_msg = "Error writing binary executable data to: " + output_filepath;
    return false;
  }

  return true;
}
