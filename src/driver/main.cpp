#include "assembler_linker.hpp"
#include "codegen.hpp"
#include "compiler_config.hpp"
#include "ir_builder.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "preprocessor.hpp"
#include "target_info.hpp"
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

  // Preprocessing setup
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

  std::vector<std::string> generated_assembly_list;
  auto linker = AssemblerLinkerFactory::create(config.general.target);

  for (size_t file_idx = 0; file_idx < config.general.input_filenames.size(); ++file_idx) {
    const std::string &input_name = config.general.input_filenames[file_idx];
    std::string code;
    if (input_name == "-") {
      std::stringstream buffer;
      buffer << std::cin.rdbuf();
      code = buffer.str();
    } else {
      std::ifstream file(input_name);
      if (!file.is_open()) {
        std::cerr << "Error: Could not open input file '" << input_name << "'\n";
        return 1;
      }
      std::stringstream buffer;
      buffer << file.rdbuf();
      code = buffer.str();
    }

    std::string preprocessed_code;
    try {
      preprocessed_code = preprocessor.process(code, (input_name == "-") ? "" : input_name);
    } catch (const std::exception &e) {
      std::cerr << "Error in '" << input_name << "': " << e.what() << "\n";
      return 1;
    }

    if (config.general.stop_phase == StopPhase::PreprocessOnly) {
      if (!config.general.output_filename.empty() && config.general.input_filenames.size() == 1) {
        std::ofstream out_file(config.general.output_filename);
        if (!out_file.is_open()) {
          std::cerr << "Error: Could not open output file '" << config.general.output_filename << "'\n";
          return 1;
        }
        out_file << preprocessed_code;
      } else {
        std::cout << preprocessed_code;
      }
      continue;
    }

    Lexer lexer;
    lexer.init(preprocessed_code.c_str());

    Parser parser_obj(&lexer);
    try {
      auto ast = parser_obj.parse_program();

      if (config.general.show_ast) {
        if (config.general.output_filename.empty()) {
          std::cerr << "=== Abstract Syntax Tree for '" << input_name << "' ===\n";
        }
        ast->print(0);
      }

      TargetInfo target_info = TargetInfo::from_string(config.general.target, config.codegen.local_var_storage);
      IRBuilder ir_builder(target_info);
      IRModule ir_mod = ir_builder.build(*ast);

      if (config.general.show_ir) {
        if (config.general.output_filename.empty()) {
          std::cerr << "=== Intermediate Representation (IR) for '" << input_name << "' ===\n";
        }
        ir_mod.print(std::cerr);
      }

      if (config.general.stop_phase == StopPhase::IROnly) {
        std::ostringstream ir_out;
        ir_mod.print(ir_out);
        std::string ir_str = ir_out.str();

        if (!config.general.output_filename.empty() && config.general.input_filenames.size() == 1) {
          std::ofstream out_file(config.general.output_filename);
          if (!out_file.is_open()) {
            std::cerr << "Error: Could not open output file '" << config.general.output_filename << "'\n";
            return 1;
          }
          out_file << ir_str;
          if (!out_file.good()) {
            std::cerr << "Error: Failed to write to output file '" << config.general.output_filename << "'\n";
            return 1;
          }
        } else {
          std::cout << ir_str;
        }
        continue;
      }

      auto codegen = CodeGeneratorFactory::create(config.general.target, config.get_codegen_options());
      std::ostringstream generated_out;
      codegen->generate(*ast, generated_out);
      std::string asm_str = generated_out.str();
      generated_assembly_list.push_back(asm_str);

      if (config.general.stop_phase == StopPhase::AssembleOnly) {
        if (!config.general.output_filename.empty() && config.general.input_filenames.size() == 1) {
          std::ofstream out_file(config.general.output_filename);
          if (!out_file.is_open()) {
            std::cerr << "Error: Could not open output file '" << config.general.output_filename << "'\n";
            return 1;
          }
          out_file << asm_str;
          if (!out_file.good()) {
            std::cerr << "Error: Failed to write to output file '" << config.general.output_filename << "'\n";
            return 1;
          }
        } else {
          std::cout << asm_str;
        }
        continue;
      }
    } catch (const std::exception &e) {
      std::cerr << "Error in '" << input_name << "': " << e.what() << "\n";
      return 1;
    }
  }

  if (config.general.stop_phase == StopPhase::PreprocessOnly ||
      config.general.stop_phase == StopPhase::IROnly ||
      config.general.stop_phase == StopPhase::AssembleOnly) {
    return 0;
  }

  if (config.general.stop_phase == StopPhase::CompileOnly) {
    if (config.general.verbose) {
      std::cout << "Successfully compiled and assembled "
                << config.general.input_filenames.size() << " translation unit(s).\n";
    }
    return 0;
  }

  if (!linker->is_supported()) {
    std::cerr << "Notice: Assembler and linker features are not yet available for target '"
              << config.general.target << "'. Compilation stopped at assembly phase.\n";

    if (!config.general.output_filename.empty() && !generated_assembly_list.empty()) {
      std::ofstream out_file(config.general.output_filename);
      if (out_file.is_open()) {
        for (const auto &asm_code : generated_assembly_list) {
          out_file << asm_code << "\n";
        }
      }
    } else {
      for (size_t i = 0; i < config.general.input_filenames.size(); ++i) {
        const auto &in_name = config.general.input_filenames[i];
        if (in_name != "-" && i < generated_assembly_list.size()) {
          std::string fallback_asm = in_name;
          auto dot_pos = fallback_asm.find_last_of('.');
          if (dot_pos != std::string::npos) {
            fallback_asm = fallback_asm.substr(0, dot_pos);
          }
          fallback_asm += (config.general.target == "c" ? ".transpiled.c" : ".s");
          std::ofstream out_file(fallback_asm);
          if (out_file.is_open()) {
            out_file << generated_assembly_list[i];
            std::cerr << "Generated output written to '" << fallback_asm << "'.\n";
          }
        } else if (i < generated_assembly_list.size()) {
          std::cout << generated_assembly_list[i];
        }
      }
    }
    return 0;
  }

  // Default executable / binary destination
  std::string exe_path = config.general.output_filename;
  if (exe_path.empty()) {
    if (config.general.target == "avr" || config.general.target == "arduino") {
      exe_path = "a.hex";
    } else {
#ifdef _WIN32
      exe_path = "a.exe";
#else
      exe_path = "a.out";
#endif
    }
  }

  linker->set_linker_config(config.linker);
  auto link_res = linker->assemble_and_link_multiple(generated_assembly_list, exe_path, config.general.target_file_type);
  if (!link_res.success) {
    std::cerr << "Error: " << link_res.message << "\n";
    return 1;
  }

  return 0;
}
