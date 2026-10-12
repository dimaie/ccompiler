#ifndef COMPILER_CONFIG_HPP
#define COMPILER_CONFIG_HPP

#include "codegen.hpp"
#include "x86_64/internal_pe_linker.hpp"
#include <iostream>
#include <string>
#include <vector>

enum class StopPhase {
  FullBuild,
  PreprocessOnly,
  IROnly,
  AssembleOnly,
  CompileOnly
};

struct GeneralConfig {
  std::vector<std::string> input_filenames;
  std::string input_filename;
  std::string output_filename;
  std::string target{"x86_64"};
  TargetSpec target_spec;
  StopPhase stop_phase{StopPhase::FullBuild};
  TargetFileType target_file_type{TargetFileType::Auto};
  bool show_ast{false};
  bool show_ir{false};
  bool verbose{false};
  bool quiet{false};
  bool emit_debug_info{true};
};

struct PreprocessorConfig {
  std::vector<std::string> include_dirs;
  std::vector<std::string> defined_macros;
};

struct CodeGenConfig {
  LocalVarStorage local_var_storage{LocalVarStorage::Auto};
  bool emit_debug_info{true};
};

class CompilerConfig {
public:
  GeneralConfig general;
  PreprocessorConfig preprocessor;
  CodeGenConfig codegen;
  LinkerConfig linker;

  CodeGenOptions get_codegen_options() const {
    CodeGenOptions opts;
    opts.local_var_storage = codegen.local_var_storage;
    opts.emit_debug_info = codegen.emit_debug_info;
    opts.source_filename = general.input_filename;
    return opts;
  }

  void print(std::ostream &out = std::cout) const;
};

class CommandLineParser {
public:
  CommandLineParser() = default;

  bool parse(int argc, char *argv[], CompilerConfig &config,
             std::string &error_message, bool &help_requested);

  bool parse(const std::vector<std::string> &args, CompilerConfig &config,
             std::string &error_message, bool &help_requested);

  static void print_usage(const char *prog_name, std::ostream &out = std::cerr);
};

#endif // COMPILER_CONFIG_HPP
