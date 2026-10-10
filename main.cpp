#include "codegen.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <cstring>
#include <iostream>

int main(int argc, char *argv[]) {
  const char *code = "int a = 5;\n"
                     "int b = (a + 2) * 3;\n"
                     "for (int i = 0; i < 10; i = i + 1) {\n"
                     "  a = a + 1;\n"
                     "}\n"
                     "if (b > 15) {\n"
                     "  return b;\n"
                     "} else {\n"
                     "  return 0;\n"
                     "}\n";

  std::string target = "x86_64";
  bool show_ast = false;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--ast") == 0) {
      show_ast = true;
    } else if ((std::strcmp(argv[i], "--target") == 0 ||
                std::strcmp(argv[i], "-t") == 0) &&
               i + 1 < argc) {
      target = argv[++i];
    }
  }

  Lexer lexer;
  lexer.init(code);

  Parser parser(&lexer);
  try {
    auto ast = parser.parse_program();

    if (show_ast || argc == 1) {
      ast->print(0);
    }

    auto codegen = CodeGeneratorFactory::create(target);
    std::cout << "\n; === Code Generator Target: " << codegen->get_arch_name()
              << " ===\n";
    codegen->generate(*ast, std::cout);

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
