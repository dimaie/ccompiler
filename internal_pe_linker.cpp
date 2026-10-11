#include "internal_pe_linker.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
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

} // namespace

bool InternalPELinker::link(AssembledModule module,
                            const std::string &output_filepath,
                            std::string &error_msg) {
  // Ensure main exists
  if (module.symbol_offsets.find("main") == module.symbol_offsets.end()) {
    error_msg = "Linker error: undefined symbol 'main'";
    return false;
  }

  // Always import ExitProcess for clean exit
  module.external_symbols.insert("ExitProcess");

  // Group imported symbols into DLLs:
  // KERNEL32.dll -> ExitProcess
  // msvcrt.dll   -> all other C library symbols (puts, printf, putchar, etc.)
  std::vector<std::string> kernel32_syms;
  std::vector<std::string> msvcrt_syms;

  for (const auto &sym : module.external_symbols) {
    if (sym == "ExitProcess") {
      kernel32_syms.push_back(sym);
    } else {
      msvcrt_syms.push_back(sym);
    }
  }

  // Structure to track import thunks in .text and IAT offsets in .rdata
  struct ImportInfo {
    std::string name;
    size_t thunk_offset{0};
    uint32_t iat_rdata_offset{0};
  };

  std::vector<ImportInfo> k32_imports;
  for (const auto &s : kernel32_syms) k32_imports.push_back({s, 0, 0});
  std::vector<ImportInfo> crt_imports;
  for (const auto &s : msvcrt_syms) crt_imports.push_back({s, 0, 0});

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

  // 2. Emit import thunks in .text for each external symbol
  // Each thunk: jmp *IAT_entry(%rip) (0xFF 0x25 <disp32>)
  for (auto &imp : k32_imports) {
    imp.thunk_offset = module.text_bytes.size();
    module.symbol_offsets[imp.name] = static_cast<uint32_t>(imp.thunk_offset);
    module.text_bytes.push_back(0xFF);
    module.text_bytes.push_back(0x25);
    for (int i = 0; i < 4; ++i) module.text_bytes.push_back(0); // placeholder disp32
  }
  for (auto &imp : crt_imports) {
    imp.thunk_offset = module.text_bytes.size();
    module.symbol_offsets[imp.name] = static_cast<uint32_t>(imp.thunk_offset);
    module.text_bytes.push_back(0xFF);
    module.text_bytes.push_back(0x25);
    for (int i = 0; i < 4; ++i) module.text_bytes.push_back(0); // placeholder disp32
  }

  // 3. Layout .rdata section
  // VirtualAddress of .text is 0x1000
  uint32_t text_rva = 0x1000;
  uint32_t text_vsize = static_cast<uint32_t>(module.text_bytes.size());
  uint32_t rdata_rva = text_rva + align_up(text_vsize, 0x1000);

  std::vector<uint8_t> rdata = module.data_bytes;
  while (rdata.size() % 16 != 0) rdata.push_back(0); // align

  // Store DLL names in .rdata
  uint32_t k32_name_offset = static_cast<uint32_t>(rdata.size());
  const char k32_str[] = "KERNEL32.dll";
  for (char c : k32_str) rdata.push_back(static_cast<uint8_t>(c));

  while (rdata.size() % 16 != 0) rdata.push_back(0);
  uint32_t crt_name_offset = static_cast<uint32_t>(rdata.size());
  const char crt_str[] = "msvcrt.dll";
  for (char c : crt_str) rdata.push_back(static_cast<uint8_t>(c));

  // Store Hint/Name entries: uint16_t hint=0, name string, \0
  std::vector<uint32_t> k32_hint_rvas;
  for (const auto &imp : k32_imports) {
    while (rdata.size() % 2 != 0) rdata.push_back(0);
    k32_hint_rvas.push_back(rdata_rva + static_cast<uint32_t>(rdata.size()));
    rdata.push_back(0); rdata.push_back(0); // hint = 0
    for (char c : imp.name) rdata.push_back(static_cast<uint8_t>(c));
    rdata.push_back(0);
  }

  std::vector<uint32_t> crt_hint_rvas;
  for (const auto &imp : crt_imports) {
    while (rdata.size() % 2 != 0) rdata.push_back(0);
    crt_hint_rvas.push_back(rdata_rva + static_cast<uint32_t>(rdata.size()));
    rdata.push_back(0); rdata.push_back(0); // hint = 0
    for (char c : imp.name) rdata.push_back(static_cast<uint8_t>(c));
    rdata.push_back(0);
  }

  // ILT (Import Lookup Tables)
  while (rdata.size() % 8 != 0) rdata.push_back(0);
  uint32_t k32_ilt_offset = static_cast<uint32_t>(rdata.size());
  for (uint32_t hrva : k32_hint_rvas) {
    uint64_t val = hrva;
    for (int b = 0; b < 8; ++b) rdata.push_back(static_cast<uint8_t>((val >> (b * 8)) & 0xFF));
  }
  for (int b = 0; b < 8; ++b) rdata.push_back(0); // null terminator

  uint32_t crt_ilt_offset = static_cast<uint32_t>(rdata.size());
  if (!crt_imports.empty()) {
    for (uint32_t hrva : crt_hint_rvas) {
      uint64_t val = hrva;
      for (int b = 0; b < 8; ++b) rdata.push_back(static_cast<uint8_t>((val >> (b * 8)) & 0xFF));
    }
    for (int b = 0; b < 8; ++b) rdata.push_back(0); // null terminator
  }

  // IAT (Import Address Tables)
  while (rdata.size() % 8 != 0) rdata.push_back(0);
  uint32_t iat_start_offset = static_cast<uint32_t>(rdata.size());

  uint32_t k32_iat_offset = static_cast<uint32_t>(rdata.size());
  for (size_t i = 0; i < k32_imports.size(); ++i) {
    k32_imports[i].iat_rdata_offset = static_cast<uint32_t>(rdata.size());
    uint64_t val = k32_hint_rvas[i];
    for (int b = 0; b < 8; ++b) rdata.push_back(static_cast<uint8_t>((val >> (b * 8)) & 0xFF));
  }
  for (int b = 0; b < 8; ++b) rdata.push_back(0);

  uint32_t crt_iat_offset = static_cast<uint32_t>(rdata.size());
  if (!crt_imports.empty()) {
    for (size_t i = 0; i < crt_imports.size(); ++i) {
      crt_imports[i].iat_rdata_offset = static_cast<uint32_t>(rdata.size());
      uint64_t val = crt_hint_rvas[i];
      for (int b = 0; b < 8; ++b) rdata.push_back(static_cast<uint8_t>((val >> (b * 8)) & 0xFF));
    }
    for (int b = 0; b < 8; ++b) rdata.push_back(0);
  }
  uint32_t iat_size = static_cast<uint32_t>(rdata.size() - iat_start_offset);

  // Import Directory Table (array of IMAGE_IMPORT_DESCRIPTOR)
  while (rdata.size() % 16 != 0) rdata.push_back(0);
  uint32_t import_dir_offset = static_cast<uint32_t>(rdata.size());

  int num_descriptors = crt_imports.empty() ? 2 : 3; // +1 for null descriptor
  std::vector<ImportDescriptor> descriptors(num_descriptors);

  // Descriptor 0: KERNEL32.dll
  descriptors[0].OriginalFirstThunk = rdata_rva + k32_ilt_offset;
  descriptors[0].Name = rdata_rva + k32_name_offset;
  descriptors[0].FirstThunk = rdata_rva + k32_iat_offset;

  // Descriptor 1: msvcrt.dll (if any)
  if (!crt_imports.empty()) {
    descriptors[1].OriginalFirstThunk = rdata_rva + crt_ilt_offset;
    descriptors[1].Name = rdata_rva + crt_name_offset;
    descriptors[1].FirstThunk = rdata_rva + crt_iat_offset;
  }

  for (const auto &d : descriptors) {
    const uint8_t *p = reinterpret_cast<const uint8_t *>(&d);
    for (size_t i = 0; i < sizeof(d); ++i) rdata.push_back(p[i]);
  }
  uint32_t import_dir_size = static_cast<uint32_t>(num_descriptors * sizeof(ImportDescriptor));

  // 4. Resolve relocations
  for (const auto &reloc : module.relocations) {
    if (reloc.type == RelocType::Rel32_Code) {
      auto it = module.symbol_offsets.find(reloc.symbol);
      if (it == module.symbol_offsets.end()) {
        error_msg = "Linker error: unresolved symbol '" + reloc.symbol + "'";
        return false;
      }
      int32_t target_offset = static_cast<int32_t>(it->second);
      int32_t next_ip = static_cast<int32_t>(reloc.text_offset + 4);
      int32_t disp = target_offset - next_ip;
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
  for (const auto &imp : k32_imports) {
    uint32_t iat_rva = rdata_rva + imp.iat_rdata_offset;
    uint32_t next_rva = text_rva + static_cast<uint32_t>(imp.thunk_offset + 6);
    int32_t disp = static_cast<int32_t>(iat_rva - next_rva);
    write_int32_le(module.text_bytes, imp.thunk_offset + 2, disp);
  }
  for (const auto &imp : crt_imports) {
    uint32_t iat_rva = rdata_rva + imp.iat_rdata_offset;
    uint32_t next_rva = text_rva + static_cast<uint32_t>(imp.thunk_offset + 6);
    int32_t disp = static_cast<int32_t>(iat_rva - next_rva);
    write_int32_le(module.text_bytes, imp.thunk_offset + 2, disp);
  }

  // 5. Build PE file structures
  uint32_t text_raw_size = align_up(static_cast<uint32_t>(module.text_bytes.size()), 0x200);
  uint32_t rdata_raw_size = align_up(static_cast<uint32_t>(rdata.size()), 0x200);
  uint32_t headers_size = 0x400;

  module.text_bytes.resize(text_raw_size, 0);
  rdata.resize(rdata_raw_size, 0);

  DosHeader dos;
  FileHeader file_hdr;
  file_hdr.NumberOfSections = 2;

  OptionalHeader64 opt_hdr;
  opt_hdr.SizeOfCode = text_raw_size;
  opt_hdr.SizeOfInitializedData = rdata_raw_size;
  opt_hdr.AddressOfEntryPoint = text_rva + static_cast<uint32_t>(entry_offset);
  opt_hdr.BaseOfCode = text_rva;
  opt_hdr.SizeOfImage = rdata_rva + align_up(static_cast<uint32_t>(rdata.size()), 0x1000);
  opt_hdr.SizeOfHeaders = headers_size;

  // Import Directory data directory (index 1)
  opt_hdr.DataDirectory[1].VirtualAddress = rdata_rva + import_dir_offset;
  opt_hdr.DataDirectory[1].Size = import_dir_size;

  // IAT data directory (index 12)
  opt_hdr.DataDirectory[12].VirtualAddress = rdata_rva + iat_start_offset;
  opt_hdr.DataDirectory[12].Size = iat_size;

  SectionHeader sec_text;
  std::memcpy(sec_text.Name, ".text", 5);
  sec_text.VirtualSize = text_vsize;
  sec_text.VirtualAddress = text_rva;
  sec_text.SizeOfRawData = text_raw_size;
  sec_text.PointerToRawData = headers_size;
  sec_text.Characteristics = 0x60000020; // Code | Executable | Read

  SectionHeader sec_rdata;
  std::memcpy(sec_rdata.Name, ".rdata", 6);
  sec_rdata.VirtualSize = static_cast<uint32_t>(rdata.size());
  sec_rdata.VirtualAddress = rdata_rva;
  sec_rdata.SizeOfRawData = rdata_raw_size;
  sec_rdata.PointerToRawData = headers_size + text_raw_size;
  sec_rdata.Characteristics = 0x40000040; // InitializedData | Read

  // 6. Write executable file
  std::ofstream out(output_filepath, std::ios::binary);
  if (!out.is_open()) {
    error_msg = "Linker error: could not open output file '" + output_filepath + "' for writing.";
    return false;
  }

  out.write(reinterpret_cast<const char *>(&dos), sizeof(dos));

  std::vector<char> stub(0x80 - sizeof(dos), 0);
  out.write(stub.data(), stub.size());

  const char pe_sig[4] = {'P', 'E', 0, 0};
  out.write(pe_sig, 4);

  out.write(reinterpret_cast<const char *>(&file_hdr), sizeof(file_hdr));
  out.write(reinterpret_cast<const char *>(&opt_hdr), sizeof(opt_hdr));

  out.write(reinterpret_cast<const char *>(&sec_text), sizeof(sec_text));
  out.write(reinterpret_cast<const char *>(&sec_rdata), sizeof(sec_rdata));

  std::streampos pos = out.tellp();
  if (static_cast<uint32_t>(pos) < headers_size) {
    std::vector<char> pad(headers_size - static_cast<uint32_t>(pos), 0);
    out.write(pad.data(), pad.size());
  }

  out.write(reinterpret_cast<const char *>(module.text_bytes.data()), module.text_bytes.size());
  out.write(reinterpret_cast<const char *>(rdata.data()), rdata.size());

  out.close();
  return true;
}
