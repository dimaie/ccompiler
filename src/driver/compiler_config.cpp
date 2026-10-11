#include "compiler_config.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>

void CompilerConfig::print(std::ostream &out) const {
  out << "=== Compiler Configuration ===\n";
  out << "[General]\n";
  out << "  Input Files:    ";
  if (general.input_filenames.empty()) {
    out << (general.input_filename.empty() ? "(none)" : general.input_filename) << "\n";
  } else {
    for (size_t i = 0; i < general.input_filenames.size(); ++i) {
      if (i > 0) out << ", ";
      out << general.input_filenames[i];
    }
    out << "\n";
  }
  out << "  Output File:    " << (general.output_filename.empty() ? "(default)" : general.output_filename) << "\n";
  out << "  Target:         " << general.target << "\n";
  out << "  Target Spec:    " << general.target_spec.to_string()
      << " (Arch: " << target_arch_str(general.target_spec.arch)
      << ", OS: " << target_os_str(general.target_spec.os)
      << ", Format: " << target_format_str(general.target_spec.format)
      << ", ABI: " << target_abi_str(general.target_spec.abi) << ")\n";
  out << "  Target Type:    " << target_file_type_str(general.target_file_type) << "\n";
  out << "  Stop Phase:     ";
  switch (general.stop_phase) {
  case StopPhase::FullBuild: out << "FullBuild\n"; break;
  case StopPhase::PreprocessOnly: out << "PreprocessOnly (-E)\n"; break;
  case StopPhase::IROnly: out << "IROnly (--emit-ir)\n"; break;
  case StopPhase::AssembleOnly: out << "AssembleOnly (-S)\n"; break;
  case StopPhase::CompileOnly: out << "CompileOnly (-c)\n"; break;
  }
  out << "  Show AST:       " << (general.show_ast ? "true" : "false") << "\n";
  out << "  Show IR:        " << (general.show_ir ? "true" : "false") << "\n";
  out << "  Verbose:        " << (general.verbose ? "true" : "false") << "\n";

  out << "\n[Preprocessor]\n";
  out << "  Include Dirs:   ";
  if (preprocessor.include_dirs.empty()) {
    out << "(none)\n";
  } else {
    for (size_t i = 0; i < preprocessor.include_dirs.size(); ++i) {
      if (i > 0) out << ", ";
      out << preprocessor.include_dirs[i];
    }
    out << "\n";
  }
  out << "  Defined Macros: ";
  if (preprocessor.defined_macros.empty()) {
    out << "(none)\n";
  } else {
    for (size_t i = 0; i < preprocessor.defined_macros.size(); ++i) {
      if (i > 0) out << ", ";
      out << preprocessor.defined_macros[i];
    }
    out << "\n";
  }

  out << "\n[CodeGen]\n";
  out << "  Local Var Mode: ";
  switch (codegen.local_var_storage) {
  case LocalVarStorage::Auto: out << "auto\n"; break;
  case LocalVarStorage::Stack: out << "stack\n"; break;
  case LocalVarStorage::Memory: out << "memory\n"; break;
  }

  out << "\n[Linker]\n";
  out << "  Library Dirs:   ";
  if (linker.lib_dirs.empty()) {
    out << "(none)\n";
  } else {
    for (size_t i = 0; i < linker.lib_dirs.size(); ++i) {
      if (i > 0) out << ", ";
      out << linker.lib_dirs[i];
    }
    out << "\n";
  }
  out << "  Libraries (-l): ";
  if (linker.lib_names.empty()) {
    out << "(none)\n";
  } else {
    for (size_t i = 0; i < linker.lib_names.size(); ++i) {
      if (i > 0) out << ", ";
      out << linker.lib_names[i];
    }
    out << "\n";
  }
  out << "  Library Files:  ";
  if (linker.lib_files.empty()) {
    out << "(none)\n";
  } else {
    for (size_t i = 0; i < linker.lib_files.size(); ++i) {
      if (i > 0) out << ", ";
      out << linker.lib_files[i];
    }
    out << "\n";
  }
  out << "  Explicit DLLs:  ";
  if (linker.explicit_dlls.empty()) {
    out << "(none)\n";
  } else {
    for (size_t i = 0; i < linker.explicit_dlls.size(); ++i) {
      if (i > 0) out << ", ";
      out << linker.explicit_dlls[i];
    }
    out << "\n";
  }
  out << "  Explicit Imports: ";
  if (linker.explicit_imports.empty()) {
    out << "(none)\n";
  } else {
    size_t count = 0;
    for (const auto &pair : linker.explicit_imports) {
      if (count++ > 0) out << ", ";
      out << pair.first << "=" << pair.second;
    }
    out << "\n";
  }
  out << "==============================\n";
}

void CommandLineParser::print_usage(const char *prog_name, std::ostream &out) {
  out << "Usage: " << prog_name << " [options] <source_files...> [libraries...]\n\n"
      << "Options:\n"
      << "  -t, --target <spec>    Target architecture and OS specification [default: host default]\n"
      << "  --type <type>          Target file type (auto, exe, obj, shared) [default: auto]\n"
      << "  --local-vars <mode>    Local variable storage (stack, memory) [default: memory for avr, stack for others]\n"
      << "  -o, --output <file>    Output file [default: a.exe (Windows) / a.out (Linux) / a.hex (AVR)]\n"
      << "  -c                     Compile and assemble only; do not link\n"
      << "  -S                     Stop after compilation phase; output assembly code\n"
      << "  -E                     Stop after preprocessing phase; output preprocessed source\n"
      << "  --emit-ir, --ir        Stop after IR generation phase; output intermediate representation\n"
      << "  --show-ir              Print intermediate representation to stderr during compilation\n"
      << "  -shared                Produce shared library / DLL\n"
      << "  -v, --verbose          Verbose output; display compiler configuration before execution\n"
      << "  -I <dir>               Add directory to include search path\n"
      << "  -D <macro>[=val]       Define preprocessor macro\n"
      << "  -l<lib>                Link against library archive or DLL (e.g. -luser32)\n"
      << "  -L<dir>                Add directory to library search path\n"
      << "  --import <dll:func>    Explicitly map symbol to DLL (e.g. --import user32.dll:MessageBoxA)\n"
      << "  --ast                  Print AST to stdout\n"
      << "  -h, --help             Display this help message\n\n";

  TargetSpec::print_supported_targets(out);
}

bool CommandLineParser::parse(int argc, char *argv[], CompilerConfig &config,
                              std::string &error_message, bool &help_requested) {
  std::vector<std::string> args;
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }
  return parse(args, config, error_message, help_requested);
}

enum class MatchResult {
  NoMatch,
  Success,
  MissingValue
};

static MatchResult match_option(const std::string &arg,
                                const std::string &short_opt,
                                const std::string &long_opt,
                                bool allow_attached_short,
                                const std::vector<std::string> &args,
                                size_t &i,
                                std::string &val,
                                std::string &err) {
  // Check long option
  if (!long_opt.empty()) {
    std::string prefix = long_opt + "=";
    if (arg.rfind(prefix, 0) == 0) {
      val = arg.substr(prefix.size());
      return MatchResult::Success;
    }
    if (arg == long_opt) {
      if (i + 1 >= args.size()) {
        err = "Missing argument for " + long_opt;
        return MatchResult::MissingValue;
      }
      val = args[++i];
      return MatchResult::Success;
    }
  }

  // Check short option
  if (!short_opt.empty()) {
    std::string prefix = short_opt + "=";
    if (arg.rfind(prefix, 0) == 0) {
      val = arg.substr(prefix.size());
      return MatchResult::Success;
    }
    if (arg == short_opt) {
      if (i + 1 >= args.size()) {
        err = "Missing argument for " + short_opt;
        return MatchResult::MissingValue;
      }
      val = args[++i];
      return MatchResult::Success;
    }
    if (allow_attached_short && arg.rfind(short_opt, 0) == 0 && arg.size() > short_opt.size()) {
      val = arg.substr(short_opt.size());
      return MatchResult::Success;
    }
  }

  return MatchResult::NoMatch;
}

bool CommandLineParser::parse(const std::vector<std::string> &args, CompilerConfig &config,
                              std::string &error_message, bool &help_requested) {
  help_requested = false;
  error_message.clear();

  for (size_t i = 0; i < args.size(); ++i) {
    const std::string &arg = args[i];

    // Help
    if (arg == "-h" || arg == "--help") {
      help_requested = true;
      return true;
    }

    // Flags
    if (arg == "-v" || arg == "--verbose") {
      config.general.verbose = true;
      continue;
    }
    if (arg == "--ast") {
      config.general.show_ast = true;
      continue;
    }
    if (arg == "--show-ir") {
      config.general.show_ir = true;
      continue;
    }
    if (arg == "--emit-ir" || arg == "-emit-ir" || arg == "--ir") {
      config.general.stop_phase = StopPhase::IROnly;
      continue;
    }
    if (arg == "-S") {
      config.general.stop_phase = StopPhase::AssembleOnly;
      continue;
    }
    if (arg == "-E") {
      config.general.stop_phase = StopPhase::PreprocessOnly;
      continue;
    }
    if (arg == "-c") {
      config.general.stop_phase = StopPhase::CompileOnly;
      config.general.target_file_type = TargetFileType::ObjectFile;
      continue;
    }
    if (arg == "-shared") {
      config.general.target_file_type = TargetFileType::SharedLib;
      continue;
    }

    std::string val;
    std::string err;

    // Target type: --type=..., --type <type>
    MatchResult res = match_option(arg, "", "--type", false, args, i, val, err);
    if (res == MatchResult::MissingValue) {
      error_message = err;
      return false;
    }
    if (res == MatchResult::Success) {
      if (val == "exe" || val == "executable") config.general.target_file_type = TargetFileType::Executable;
      else if (val == "obj" || val == "object") config.general.target_file_type = TargetFileType::ObjectFile;
      else if (val == "dll" || val == "shared") config.general.target_file_type = TargetFileType::SharedLib;
      else if (val == "auto") config.general.target_file_type = TargetFileType::Auto;
      else {
        error_message = "Unknown target file type '" + val + "'. Valid types: auto, exe, obj, shared";
        return false;
      }
      continue;
    }

    // Target: -t, -t=..., --target, --target=...
    res = match_option(arg, "-t", "--target", false, args, i, val, err);
    if (res == MatchResult::MissingValue) {
      error_message = err;
      return false;
    }
    if (res == MatchResult::Success) {
      try {
        config.general.target_spec = TargetSpec::parse(val);
        config.general.target = val;
      } catch (const std::exception &e) {
        error_message = e.what();
        return false;
      }
      continue;
    }

    // Output: -o, -o=..., --output, --output=...
    res = match_option(arg, "-o", "--output", false, args, i, val, err);
    if (res == MatchResult::MissingValue) {
      error_message = err;
      return false;
    }
    if (res == MatchResult::Success) {
      config.general.output_filename = val;
      continue;
    }

    // Local variable storage: --local-vars, --local-vars=...
    res = match_option(arg, "", "--local-vars", false, args, i, val, err);
    if (res == MatchResult::MissingValue) {
      error_message = err;
      return false;
    }
    if (res == MatchResult::Success) {
      if (val == "stack") {
        config.codegen.local_var_storage = LocalVarStorage::Stack;
      } else if (val == "memory" || val == "static") {
        config.codegen.local_var_storage = LocalVarStorage::Memory;
      } else if (val == "auto") {
        config.codegen.local_var_storage = LocalVarStorage::Auto;
      } else {
        error_message = "Invalid argument for --local-vars ('" + val + "'). Expected 'stack' or 'memory'";
        return false;
      }
      continue;
    }

    // Include path: -I, -I=..., -I<path>, --include, --include=...
    res = match_option(arg, "-I", "--include", true, args, i, val, err);
    if (res == MatchResult::MissingValue) {
      error_message = err;
      return false;
    }
    if (res == MatchResult::Success) {
      config.preprocessor.include_dirs.push_back(val);
      continue;
    }

    // Define macro: -D, -D=..., -D<macro>, --define, --define=...
    res = match_option(arg, "-D", "--define", true, args, i, val, err);
    if (res == MatchResult::MissingValue) {
      error_message = err;
      return false;
    }
    if (res == MatchResult::Success) {
      config.preprocessor.defined_macros.push_back(val);
      continue;
    }

    // Library: -l, -l=..., -l<lib>
    res = match_option(arg, "-l", "", true, args, i, val, err);
    if (res == MatchResult::MissingValue) {
      error_message = err;
      return false;
    }
    if (res == MatchResult::Success) {
      config.linker.lib_names.push_back(val);
      continue;
    }

    // Library search directory: -L, -L=..., -L<dir>
    res = match_option(arg, "-L", "", true, args, i, val, err);
    if (res == MatchResult::MissingValue) {
      error_message = err;
      return false;
    }
    if (res == MatchResult::Success) {
      config.linker.lib_dirs.push_back(val);
      continue;
    }

    // Explicit import: --import <spec>, --import=<spec>
    res = match_option(arg, "", "--import", false, args, i, val, err);
    if (res == MatchResult::MissingValue) {
      error_message = err;
      return false;
    }
    if (res == MatchResult::Success) {
      size_t eq = val.find('=');
      size_t colon = val.find(':');
      if (eq != std::string::npos) {
        config.linker.explicit_imports[val.substr(0, eq)] = val.substr(eq + 1);
      } else if (colon != std::string::npos) {
        std::string p1 = val.substr(0, colon);
        std::string p2 = val.substr(colon + 1);
        std::string p1_lower = p1;
        for (char &c : p1_lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (p1_lower.rfind(".dll") != std::string::npos) {
          config.linker.explicit_imports[p2] = p1;
        } else {
          config.linker.explicit_imports[p1] = p2;
        }
      } else {
        error_message = "Invalid format for --import. Expected <dll:symbol> or <symbol=dll>";
        return false;
      }
      continue;
    }

    // Stdin
    if (arg == "-") {
      config.general.input_filenames.push_back("-");
      continue;
    }

    // Non-option positional arguments (input files, library archives, DLLs)
    if (!arg.empty() && arg[0] != '-') {
      std::string lower = arg;
      for (char &c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
      if (lower.size() > 4 && lower.substr(lower.size() - 4) == ".lib") {
        config.linker.lib_files.push_back(arg);
      } else if (lower.size() > 2 && lower.substr(lower.size() - 2) == ".a") {
        config.linker.lib_files.push_back(arg);
      } else if (lower.size() > 4 && lower.substr(lower.size() - 4) == ".dll") {
        config.linker.explicit_dlls.push_back(arg);
      } else {
        config.general.input_filenames.push_back(arg);
      }
      continue;
    }

    // Unknown option
    error_message = "Unknown option '" + arg + "'";
    return false;
  }

  if (config.general.input_filenames.empty()) {
    error_message = "No input file specified.";
    return false;
  }
  config.general.input_filename = config.general.input_filenames[0];

  return true;
}
