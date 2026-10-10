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
            << "  -o, --output <file>    Output file [default: stdout]\n"
            << "  --ast                  Print AST to stdout\n"
            << "  -h, --help             Display this help message\n";
}

int main(int argc, char *argv[]) {
  std::string input_filename;
  std::string output_filename;
  std::string target = "x86_64";
  bool show_ast = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      print_usage(argv[0]);
      return 0;
    } else if (arg == "--ast") {
      show_ast = true;
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
    } else if (!arg.empty() && arg[0] != '-') {
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
      ast->print(0);
    }

    auto codegen = CodeGeneratorFactory::create(target);
    if (!output_filename.empty()) {
      std::ofstream out_file(output_filename);
      if (!out_file.is_open()) {
        std::cerr << "Error: Could not open output file '" << output_filename << "'\n";
        return 1;
      }
      codegen->generate(*ast, out_file);
    } else {
      codegen->generate(*ast, std::cout);
    }

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
