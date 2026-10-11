#ifndef INTERNAL_PE_LINKER_HPP
#define INTERNAL_PE_LINKER_HPP

#include "internal_assembler.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <set>

enum class TargetFileType {
  Auto,
  Executable,
  ObjectFile,
  SharedLib
};

inline const char *target_file_type_str(TargetFileType t) {
  switch (t) {
  case TargetFileType::Auto: return "auto";
  case TargetFileType::Executable: return "executable";
  case TargetFileType::ObjectFile: return "object";
  case TargetFileType::SharedLib: return "shared_lib";
  default: return "unknown";
  }
}

struct LinkerConfig {
  std::vector<std::string> lib_dirs;      // -L<dir>
  std::vector<std::string> lib_files;     // path/to/lib.lib or lib.a
  std::vector<std::string> lib_names;     // -luser32 -> "user32"
  std::vector<std::string> explicit_dlls; // user32.dll
  std::unordered_map<std::string, std::string> explicit_imports; // symbol -> dll
  bool strip_debug_info{false};
};

class InternalPELinker {
public:
  // Identify the target file type that can be produced based on symbols (e.g. presence of 'main')
  static TargetFileType identify_target_type(const std::vector<AssembledModule> &modules);

  // Link multiple assembled modules into a target file
  static bool link(const std::vector<AssembledModule> &modules,
                   const std::string &output_filepath,
                   std::string &error_msg,
                   const LinkerConfig &config = {},
                   TargetFileType target_file_type = TargetFileType::Auto);

  // Backward compatibility overload for single module
  static bool link(AssembledModule module,
                   const std::string &output_filepath,
                   std::string &error_msg,
                   const LinkerConfig &config = {}) {
    std::vector<AssembledModule> modules;
    modules.push_back(std::move(module));
    return link(modules, output_filepath, error_msg, config, TargetFileType::Auto);
  }

  // Utility to read exported function names from any PE DLL file
  static bool read_dll_exports(const std::string &dll_path, std::set<std::string> &exports);

  // Utility to read COFF archive (.lib / .a) and resolve symbols
  static bool read_coff_archive(const std::string &lib_path,
                                std::unordered_map<std::string, std::string> &import_symbols,
                                std::string &error_msg);
};

#endif // INTERNAL_PE_LINKER_HPP
