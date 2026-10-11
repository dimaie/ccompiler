#include "assembler_linker.hpp"
#include "codegen.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static void print_usage(const char *prog_name) {
  std::cerr << "Usage: " << prog_name << " [options] <source_file>\n\n"
            << "Options:\n"
            << "  -t, --target <arch>    Target architecture (x86_64, arm64, avr, c) [default: x86_64]\n"
            << "  -o, --output <file>    Output file [default: a.exe (Windows) / a.out]\n"
            << "  -S                     Stop after compilation phase; output assembly code\n"
            << "  --ast                  Print AST to stdout\n"
            << "  -h, --help             Display this help message\n";
}

int main(int argc, char *argv[]) {
  std::string input_filename;
  std::string output_filename;
  std::string target = "x86_64";
  bool show_ast = false;
  bool emit_asm_only = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      print_usage(argv[0]);
      return 0;
    } else if (arg == "--ast") {
      show_ast = true;
    } else if (arg == "-S") {
      emit_asm_only = true;
    } else if (arg == "-t" || arg == "--target") {
      if (i + 1 >= argc) {
        std::cerr << "Error: Missing argument for " << arg << "\n";
        return 1;
      }
      target = argv[++i];
    } else if (arg == "-o" || arg == "--output") {
      if (i + 1 >= argc) {
        std::cerr << "Error: Missing argument for " << arg << "\n";
        return 1;
      }
      output_filename = argv[++i];
    } else if (arg == "-" || (!arg.empty() && arg[0] != '-')) {
      if (!input_filename.empty()) {
        std::cerr << "Error: Multiple input files specified ('"
                  << input_filename << "' and '" << arg << "')\n";
        return 1;
      }
      input_filename = arg;
    } else {
      std::cerr << "Error: Unknown option '" << arg << "'\n";
      print_usage(argv[0]);
      return 1;
    }
  }

  if (input_filename.empty()) {
    std::cerr << "Error: No input file specified.\n";
    print_usage(argv[0]);
    return 1;
  }

  std::string code;
  if (input_filename == "-") {
    std::stringstream buffer;
    buffer << std::cin.rdbuf();
    code = buffer.str();
  } else {
    std::ifstream file(input_filename);
    if (!file.is_open()) {
      std::cerr << "Error: Could not open input file '" << input_filename << "'\n";
      return 1;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    code = buffer.str();
  }

  Lexer lexer;
  lexer.init(code.c_str());

  Parser parser(&lexer);
  try {
    auto ast = parser.parse_program();

    if (show_ast) {
      if (output_filename.empty()) {
        std::cerr << "=== Abstract Syntax Tree ===\n";
      }
      ast->print(0);
    }

    auto codegen = CodeGeneratorFactory::create(target);
    std::ostringstream generated_out;
    codegen->generate(*ast, generated_out);

    if (emit_asm_only) {
      if (!output_filename.empty()) {
        std::ofstream out_file(output_filename);
        if (!out_file.is_open()) {
          std::cerr << "Error: Could not open output file '" << output_filename << "'\n";
          return 1;
        }
        out_file << generated_out.str();
        if (!out_file.good()) {
          std::cerr << "Error: Failed to write to output file '" << output_filename << "'\n";
          return 1;
        }
      } else {
        std::cout << generated_out.str();
      }
      return 0;
    }

    auto linker = AssemblerLinkerFactory::create(target);
    if (!linker->is_supported()) {
      // Stub: Stop at generating assembly phase and report feature not yet available
      std::cerr << "Notice: Assembler and linker features are not yet available for target '"
                << target << "'. Compilation stopped at assembly phase.\n";

      if (!output_filename.empty()) {
        std::ofstream out_file(output_filename);
        if (out_file.is_open()) {
          out_file << generated_out.str();
        }
      } else if (input_filename != "-") {
        std::string fallback_asm = input_filename;
        auto dot_pos = fallback_asm.find_last_of('.');
        if (dot_pos != std::string::npos) {
          fallback_asm = fallback_asm.substr(0, dot_pos);
        }
        fallback_asm += (target == "c" ? ".transpiled.c" : ".s");
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
    std::string exe_path = output_filename;
    if (exe_path.empty()) {
#ifdef _WIN32
      exe_path = "a.exe";
#else
      exe_path = "a.out";
#endif
    }

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
