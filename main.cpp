#include "assembler_linker.hpp"
#include "codegen.hpp"
#include "compiler_config.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "preprocessor.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

int main(int argc, char *argv[]) {
  CompilerConfig config;
  CommandLineParser parser;
  std::string error_msg;
  bool help_requested = false;

  if (!parser.parse(argc, argv, config, error_msg, help_requested)) {
    std::cerr << "Error: " << error_msg << "\n";
    CommandLineParser::print_usage(argv[0]);
    return 1;
  }

  if (help_requested) {
    CommandLineParser::print_usage(argv[0], std::cout);
    return 0;
  }

  // Configuration output must be done before any task run by the compiler if verbose is set
  if (config.general.verbose) {
    config.print(std::cout);
  }

  std::string code;
  if (config.general.input_filename == "-") {
    std::stringstream buffer;
    buffer << std::cin.rdbuf();
    code = buffer.str();
  } else {
    std::ifstream file(config.general.input_filename);
    if (!file.is_open()) {
      std::cerr << "Error: Could not open input file '" << config.general.input_filename << "'\n";
      return 1;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    code = buffer.str();
  }

  // Preprocessing phase
  Preprocessor preprocessor;
  for (const auto &dir : config.preprocessor.include_dirs) {
    preprocessor.add_include_dir(dir);
  }
  for (const auto &m : config.preprocessor.defined_macros) {
    size_t eq = m.find('=');
    if (eq != std::string::npos) {
      preprocessor.define_macro(m.substr(0, eq), m.substr(eq + 1));
    } else {
      preprocessor.define_macro(m, "1");
    }
  }

  std::string preprocessed_code;
  try {
    preprocessed_code = preprocessor.process(code, (config.general.input_filename == "-") ? "" : config.general.input_filename);
  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  if (config.general.stop_phase == StopPhase::PreprocessOnly) {
    if (!config.general.output_filename.empty()) {
      std::ofstream out_file(config.general.output_filename);
      if (!out_file.is_open()) {
        std::cerr << "Error: Could not open output file '" << config.general.output_filename << "'\n";
        return 1;
      }
      out_file << preprocessed_code;
    } else {
      std::cout << preprocessed_code;
    }
    return 0;
  }

  Lexer lexer;
  lexer.init(preprocessed_code.c_str());

  Parser parser_obj(&lexer);
  try {
    auto ast = parser_obj.parse_program();

    if (config.general.show_ast) {
      if (config.general.output_filename.empty()) {
        std::cerr << "=== Abstract Syntax Tree ===\n";
      }
      ast->print(0);
    }

    auto codegen = CodeGeneratorFactory::create(config.general.target, config.get_codegen_options());
    std::ostringstream generated_out;
    codegen->generate(*ast, generated_out);

    if (config.general.stop_phase == StopPhase::AssembleOnly) {
      if (!config.general.output_filename.empty()) {
        std::ofstream out_file(config.general.output_filename);
        if (!out_file.is_open()) {
          std::cerr << "Error: Could not open output file '" << config.general.output_filename << "'\n";
          return 1;
        }
        out_file << generated_out.str();
        if (!out_file.good()) {
          std::cerr << "Error: Failed to write to output file '" << config.general.output_filename << "'\n";
          return 1;
        }
      } else {
        std::cout << generated_out.str();
      }
      return 0;
    }

    auto linker = AssemblerLinkerFactory::create(config.general.target);
    if (!linker->is_supported()) {
      // Stub: Stop at generating assembly phase and report feature not yet available
      std::cerr << "Notice: Assembler and linker features are not yet available for target '"
                << config.general.target << "'. Compilation stopped at assembly phase.\n";

      if (!config.general.output_filename.empty()) {
        std::ofstream out_file(config.general.output_filename);
        if (out_file.is_open()) {
          out_file << generated_out.str();
        }
      } else if (config.general.input_filename != "-") {
        std::string fallback_asm = config.general.input_filename;
        auto dot_pos = fallback_asm.find_last_of('.');
        if (dot_pos != std::string::npos) {
          fallback_asm = fallback_asm.substr(0, dot_pos);
        }
        fallback_asm += (config.general.target == "c" ? ".transpiled.c" : ".s");
        std::ofstream out_file(fallback_asm);
        if (out_file.is_open()) {
          out_file << generated_out.str();
          std::cerr << "Generated output written to '" << fallback_asm << "'.\n";
        }
      } else {
        std::cout << generated_out.str();
      }
      return 0;
    }

    // Default executable destination
    std::string exe_path = config.general.output_filename;
    if (exe_path.empty()) {
#ifdef _WIN32
      exe_path = "a.exe";
#else
      exe_path = "a.out";
#endif
    }

    linker->set_linker_config(config.linker);
    auto link_res = linker->assemble_and_link(generated_out.str(), exe_path);
    if (!link_res.success) {
      std::cerr << "Error: " << link_res.message << "\n";
      return 1;
    }

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
