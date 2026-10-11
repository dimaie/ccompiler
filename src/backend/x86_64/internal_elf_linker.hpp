#ifndef INTERNAL_ELF_LINKER_HPP
#define INTERNAL_ELF_LINKER_HPP

#include "internal_assembler.hpp"
#include "internal_pe_linker.hpp" // for TargetFileType, LinkerConfig
#include <string>
#include <vector>

/**
 * @brief Embedded 64-bit ELF Linker for Linux (x86_64).
 * Produces standalone statically-executable ELF64 binaries with built-in
 * SysV ABI runtime (_start, puts, putchar, printf, exit, write) requiring
 * zero external dependencies or dynamic linkers.
 */
class InternalElfLinker {
public:
  // Identify the target file type that can be produced based on symbols
  static TargetFileType identify_target_type(const std::vector<AssembledModule> &modules);

  // Link multiple assembled modules into a Linux ELF64 executable
  static bool link(const std::vector<AssembledModule> &modules,
                   const std::string &output_filepath,
                   std::string &error_msg,
                   const LinkerConfig &config = {},
                   TargetFileType target_file_type = TargetFileType::Auto);

  // Single module overload
  static bool link(AssembledModule module,
                   const std::string &output_filepath,
                   std::string &error_msg,
                   const LinkerConfig &config = {}) {
    std::vector<AssembledModule> modules;
    modules.push_back(std::move(module));
    return link(modules, output_filepath, error_msg, config, TargetFileType::Auto);
  }
};

#endif // INTERNAL_ELF_LINKER_HPP
