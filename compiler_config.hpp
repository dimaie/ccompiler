#ifndef COMPILER_CONFIG_HPP
#define COMPILER_CONFIG_HPP

#include "codegen.hpp"
#include "internal_pe_linker.hpp"
#include <iostream>
#include <string>
#include <vector>

enum class StopPhase {
  FullBuild,
  PreprocessOnly,
  AssembleOnly
};

struct GeneralConfig {
  std::string input_filename;
  std::string output_filename;
  std::string target{"x86_64"};
  StopPhase stop_phase{StopPhase::FullBuild};
  bool show_ast{false};
  bool verbose{false};
};

struct PreprocessorConfig {
  std::vector<std::string> include_dirs;
  std::vector<std::string> defined_macros;
};

struct CodeGenConfig {
  LocalVarStorage local_var_storage{LocalVarStorage::Auto};
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
