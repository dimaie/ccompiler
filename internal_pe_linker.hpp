#ifndef INTERNAL_PE_LINKER_HPP
#define INTERNAL_PE_LINKER_HPP

#include "internal_assembler.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <set>

struct LinkerConfig {
  std::vector<std::string> lib_dirs;      // -L<dir>
  std::vector<std::string> lib_files;     // path/to/lib.lib or lib.a
  std::vector<std::string> lib_names;     // -luser32 -> "user32"
  std::vector<std::string> explicit_dlls; // user32.dll
  std::unordered_map<std::string, std::string> explicit_imports; // symbol -> dll
};

class InternalPELinker {
public:
  static bool link(AssembledModule module,
                   const std::string &output_filepath,
                   std::string &error_msg,
                   const LinkerConfig &config = {});

  // Utility to read exported function names from any PE DLL file
  static bool read_dll_exports(const std::string &dll_path, std::set<std::string> &exports);

  // Utility to read COFF archive (.lib / .a) and resolve symbols
  static bool read_coff_archive(const std::string &lib_path,
                                std::unordered_map<std::string, std::string> &import_symbols,
                                std::string &error_msg);
};

#endif // INTERNAL_PE_LINKER_HPP
