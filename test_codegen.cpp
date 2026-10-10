#include "codegen.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <cassert>
#include <iostream>
#include <string>

static std::unique_ptr<ProgramNode> parse(const char *source) {
  Lexer lexer;
  lexer.init(source);
  Parser parser(&lexer);
  return parser.parse_program();
}

static void test_factory() {
  std::cout << "Testing CodeGeneratorFactory...\n";

  auto x86 = CodeGeneratorFactory::create("x86_64");
  assert(x86 != nullptr);
  assert(x86->get_arch() == TargetArch::X86_64);

  auto avr = CodeGeneratorFactory::create("avr");
  assert(avr != nullptr);
  assert(avr->get_arch() == TargetArch::AVR);

  auto arm = CodeGeneratorFactory::create("arm64");
  assert(arm != nullptr);
  assert(arm->get_arch() == TargetArch::ARM64);

  auto rpi = CodeGeneratorFactory::create("rpi");
  assert(rpi != nullptr);
  assert(rpi->get_arch() == TargetArch::ARM64);

  auto c_gen = CodeGeneratorFactory::create("c");
  assert(c_gen != nullptr);
  assert(c_gen->get_arch() == TargetArch::C_Transpiler);

  bool threw = false;
  try {
    CodeGeneratorFactory::create("invalid_arch");
  } catch (const std::exception &) {
    threw = true;
  }
  assert(threw);
}

static void test_x86_64_codegen() {
  std::cout << "Testing x86_64 Code Generation...\n";

  auto prog = parse("int a = 5;\n"
                    "int b = a + 2;\n"
                    "return b;\n");
  auto gen = CodeGeneratorFactory::create(TargetArch::X86_64);
  std::string asm_code = gen->generate_to_string(*prog);

  // Check essential x86-64 instructions and structure
  assert(asm_code.find(".globl main") != std::string::npos);
  assert(asm_code.find("pushq %rbp") != std::string::npos);
  assert(asm_code.find("movq $5, %rax") != std::string::npos);
  assert(asm_code.find("addq %r10, %rax") != std::string::npos);
  assert(asm_code.find("leave") != std::string::npos);
  assert(asm_code.find("ret") != std::string::npos);

  // Test while and do-while loops
  {
    auto loop_prog = parse("int x = 0;\n"
                           "while (x < 10) {\n"
                           "  x = x + 1;\n"
                           "}\n"
                           "do {\n"
                           "  x = x - 1;\n"
                           "} while (x > 0);\n"
                           "return x;\n");
    std::string loop_asm = gen->generate_to_string(*loop_prog);
    assert(loop_asm.find(".Lwhile_cond") != std::string::npos);
    assert(loop_asm.find(".Ldowhile_body") != std::string::npos);
  }
}

static void test_arm64_codegen() {
  std::cout << "Testing ARM64 (Raspberry Pi) Code Generation...\n";

  auto prog = parse("int a = 5;\n"
                    "int b = a + 2;\n"
                    "for (int i = 0; i < 10; i = i + 1) {\n"
                    "  a = a + 1;\n"
                    "}\n"
                    "return b;\n");
  auto gen = CodeGeneratorFactory::create(TargetArch::ARM64);
  std::string asm_code = gen->generate_to_string(*prog);

  // Check essential ARM64 / AAPCS64 instructions
  assert(asm_code.find(".arch armv8-a") != std::string::npos);
  assert(asm_code.find("stp x29, x30, [sp, -64]!") != std::string::npos);
  assert(asm_code.find("mov x29, sp") != std::string::npos);
  assert(asm_code.find("mov x0, #5") != std::string::npos);
  assert(asm_code.find("add x0, x0, x1") != std::string::npos);
  assert(asm_code.find("str x0, [x29,") != std::string::npos);
  assert(asm_code.find("ldr x0, [x29,") != std::string::npos);
  assert(asm_code.find("ldp x29, x30, [sp], 64") != std::string::npos);
  assert(asm_code.find("ret") != std::string::npos);

  // Test while and do-while loops
  {
    auto loop_prog = parse("int x = 0;\n"
                           "while (x < 5) {\n"
                           "  x = x + 1;\n"
                           "}\n"
                           "do {\n"
                           "  x = x - 1;\n"
                           "} while (x > 0);\n"
                           "return x;\n");
    std::string loop_asm = gen->generate_to_string(*loop_prog);
    assert(loop_asm.find(".Larm_while_cond") != std::string::npos);
    assert(loop_asm.find(".Larm_dowhile_body") != std::string::npos);
    assert(loop_asm.find("cbnz x0,") != std::string::npos);
  }
}

static void test_avr_codegen() {
  std::cout << "Testing AVR (Arduino UNO / ATmega328P) Code Generation...\n";

  auto prog = parse("int count = 10;\n"
                    "if (count > 0) {\n"
                    "  return count;\n"
                    "} else {\n"
                    "  return 0;\n"
                    "}\n");
  auto gen = CodeGeneratorFactory::create(TargetArch::AVR);
  std::string asm_code = gen->generate_to_string(*prog);

  // Check AVR directives and register conventions
  assert(asm_code.find(".arch atmega328p") != std::string::npos);
  assert(asm_code.find("push r28") != std::string::npos);
  assert(asm_code.find("push r29") != std::string::npos);
  assert(asm_code.find("r24") != std::string::npos);
  assert(asm_code.find("r25") != std::string::npos);
  assert(asm_code.find("std Y+") != std::string::npos);
  assert(asm_code.find("ret") != std::string::npos);

  // Test while and do-while loops
  {
    auto loop_prog = parse("int x = 0;\n"
                           "while (x < 5) {\n"
                           "  x = x + 1;\n"
                           "}\n"
                           "do {\n"
                           "  x = x - 1;\n"
                           "} while (x > 0);\n"
                           "return x;\n");
    std::string loop_asm = gen->generate_to_string(*loop_prog);
    assert(loop_asm.find(".Lavr_while_cond") != std::string::npos);
    assert(loop_asm.find(".Lavr_dowhile_body") != std::string::npos);
    assert(loop_asm.find("brne .Lavr_dowhile_body") != std::string::npos);
  }
}

static void test_c_codegen() {
  std::cout << "Testing C Transpiler Code Generation...\n";

  auto prog = parse("int x = 42;\nreturn x + 1;\n");
  auto gen = CodeGeneratorFactory::create(TargetArch::C_Transpiler);
  std::string c_code = gen->generate_to_string(*prog);

  assert(c_code.find("int main(void)") != std::string::npos);
  assert(c_code.find("int x = 42;") != std::string::npos);
  assert(c_code.find("return (x + 1);") != std::string::npos);

  // Test while and do-while loops
  {
    auto loop_prog = parse("int a = 0;\n"
                           "while (a < 10) {\n"
                           "  a = a + 1;\n"
                           "}\n"
                           "do {\n"
                           "  a = a - 1;\n"
                           "} while (a > 0);\n"
                           "return a;\n");
    std::string loop_c = gen->generate_to_string(*loop_prog);
    assert(loop_c.find("while ((a < 10)) {") != std::string::npos);
    assert(loop_c.find("do {") != std::string::npos);
    assert(loop_c.find("} while ((a > 0));") != std::string::npos);
  }
}

// Mock code generator that does NOT implement while or for loops
class MockMinimalCodeGenerator : public ICodeGenerator {
public:
  TargetArch get_arch() const override { return TargetArch::X86_64; }
  const char *get_arch_name() const override { return "minimal_mock"; }
  void generate(const ProgramNode &program, std::ostream &) override {
    program.accept(*this);
  }
  // Only overrides ProgramNode and ReturnNode; leaves everything else to ICodeGenerator defaults
  void visit(const ProgramNode &node) override {
    for (const auto &stmt : node.statements) {
      if (stmt) stmt->accept(*this);
    }
  }
  void visit(const ReturnNode &) override {}
};

static void test_unsupported_features() {
  std::cout << "Testing Unsupported Feature Error Handling...\n";

  // 1. Unimplemented AST node in an unsupported generator requires zero code in the generator
  {
    MockMinimalCodeGenerator mock_gen;
    auto prog = parse("while (1) { return 0; }\n");

    bool threw = false;
    try {
      mock_gen.generate_to_string(*prog);
    } catch (const UnsupportedFeatureException &e) {
      threw = true;
      assert(e.get_feature() == "while loop");
      assert(e.get_arch() == "minimal_mock");
      assert(e.get_line() == 1);
      std::string msg = e.what();
      assert(msg.find("Feature 'while loop' is not supported by minimal_mock code generator (line 1)") != std::string::npos);
    }
    assert(threw);
  }

  // 2. Unsupported operator in x86_64 generator
  {
    auto x86 = CodeGeneratorFactory::create("x86_64");
    auto prog = std::make_unique<ProgramNode>(42);
    prog->append(std::make_unique<BinaryOpNode>(42, TOK_ASSIGN, std::make_unique<IntLitNode>(42, 1), std::make_unique<IntLitNode>(42, 2)));

    bool threw = false;
    try {
      x86->generate_to_string(*prog);
    } catch (const UnsupportedFeatureException &e) {
      threw = true;
      assert(e.get_arch() == "x86_64");
      assert(e.get_line() == 42);
      std::string msg = e.what();
      assert(msg.find("Binary operator '='") != std::string::npos);
    }
    assert(threw);
  }

  // 3. Unsupported operator in AVR generator
  {
    auto avr = CodeGeneratorFactory::create("avr");
    auto prog = std::make_unique<ProgramNode>(10);
    prog->append(std::make_unique<BinaryOpNode>(10, TOK_ASSIGN, std::make_unique<IntLitNode>(10, 1), std::make_unique<IntLitNode>(10, 2)));

    bool threw = false;
    try {
      avr->generate_to_string(*prog);
    } catch (const UnsupportedFeatureException &e) {
      threw = true;
      assert(e.get_arch() == "avr");
      assert(e.get_line() == 10);
    }
    assert(threw);
  }

  // 4. Unsupported operator in ARM64 generator
  {
    auto arm = CodeGeneratorFactory::create("arm64");
    auto prog = std::make_unique<ProgramNode>(15);
    prog->append(std::make_unique<BinaryOpNode>(15, TOK_ASSIGN, std::make_unique<IntLitNode>(15, 1), std::make_unique<IntLitNode>(15, 2)));

    bool threw = false;
    try {
      arm->generate_to_string(*prog);
    } catch (const UnsupportedFeatureException &e) {
      threw = true;
      assert(e.get_arch() == "arm64");
      assert(e.get_line() == 15);
    }
    assert(threw);
  }
}

int main() {
  std::cout << "========================================\n";
  std::cout << "  Running Code Generator Test Suite\n";
  std::cout << "========================================\n";

  test_factory();
  test_x86_64_codegen();
  test_arm64_codegen();
  test_avr_codegen();
  test_c_codegen();
  test_unsupported_features();

  std::cout << "========================================\n";
  std::cout << "  ALL CODEGEN TESTS PASSED! (6/6)\n";
  std::cout << "========================================\n";
  return 0;
}

