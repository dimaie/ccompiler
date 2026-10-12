#include "assembler_linker.hpp"
#include "codegen.hpp"
#include "compiler_config.hpp"
#include "x86_64/internal_elf_linker.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
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

  // Test switch-case
  {
    auto sw_prog = parse("int x = 2;\n"
                         "int res = 0;\n"
                         "switch (x) {\n"
                         "  case 1:\n"
                         "    res = 10;\n"
                         "    break;\n"
                         "  case 2:\n"
                         "    res = 20;\n"
                         "    break;\n"
                         "  default:\n"
                         "    res = -1;\n"
                         "    break;\n"
                         "}\n"
                         "return res;\n");
    std::string sw_asm = gen->generate_to_string(*sw_prog);
    assert(sw_asm.find(".Lswitch_case") != std::string::npos);
    assert(sw_asm.find(".Lswitch_default") != std::string::npos);
    assert(sw_asm.find(".Lswitch_end") != std::string::npos);
  }

  // Test continue in loops and goto/label
  {
    auto cg_prog = parse("int sum = 0;\n"
                         "for (int i = 0; i < 10; i = i + 1) {\n"
                         "  if (i == 5) continue;\n"
                         "  sum = sum + i;\n"
                         "}\n"
                         "loop_start:\n"
                         "sum = sum + 1;\n"
                         "if (sum < 100) goto loop_start;\n"
                         "return sum;\n");
    std::string cg_asm = gen->generate_to_string(*cg_prog);
    assert(cg_asm.find("jmp .Lfor_step") != std::string::npos);
    assert(cg_asm.find(".Luser_loop_start:") != std::string::npos);
    assert(cg_asm.find("jmp .Luser_loop_start") != std::string::npos);
  }

  // Test short and long modifiers
  {
    auto type_prog = parse("short s = 10;\n"
                           "long l = 20;\n"
                           "s = s + 1;\n"
                           "return s + l;\n");
    std::string type_asm = gen->generate_to_string(*type_prog);
    assert(type_asm.find("movw %ax,") != std::string::npos);
    assert(type_asm.find("movswq") != std::string::npos);
    assert(type_asm.find("movq") != std::string::npos);
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

  // Test switch-case
  {
    auto sw_prog = parse("int x = 1;\n"
                         "switch (x) {\n"
                         "  case 1:\n"
                         "    return 100;\n"
                         "  default:\n"
                         "    return 0;\n"
                         "}\n");
    std::string sw_asm = gen->generate_to_string(*sw_prog);
    assert(sw_asm.find(".Larm_switch_case") != std::string::npos);
    assert(sw_asm.find(".Larm_switch_default") != std::string::npos);
    assert(sw_asm.find(".Larm_switch_end") != std::string::npos);
  }

  // Test continue and goto/label
  {
    auto cg_prog = parse("int i = 0;\n"
                         "while (i < 10) {\n"
                         "  i = i + 1;\n"
                         "  if (i == 5) continue;\n"
                         "}\n"
                         "my_label:\n"
                         "goto my_label;\n"
                         "return i;\n");
    std::string cg_asm = gen->generate_to_string(*cg_prog);
    assert(cg_asm.find("b .Larm_while_cond") != std::string::npos);
    assert(cg_asm.find(".Luser_my_label:") != std::string::npos);
    assert(cg_asm.find("b .Luser_my_label") != std::string::npos);
  }

  // Test short and long modifiers
  {
    auto type_prog = parse("short s = 10;\n"
                           "long l = 20;\n"
                           "s = s + 1;\n"
                           "return s + l;\n");
    std::string type_asm = gen->generate_to_string(*type_prog);
    assert(type_asm.find("strh w0,") != std::string::npos);
    assert(type_asm.find("ldrsh x0,") != std::string::npos);
    assert(type_asm.find("str x0,") != std::string::npos);
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
  // Default AVR uses static memory storage (.bss)
  auto gen = CodeGeneratorFactory::create(TargetArch::AVR);
  assert(gen->get_effective_local_var_storage() == LocalVarStorage::Memory);
  std::string asm_code = gen->generate_to_string(*prog);

  // Check AVR directives and register conventions in default memory mode
  assert(asm_code.find(".arch atmega328p") != std::string::npos);
  assert(asm_code.find("r24") != std::string::npos);
  assert(asm_code.find("r25") != std::string::npos);
  assert(asm_code.find("sts __local_main_count") != std::string::npos);
  assert(asm_code.find("lds r24, __local_main_count") != std::string::npos);
  assert(asm_code.find(".section .bss") != std::string::npos);
  assert(asm_code.find("__local_main_count:") != std::string::npos);
  assert(asm_code.find(".zero 2") != std::string::npos);
  assert(asm_code.find("ret") != std::string::npos);
  // In memory mode, frame pointer setup is omitted
  assert(asm_code.find("push r28") == std::string::npos);

  // Test AVR explicitly configured with stack frame storage
  {
    CodeGenOptions stack_opts;
    stack_opts.local_var_storage = LocalVarStorage::Stack;
    auto stack_gen = CodeGeneratorFactory::create(TargetArch::AVR, stack_opts);
    assert(stack_gen->get_effective_local_var_storage() == LocalVarStorage::Stack);
    std::string stack_asm = stack_gen->generate_to_string(*prog);
    assert(stack_asm.find("push r28") != std::string::npos);
    assert(stack_asm.find("push r29") != std::string::npos);
    assert(stack_asm.find("std Y+") != std::string::npos);
    assert(stack_asm.find("ldd r24, Y+") != std::string::npos);
    assert(stack_asm.find("ret") != std::string::npos);
  }

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

  // Test switch-case
  {
    auto sw_prog = parse("int x = 1;\n"
                         "switch (x) {\n"
                         "  case 1:\n"
                         "    return 100;\n"
                         "  default:\n"
                         "    return 0;\n"
                         "}\n");
    std::string sw_asm = gen->generate_to_string(*sw_prog);
    assert(sw_asm.find(".Lavr_switch_case") != std::string::npos);
    assert(sw_asm.find(".Lavr_switch_default") != std::string::npos);
    assert(sw_asm.find(".Lavr_switch_end") != std::string::npos);
  }

  // Test continue and goto/label
  {
    auto cg_prog = parse("int i = 0;\n"
                         "do {\n"
                         "  i = i + 1;\n"
                         "  if (i == 3) continue;\n"
                         "} while (i < 5);\n"
                         "target:\n"
                         "goto target;\n"
                         "return i;\n");
    std::string cg_asm = gen->generate_to_string(*cg_prog);
    assert(cg_asm.find("rjmp .Lavr_dowhile_cond") != std::string::npos);
    assert(cg_asm.find(".Luser_target:") != std::string::npos);
    assert(cg_asm.find("rjmp .Luser_target") != std::string::npos);
  }

  // Test short modifier (memory vs stack)
  {
    auto type_prog = parse("short s = 10;\n"
                           "return s;\n");
    std::string type_asm = gen->generate_to_string(*type_prog);
    assert(type_asm.find("sts __local_main_s") != std::string::npos);
    assert(type_asm.find("lds r24, __local_main_s") != std::string::npos);

    gen->set_local_var_storage(LocalVarStorage::Stack);
    std::string stack_type_asm = gen->generate_to_string(*type_prog);
    assert(stack_type_asm.find("std Y+") != std::string::npos);
    assert(stack_type_asm.find("ldd r24,") != std::string::npos);
    gen->set_local_var_storage(LocalVarStorage::Memory);
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

  // Test switch-case
  {
    auto sw_prog = parse("int x = 2;\n"
                         "switch (x) {\n"
                         "  case 1:\n"
                         "    return 10;\n"
                         "  case 2:\n"
                         "    return 20;\n"
                         "  default:\n"
                         "    return 0;\n"
                         "}\n");
    std::string sw_c = gen->generate_to_string(*sw_prog);
    assert(sw_c.find("switch (x) {") != std::string::npos);
    assert(sw_c.find("case 1:") != std::string::npos);
    assert(sw_c.find("case 2:") != std::string::npos);
    assert(sw_c.find("default:") != std::string::npos);
  }

  // Test continue and goto/label
  {
    auto cg_prog = parse("int i = 0;\n"
                         "start:\n"
                         "while (i < 10) {\n"
                         "  i = i + 1;\n"
                         "  continue;\n"
                         "}\n"
                         "goto start;\n"
                         "return i;\n");
    std::string cg_c = gen->generate_to_string(*cg_prog);
    assert(cg_c.find("continue;") != std::string::npos);
    assert(cg_c.find("start:") != std::string::npos);
    assert(cg_c.find("goto start;") != std::string::npos);
  }

  // Test short and long modifiers
  {
    auto type_prog = parse("short s = 10;\n"
                           "long l = 20;\n"
                           "long long ll = 30;\n"
                           "return s + l + ll;\n");
    std::string type_c = gen->generate_to_string(*type_prog);
    assert(type_c.find("short s = 10;") != std::string::npos);
    assert(type_c.find("long l = 20;") != std::string::npos);
    assert(type_c.find("long long ll = 30;") != std::string::npos);
  }
}

// Mock code generator that does NOT implement while or for loops or switch
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

  // 1. Unimplemented AST node (while loop) in an unsupported generator
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

  // 2. Unimplemented AST node (switch statement) in an unsupported generator
  {
    MockMinimalCodeGenerator mock_gen;
    auto prog = parse("switch (1) { case 1: break; }\n");

    bool threw = false;
    try {
      mock_gen.generate_to_string(*prog);
    } catch (const UnsupportedFeatureException &e) {
      threw = true;
      assert(e.get_feature() == "switch statement");
      assert(e.get_arch() == "minimal_mock");
    }
    assert(threw);
  }

  // 3. Unimplemented AST node (continue statement) in an unsupported generator
  {
    MockMinimalCodeGenerator mock_gen;
    auto prog = parse("continue;\n");

    bool threw = false;
    try {
      mock_gen.generate_to_string(*prog);
    } catch (const UnsupportedFeatureException &e) {
      threw = true;
      assert(e.get_feature() == "continue statement");
      assert(e.get_arch() == "minimal_mock");
    }
    assert(threw);
  }

  // 4. Unimplemented AST node (goto statement) in an unsupported generator
  {
    MockMinimalCodeGenerator mock_gen;
    auto prog = parse("goto foo;\n");

    bool threw = false;
    try {
      mock_gen.generate_to_string(*prog);
    } catch (const UnsupportedFeatureException &e) {
      threw = true;
      assert(e.get_feature() == "goto statement");
      assert(e.get_arch() == "minimal_mock");
    }
    assert(threw);
  }

  // 5. Unimplemented AST node (label statement) in an unsupported generator
  {
    MockMinimalCodeGenerator mock_gen;
    auto prog = parse("foo: return 0;\n");

    bool threw = false;
    try {
      mock_gen.generate_to_string(*prog);
    } catch (const UnsupportedFeatureException &e) {
      threw = true;
      assert(e.get_feature() == "label statement");
      assert(e.get_arch() == "minimal_mock");
    }
    assert(threw);
  }

  // 6. Continue statement outside loop error in backends
  {
    auto x86 = CodeGeneratorFactory::create("x86_64");
    auto prog = parse("continue;\n");
    bool threw = false;
    try {
      x86->generate_to_string(*prog);
    } catch (const UnsupportedFeatureException &e) {
      threw = true;
      assert(e.get_feature() == "continue statement outside of loop");
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

  // 5. Unsupported long modifier on AVR
  {
    auto avr = CodeGeneratorFactory::create("avr");
    auto prog = parse("long x = 10;\nreturn x;\n");
    bool threw = false;
    try {
      avr->generate_to_string(*prog);
    } catch (const UnsupportedFeatureException &e) {
      threw = true;
      assert(e.get_feature() == "type modifier 'long'");
      assert(e.get_arch() == "avr");
    }
    assert(threw);
  }
}

static void test_functions_codegen() {
  std::cout << "Testing Function Definitions & Calls Code Generation...\n";

  const char *src =
      "int add(int a, int b) {\n"
      "  return a + b;\n"
      "}\n"
      "int main() {\n"
      "  return add(2, 3);\n"
      "}\n";

  // 1. x86_64
  {
    auto gen = CodeGeneratorFactory::create("x86_64");
    auto prog = parse(src);
    std::string asm_code = gen->generate_to_string(*prog);

    assert(asm_code.find(".globl add") != std::string::npos);
    assert(asm_code.find("add:") != std::string::npos);
    assert(asm_code.find(".globl main") != std::string::npos);
    assert(asm_code.find("main:") != std::string::npos);
    assert(asm_code.find("call add") != std::string::npos);
    assert(asm_code.find("leave") != std::string::npos);
    assert(asm_code.find("ret") != std::string::npos);

    // Test recursive function (factorial)
    const char *fact_src =
        "int fact(int n) {\n"
        "  if (n <= 1) return 1;\n"
        "  return n * fact(n - 1);\n"
        "}\n"
        "int main() {\n"
        "  return fact(5);\n"
        "}\n";
    auto fact_prog = parse(fact_src);
    std::string fact_asm = gen->generate_to_string(*fact_prog);
    assert(fact_asm.find(".globl fact") != std::string::npos);
    assert(fact_asm.find("call fact") != std::string::npos);

    // Test > 6 args unsupported
    auto bad_call = parse("return foo(1, 2, 3, 4, 5, 6, 7);\n");
    bool threw = false;
    try {
      gen->generate_to_string(*bad_call);
    } catch (const UnsupportedFeatureException &e) {
      threw = true;
      assert(e.get_arch() == "x86_64");
    }
    assert(threw);

    // Test variadic call sets %al to 0
    auto var_call = parse("int printf(const char *fmt, ...);\nint main() { printf(\"hello %d\", 42); return 0; }\n");
    std::string var_asm = gen->generate_to_string(*var_call);
    assert(var_asm.find("movb $0, %al") != std::string::npos);
    assert(var_asm.find("call printf") != std::string::npos);

    // Test variadic function definition spills argument registers
    auto var_def = parse("int sum(int count, ...) { return count; }\n");
    std::string var_def_asm = gen->generate_to_string(*var_def);
    assert(var_def_asm.find("movq %rdi, -48(%rbp)") != std::string::npos);
    assert(var_def_asm.find("movq %rsi, -40(%rbp)") != std::string::npos);
    assert(var_def_asm.find("movq %r9, -8(%rbp)") != std::string::npos);

    // Test variadic sum function with va_start, va_arg, va_end
    auto va_prog = parse(
      "int sum(int count, ...) {\n"
      "  va_list ap;\n"
      "  va_start(ap, count);\n"
      "  int total = 0;\n"
      "  int i = 0;\n"
      "  while (i < count) {\n"
      "    total = total + va_arg(ap, int);\n"
      "    i = i + 1;\n"
      "  }\n"
      "  va_end(ap);\n"
      "  return total;\n"
      "}\n"
      "int main() {\n"
      "  return sum(3, 10, 20, 30);\n"
      "}\n"
    );
    std::string va_asm = gen->generate_to_string(*va_prog);
    assert(va_asm.find("sum:") != std::string::npos);
    assert(va_asm.find("call sum") != std::string::npos);
  }

  // 2. ARM64
  {
    auto gen = CodeGeneratorFactory::create("arm64");
    auto prog = parse(src);
    std::string asm_code = gen->generate_to_string(*prog);

    assert(asm_code.find(".globl add") != std::string::npos);
    assert(asm_code.find("add:") != std::string::npos);
    assert(asm_code.find(".globl main") != std::string::npos);
    assert(asm_code.find("main:") != std::string::npos);
    assert(asm_code.find("bl add") != std::string::npos);
    assert(asm_code.find("ret") != std::string::npos);

    // Test > 8 args unsupported
    auto bad_call = parse("return foo(1, 2, 3, 4, 5, 6, 7, 8, 9);\n");
    bool threw = false;
    try {
      gen->generate_to_string(*bad_call);
    } catch (const UnsupportedFeatureException &e) {
      threw = true;
      assert(e.get_arch() == "arm64");
    }
    assert(threw);

    // Test ARM64 variadic function definition spills registers
    auto var_arm = parse("int sum(int count, ...) { return count; }\n");
    std::string var_arm_asm = gen->generate_to_string(*var_arm);
    assert(var_arm_asm.find("str x0, [x29, #16]") != std::string::npos);
    assert(var_arm_asm.find("str x7, [x29, #72]") != std::string::npos);
  }

  // 3. AVR
  {
    auto gen = CodeGeneratorFactory::create("avr");
    auto prog = parse(src);
    std::string asm_code = gen->generate_to_string(*prog);

    assert(asm_code.find(".global add") != std::string::npos);
    assert(asm_code.find("add:") != std::string::npos);
    assert(asm_code.find(".global main") != std::string::npos);
    assert(asm_code.find("main:") != std::string::npos);
    assert(asm_code.find("call add") != std::string::npos);
    assert(asm_code.find("ret") != std::string::npos);

    // Test > 4 args unsupported
    auto bad_call = parse("return foo(1, 2, 3, 4, 5);\n");
    bool threw = false;
    try {
      gen->generate_to_string(*bad_call);
    } catch (const UnsupportedFeatureException &e) {
      threw = true;
      assert(e.get_arch() == "avr");
    }
    assert(threw);
  }

  // 4. C Transpiler
  {
    auto gen = CodeGeneratorFactory::create("c");
    auto prog = parse(src);
    std::string c_code = gen->generate_to_string(*prog);

    assert(c_code.find("int add(int a, int b)") != std::string::npos);
    assert(c_code.find("int main(void)") != std::string::npos);
    assert(c_code.find("add(2, 3)") != std::string::npos);

    // Test script-style with function def
    const char *script_src =
        "int square(int x) {\n"
        "  return x * x;\n"
        "}\n"
        "return square(4);\n";
    auto script_prog = parse(script_src);
    std::string script_c = gen->generate_to_string(*script_prog);
    assert(script_c.find("int square(int x)") != std::string::npos);
    assert(script_c.find("int main(void)") != std::string::npos);
    assert(script_c.find("return square(4);") != std::string::npos);

    // Test variadic function transpilation
    auto var_c = parse("int printf(const char *format, ...);\nint sum(int count, ...) { return count; }\n");
    std::string var_c_out = gen->generate_to_string(*var_c);
    assert(var_c_out.find("int printf(char *format, ...);") != std::string::npos);
    assert(var_c_out.find("int sum(int count, ...)") != std::string::npos);
  }
}

static void test_arrays_codegen() {
  std::cout << "Testing Arrays Code Generation across all backends...\n";

  const char *src =
      "int a[3] = {10, 20, 30};\n"
      "a[1] = 42;\n"
      "int sum = a[0] + a[1] + a[2];\n"
      "return sum;\n";

  // 1. x86_64
  {
    auto gen = CodeGeneratorFactory::create("x86_64");
    auto prog = parse(src);
    std::string asm_code = gen->generate_to_string(*prog);
    assert(asm_code.find("leaq") != std::string::npos);
    assert(asm_code.find("movq $10,") != std::string::npos);
    assert(asm_code.find("movq $42,") != std::string::npos);
  }

  // 2. ARM64
  {
    auto gen = CodeGeneratorFactory::create("arm64");
    auto prog = parse(src);
    std::string asm_code = gen->generate_to_string(*prog);
    assert(asm_code.find("lsl x2, x0, #3") != std::string::npos);
    assert(asm_code.find("ldr x0, [x1]") != std::string::npos);
    assert(asm_code.find("str x0, [x2]") != std::string::npos);
  }

  // 3. AVR
  {
    auto gen = CodeGeneratorFactory::create("avr");
    auto prog = parse(src);
    std::string asm_code = gen->generate_to_string(*prog);
    assert(asm_code.find("lsl r24") != std::string::npos);
    assert(asm_code.find("rol r25") != std::string::npos);
    assert(asm_code.find("ld r24, Z") != std::string::npos);
    assert(asm_code.find("st Z, r24") != std::string::npos);
  }

  // 4. C Transpiler
  {
    auto gen = CodeGeneratorFactory::create("c");
    auto prog = parse(src);
    std::string c_code = gen->generate_to_string(*prog);
    assert(c_code.find("int a[3] = {10, 20, 30};") != std::string::npos);
    assert(c_code.find("a[1] = 42;") != std::string::npos);
    assert(c_code.find("a[0]") != std::string::npos);
    assert(c_code.find("a[1]") != std::string::npos);
    assert(c_code.find("a[2]") != std::string::npos);
  }

  // 5. Short array testing on x86_64, arm64, and C
  {
    const char *short_src =
        "short s[2] = {5, 15};\n"
        "s[0] = s[1];\n"
        "return s[0];\n";
    auto prog = parse(short_src);

    auto x86_gen = CodeGeneratorFactory::create("x86_64");
    std::string x86_asm = x86_gen->generate_to_string(*prog);
    assert(x86_asm.find("movswq") != std::string::npos);
    assert(x86_asm.find("movw %ax,") != std::string::npos);

    auto arm_gen = CodeGeneratorFactory::create("arm64");
    std::string arm_asm = arm_gen->generate_to_string(*prog);
    assert(arm_asm.find("ldrsh x0, [x1]") != std::string::npos);
    assert(arm_asm.find("strh w0, [x2]") != std::string::npos);

    auto c_gen = CodeGeneratorFactory::create("c");
    std::string c_out = c_gen->generate_to_string(*prog);
    assert(c_out.find("short s[2] = {5, 15};") != std::string::npos);
  }
}

static void test_pointers_codegen() {
  std::cout << "Testing Pointers Code Generation across all backends...\n";

  const char *src =
      "int x = 10;\n"
      "int *p = &x;\n"
      "*p = 42;\n"
      "return *p;\n";

  // 1. x86_64
  {
    auto gen = CodeGeneratorFactory::create("x86_64");
    auto prog = parse(src);
    std::string asm_code = gen->generate_to_string(*prog);
    assert(asm_code.find("leaq") != std::string::npos);
    assert(asm_code.find("movq (%rax), %rax") != std::string::npos);
    assert(asm_code.find("movq %rax, (%r10)") != std::string::npos);
  }

  // 2. ARM64
  {
    auto gen = CodeGeneratorFactory::create("arm64");
    auto prog = parse(src);
    std::string asm_code = gen->generate_to_string(*prog);
    assert(asm_code.find("add x0, x29,") != std::string::npos);
    assert(asm_code.find("ldr x0, [x0]") != std::string::npos);
    assert(asm_code.find("str x0, [x1]") != std::string::npos);
  }

  // 3. AVR
  {
    auto gen = CodeGeneratorFactory::create("avr");
    auto prog = parse(src);
    std::string asm_code = gen->generate_to_string(*prog);
    assert(asm_code.find("ld r24, Z") != std::string::npos);
    assert(asm_code.find("st Z, r24") != std::string::npos);
  }

  // 4. C Transpiler
  {
    auto gen = CodeGeneratorFactory::create("c");
    auto prog = parse(src);
    std::string c_code = gen->generate_to_string(*prog);
    assert(c_code.find("int *p = &x;") != std::string::npos);
    assert(c_code.find("(*p) = 42;") != std::string::npos);
    assert(c_code.find("return (*p);") != std::string::npos);
  }
}

static void test_func_ptrs_codegen() {
  std::cout << "Testing Function Pointers Code Generation across all backends...\n";

  const char *src =
      "int add(int a, int b) {\n"
      "  return a + b;\n"
      "}\n"
      "int (*fp)(int, int) = add;\n"
      "int r1 = fp(2, 3);\n"
      "int r2 = (*fp)(10, 20);\n"
      "return r1 + r2;\n";

  // 1. x86_64
  {
    auto gen = CodeGeneratorFactory::create("x86_64");
    auto prog = parse(src);
    std::string asm_code = gen->generate_to_string(*prog);
    assert(asm_code.find("leaq add(%rip), %rax") != std::string::npos);
    assert(asm_code.find("call *%r11") != std::string::npos);
  }

  // 2. ARM64
  {
    auto gen = CodeGeneratorFactory::create("arm64");
    auto prog = parse(src);
    std::string asm_code = gen->generate_to_string(*prog);
    assert(asm_code.find("adrp x0, add") != std::string::npos);
    assert(asm_code.find("blr x16") != std::string::npos);
  }

  // 3. AVR
  {
    auto gen = CodeGeneratorFactory::create("avr");
    auto prog = parse(src);
    std::string asm_code = gen->generate_to_string(*prog);
    assert(asm_code.find("pm_lo8(add)") != std::string::npos);
    assert(asm_code.find("icall") != std::string::npos);
  }

  // 4. C Transpiler
  {
    auto gen = CodeGeneratorFactory::create("c");
    auto prog = parse(src);
    std::string c_code = gen->generate_to_string(*prog);
    assert(c_code.find("int (*fp)(int, int) = add;") != std::string::npos);
    assert(c_code.find("fp(2, 3);") != std::string::npos);
    assert(c_code.find("(*fp)") != std::string::npos);
  }
}

static void test_struct_codegen() {
  std::cout << "Testing Structures Code Generation across all backends...\n";

  const char *src =
      "struct Point {\n"
      "  int x;\n"
      "  int y;\n"
      "};\n"
      "struct Point p = {10, 20};\n"
      "struct Point *pp = &p;\n"
      "p.x = 42;\n"
      "pp->y = 99;\n"
      "return p.x + pp->y;\n";

  // 1. x86_64
  {
    auto gen = CodeGeneratorFactory::create("x86_64");
    auto prog = parse(src);
    std::string asm_code = gen->generate_to_string(*prog);
    assert(asm_code.find("movq") != std::string::npos);
    assert(asm_code.find("leaq") != std::string::npos);
  }

  // 2. ARM64
  {
    auto gen = CodeGeneratorFactory::create("arm64");
    auto prog = parse(src);
    std::string asm_code = gen->generate_to_string(*prog);
    assert(asm_code.find("ldr x0, [x29, #") != std::string::npos);
    assert(asm_code.find("str x0, [x29, #") != std::string::npos);
    assert(asm_code.find("str x0, [x1]") != std::string::npos);
  }

  // 3. AVR
  {
    auto gen = CodeGeneratorFactory::create("avr");
    auto prog = parse(src);
    std::string asm_code = gen->generate_to_string(*prog);
    assert(asm_code.find("sts __local_main_p") != std::string::npos);
    assert(asm_code.find("lds r24, __local_main_p") != std::string::npos);
    assert(asm_code.find("st Z, r24") != std::string::npos);

    // Also test stack mode for structs
    gen->set_local_var_storage(LocalVarStorage::Stack);
    std::string stack_asm = gen->generate_to_string(*prog);
    assert(stack_asm.find("ldd r24, Y+") != std::string::npos);
    assert(stack_asm.find("std Y+") != std::string::npos);
  }

  // 4. C Transpiler
  {
    auto gen = CodeGeneratorFactory::create("c");
    auto prog = parse(src);
    std::string c_code = gen->generate_to_string(*prog);
    assert(c_code.find("struct Point {") != std::string::npos);
    assert(c_code.find("struct Point p = {10, 20};") != std::string::npos);
    assert(c_code.find("struct Point *pp = &p;") != std::string::npos);
    assert(c_code.find("p.x = 42;") != std::string::npos);
    assert(c_code.find("pp->y = 99;") != std::string::npos);
  }
}

static void test_union_codegen() {
  std::cout << "Testing Unions Code Generation across all backends...\n";

  const char *src =
      "union Data {\n"
      "  int i;\n"
      "  short s;\n"
      "};\n"
      "union Data d;\n"
      "d.i = 42;\n"
      "return d.i;\n";

  // 1. x86_64
  {
    auto gen = CodeGeneratorFactory::create("x86_64");
    auto prog = parse(src);
    std::string asm_code = gen->generate_to_string(*prog);
    assert(asm_code.find("movq") != std::string::npos);
  }

  // 2. ARM64
  {
    auto gen = CodeGeneratorFactory::create("arm64");
    auto prog = parse(src);
    std::string asm_code = gen->generate_to_string(*prog);
    assert(asm_code.find("str x0, [x29, #") != std::string::npos);
  }

  // 3. AVR
  {
    auto gen = CodeGeneratorFactory::create("avr");
    auto prog = parse(src);
    std::string asm_code = gen->generate_to_string(*prog);
    assert(asm_code.find("sts __local_main_d") != std::string::npos);
    assert(asm_code.find("lds r24, __local_main_d") != std::string::npos);

    // Also test stack mode for unions
    gen->set_local_var_storage(LocalVarStorage::Stack);
    std::string stack_asm = gen->generate_to_string(*prog);
    assert(stack_asm.find("std Y+") != std::string::npos);
  }

  // 4. C Transpiler
  {
    auto gen = CodeGeneratorFactory::create("c");
    auto prog = parse(src);
    std::string c_code = gen->generate_to_string(*prog);
    assert(c_code.find("union Data {") != std::string::npos);
    assert(c_code.find("union Data d;") != std::string::npos);
    assert(c_code.find("d.i = 42;") != std::string::npos);
  }
}

static void test_code_review_fixes() {
  std::cout << "Testing Code Review fixes across C and x86_64 backends...\n";

  // 1. C backend: non-compound statement bodies terminating with semicolon
  {
    auto gen = CodeGeneratorFactory::create("c");
    auto prog = parse("int x = 1;\n"
                      "int y = 0;\n"
                      "int z = 0;\n"
                      "if (x) y = 1; else z = 2;\n"
                      "while (x < 5) x = x + 1;\n"
                      "for (int i = 0; i < 5; i = i + 1) y = y + i;\n"
                      "while (x); \n");
    std::string c_code = gen->generate_to_string(*prog);
    assert(c_code.find("if (x) y = 1; else z = 2;") != std::string::npos);
    assert(c_code.find("while ((x < 5)) x = (x + 1);") != std::string::npos);
    assert(c_code.find("for (int i = 0; (i < 5); i = (i + 1)) y = (y + i);") != std::string::npos);
    assert(c_code.find("while (x) ;") != std::string::npos);
  }

  // 2. C backend: unary negative not producing --
  {
    auto gen = CodeGeneratorFactory::create("c");
    auto prog = parse("int x = 5;\n"
                      "int a = -(-x);\n"
                      "int b = -(-1);\n"
                      "return a + b;\n");
    std::string c_code = gen->generate_to_string(*prog);
    assert(c_code.find("--x") == std::string::npos);
    assert(c_code.find("--1") == std::string::npos);
    assert(c_code.find("- -x") != std::string::npos || c_code.find("- - x") != std::string::npos || c_code.find("- (-x)") != std::string::npos);
  }

  // 3. C backend: global variables preserved when main() is present
  {
    auto gen = CodeGeneratorFactory::create("c");
    auto prog = parse("int global_var = 42;\n"
                      "int foo() { return global_var; }\n"
                      "int main() { return foo(); }\n");
    std::string c_code = gen->generate_to_string(*prog);
    assert(c_code.find("int global_var = 42;") != std::string::npos);
    assert(c_code.find("int foo(void) {") != std::string::npos);
    assert(c_code.find("int main(void) {") != std::string::npos);
  }

  // 4. C backend: StructDef with var_decl not emitted twice
  {
    auto gen = CodeGeneratorFactory::create("c");
    auto prog = parse("struct Point { int x; int y; } p;\n"
                      "int foo() { return p.x; }\n"
                      "return foo();\n");
    std::string c_code = gen->generate_to_string(*prog);
    size_t first = c_code.find("struct Point {");
    assert(first != std::string::npos);
    size_t second = c_code.find("struct Point {", first + 1);
    assert(second == std::string::npos);
  }

  // 5. x86_64: short pointer handling and dereference
  {
    auto gen = CodeGeneratorFactory::create("x86_64");
    auto prog = parse("short x = 10;\n"
                      "short *p = &x;\n"
                      "*p = 20;\n"
                      "short *q = p;\n"
                      "return *q;\n");
    std::string asm_code = gen->generate_to_string(*prog);
    // Writing through short* must be 16-bit store movw %ax, (%r10)
    assert(asm_code.find("movw %ax, (%r10)") != std::string::npos);
    // Reading through short* must be 16-bit load movswq (%rax), %rax
    assert(asm_code.find("movswq (%rax), %rax") != std::string::npos);
    // Pointer assignment q = p must be 64-bit movq, not movw
    assert(asm_code.find("movswq -") == std::string::npos); // p is short*, not short scalar
  }

  // 6. x86_64: pointer indexing p[i] uses pointer value, not stack address
  {
    auto gen = CodeGeneratorFactory::create("x86_64");
    auto prog = parse("short x = 10;\n"
                      "short *p = &x;\n"
                      "return p[0];\n");
    std::string asm_code = gen->generate_to_string(*prog);
    // Must load pointer into %r11: movq -offset(%rbp), %r11
    assert(asm_code.find("movq -") != std::string::npos);
    assert(asm_code.find("(%r11, %rax, 2)") != std::string::npos);
  }
}

static void test_char_and_string_literals_codegen() {
  std::cout << "Testing Char and String Literals Codegen...\n";

  // 1. x86_64: char scalar store and load
  {
    auto gen = CodeGeneratorFactory::create("x86_64");
    auto prog = parse("char c = 'A';\n"
                      "return c;\n");
    std::string asm_code = gen->generate_to_string(*prog);
    assert(asm_code.find("movb %al, -") != std::string::npos);
    assert(asm_code.find("movsbq -") != std::string::npos);
  }

  // 2. x86_64: char array and pointer indexing
  {
    auto gen = CodeGeneratorFactory::create("x86_64");
    auto prog = parse("char arr[4];\n"
                      "arr[1] = 65;\n"
                      "return arr[1];\n");
    std::string asm_code = gen->generate_to_string(*prog);
    assert(asm_code.find("movb %al, (%r11, %r10, 1)") != std::string::npos);
    assert(asm_code.find("movsbq (%r11, %rax, 1), %rax") != std::string::npos);
  }

  // 3. x86_64: string literal and .rodata
  {
    auto gen = CodeGeneratorFactory::create("x86_64");
    auto prog = parse("int main() {\n"
                      "  puts(\"Hello, World!\");\n"
                      "  return 0;\n"
                      "}\n");
    std::string asm_code = gen->generate_to_string(*prog);
    assert(asm_code.find(".section .rodata") != std::string::npos);
    assert(asm_code.find(".string \"Hello, World!\"") != std::string::npos);
    assert(asm_code.find("leaq .LC_") != std::string::npos);
    assert(asm_code.find("call puts") != std::string::npos);
    assert(asm_code.find("movq %rdi, %rcx") != std::string::npos); // Windows ABI support
    assert(asm_code.find("subq $40, %rsp") != std::string::npos); // 32-byte shadow space
  }

  // 4. C backend: char and string literal
  {
    auto gen = CodeGeneratorFactory::create("c");
    auto prog = parse("char *msg = \"test\";\n");
    std::string c_code = gen->generate_to_string(*prog);
    assert(c_code.find("char *msg = \"test\";") != std::string::npos);
  }
}

static void test_assembler_linker() {
  std::cout << "Testing Assembler and Linker Subsystem...\n";

  // 1. Factory creation & support checking
  {
    auto x86_linker = AssemblerLinkerFactory::create("x86_64");
    assert(x86_linker != nullptr);
    assert(x86_linker->get_arch() == TargetArch::X86_64);
    assert(x86_linker->is_supported() == true);

    auto arm_linker = AssemblerLinkerFactory::create("arm64");
    assert(arm_linker != nullptr);
    assert(arm_linker->is_supported() == false);

    auto avr_linker = AssemblerLinkerFactory::create("avr");
    assert(avr_linker != nullptr);
    assert(avr_linker->is_supported() == true);

    auto c_linker = AssemblerLinkerFactory::create("c");
    assert(c_linker != nullptr);
    assert(c_linker->is_supported() == false);
  }

  // 1b. AVR Assembler and Linker: produce Intel HEX file
  {
    auto avr_linker = AssemblerLinkerFactory::create("avr");
    auto avr_gen = CodeGeneratorFactory::create("avr");
    auto prog = parse("int main() {\n"
                      "  int x = 42;\n"
                      "  return x + 1;\n"
                      "}\n");
    std::string asm_code = avr_gen->generate_to_string(*prog);
    std::string hex_path = "test_avr_output.hex";
    auto res = avr_linker->assemble_and_link(asm_code, hex_path);
    assert(res.success);
    assert(std::filesystem::exists(hex_path));

    std::ifstream hex_file(hex_path);
    std::string first_line;
    std::getline(hex_file, first_line);
    assert(!first_line.empty() && first_line[0] == ':'); // Valid Intel HEX start
    hex_file.close();

    std::error_code ec;
    std::filesystem::remove(hex_path, ec);
  }

  // 2. Stub linker behavior
  {
    auto arm_linker = AssemblerLinkerFactory::create("arm64");
    auto res = arm_linker->assemble_and_link("dummy assembly", "test_stub.s");
    assert(!res.success);
    assert(res.message.find("not yet available") != std::string::npos);
    std::error_code ec;
    std::filesystem::remove("test_stub.s", ec);
  }

  // 3. x86_64 assembler and linker execution
  {
    auto x86_linker = AssemblerLinkerFactory::create("x86_64");
    auto gen = CodeGeneratorFactory::create("x86_64");
    auto prog = parse("int main() { return 42; }\n");
    std::string asm_code = gen->generate_to_string(*prog);

    std::string exe_path = "test_build_exe.exe";
    auto res = x86_linker->assemble_and_link(asm_code, exe_path);
    assert(res.success);
    assert(std::filesystem::exists(exe_path));

    // Run the generated executable and verify exit code
    int code = std::system(".\\test_build_exe.exe");
    assert(code == 42);

    std::error_code ec;
    std::filesystem::remove(exe_path, ec);
  }

  // 4. Embedded assembler and linker: complex functions, string literals & puts
  {
    auto x86_linker = AssemblerLinkerFactory::create("x86_64");
    auto gen = CodeGeneratorFactory::create("x86_64");
    auto prog = parse("int add(int a, int b) { return a + b; }\n"
                      "int main() {\n"
                      "  puts(\"Testing self-contained linking!\");\n"
                      "  return add(10, 20);\n"
                      "}\n");
    std::string asm_code = gen->generate_to_string(*prog);

    std::string exe_path = "test_complex_exe.exe";
    auto res = x86_linker->assemble_and_link(asm_code, exe_path);
    assert(res.success);
    assert(std::filesystem::exists(exe_path));

    int code = std::system(".\\test_complex_exe.exe");
    assert(code == 30);

    std::error_code ec;
    std::filesystem::remove(exe_path, ec);
  }

  // 5. Embedded Linux ELF linker: hello world and static ELF binary generation
  {
    auto linux_linker = AssemblerLinkerFactory::create("linux");
    assert(linux_linker != nullptr);
    assert(linux_linker->is_supported());
    assert(std::string(linux_linker->get_arch_name()) == "linux");

    auto gen = CodeGeneratorFactory::create("linux");
    auto prog = parse("int main() {\n"
                      "  puts(\"Testing Linux ELF linking!\");\n"
                      "  return 0;\n"
                      "}\n");
    std::string asm_code = gen->generate_to_string(*prog);

    std::string elf_path = "test_linux_gen.elf";
    auto res = linux_linker->assemble_and_link(asm_code, elf_path);
    assert(res.success);
    assert(std::filesystem::exists(elf_path));

    // Verify ELF magic: 0x7F 'E' 'L' 'F'
    std::ifstream elf_file(elf_path, std::ios::binary);
    char magic[4];
    elf_file.read(magic, 4);
    assert(magic[0] == 0x7F && magic[1] == 'E' && magic[2] == 'L' && magic[3] == 'F');
    elf_file.close();

    std::error_code ec;
    std::filesystem::remove(elf_path, ec);
  }
}

static void test_inline_asm_codegen() {
  std::cout << "Testing Inline Assembly Codegen...\n";

  // 1. x86_64 inline assembly
  {
    auto gen = CodeGeneratorFactory::create("x86_64");
    auto prog = parse("int main() {\n"
                      "  __asm__ volatile (\"nop\");\n"
                      "  return 0;\n"
                      "}\n");
    std::string asm_code = gen->generate_to_string(*prog);
    assert(asm_code.find("nop") != std::string::npos);
  }

  // 2. ARM64 inline assembly
  {
    auto gen = CodeGeneratorFactory::create("arm64");
    auto prog = parse("int main() {\n"
                      "  asm(\"nop\");\n"
                      "  return 0;\n"
                      "}\n");
    std::string asm_code = gen->generate_to_string(*prog);
    assert(asm_code.find("nop") != std::string::npos);
  }

  // 3. AVR inline assembly
  {
    auto gen = CodeGeneratorFactory::create("avr");
    auto prog = parse("int main() {\n"
                      "  asm(\"nop\");\n"
                      "  return 0;\n"
                      "}\n");
    std::string asm_code = gen->generate_to_string(*prog);
    assert(asm_code.find("nop") != std::string::npos);
  }

  // 4. C backend inline assembly
  {
    auto gen = CodeGeneratorFactory::create("c");
    auto prog = parse("int main() {\n"
                      "  asm(\"nop\");\n"
                      "  return 0;\n"
                      "}\n");
    std::string c_code = gen->generate_to_string(*prog);
    assert(c_code.find("__asm__(\"nop\");") != std::string::npos);
  }

  // 5. Execution of inline assembly on x86_64
  {
    auto x86_linker = AssemblerLinkerFactory::create("x86_64");
    auto gen = CodeGeneratorFactory::create("x86_64");
    auto prog = parse("int main() {\n"
                      "  __asm__(\"nop\");\n"
                      "  return 0;\n"
                      "}\n");
    std::string asm_code = gen->generate_to_string(*prog);
    std::string exe_path = "test_asm_exe.exe";
    auto res = x86_linker->assemble_and_link(asm_code, exe_path);
    assert(res.success);
    assert(std::filesystem::exists(exe_path));

    int code = std::system(".\\test_asm_exe.exe");
    assert(code == 0);

    std::error_code ec;
    std::filesystem::remove(exe_path, ec);
  }
}

static void test_linker_libraries() {
  std::cout << "Testing Linker Library Ingestion (-l, direct DLL, and .lib archives)...\n";

  // 1. Direct DLL export parsing (kernel32.dll)
  {
    std::set<std::string> exports;
    bool ok = InternalPELinker::read_dll_exports("C:\\Windows\\System32\\kernel32.dll", exports);
    assert(ok);
    assert(exports.count("ExitProcess") > 0);
    assert(exports.count("GetStdHandle") > 0);
  }

  // 2. Direct DLL export parsing (user32.dll)
  {
    std::set<std::string> exports;
    bool ok = InternalPELinker::read_dll_exports("C:\\Windows\\System32\\user32.dll", exports);
    assert(ok);
    assert(exports.count("MessageBoxA") > 0);
  }

  // 3. Link with -luser32
  {
    auto x86_linker = AssemblerLinkerFactory::create("x86_64");
    auto gen = CodeGeneratorFactory::create("x86_64");
    auto prog = parse("int MessageBoxA(void* hwnd, char* text, char* caption, int type);\n"
                      "int main() {\n"
                      "  return 0;\n"
                      "}\n");
    std::string asm_code = gen->generate_to_string(*prog);

    LinkerConfig config;
    config.lib_names.push_back("user32");
    x86_linker->set_linker_config(config);

    std::string exe_path = "test_user32_link.exe";
    auto res = x86_linker->assemble_and_link(asm_code, exe_path);
    assert(res.success);
    assert(std::filesystem::exists(exe_path));

    int code = std::system(".\\test_user32_link.exe");
    assert(code == 0);

    std::error_code ec;
    std::filesystem::remove(exe_path, ec);
  }

  // 4. COFF Archive (.lib) parsing
  {
    std::string test_lib = "test_mock.lib";
    std::ofstream out(test_lib, std::ios::binary);
    // !<arch>\n
    out.write("!<arch>\n", 8);

    // 60-byte member header
    std::string hdr(60, ' ');
    std::string name = "test.obj/";
    std::copy(name.begin(), name.end(), hdr.begin());
    // member size: 20 bytes short import + "mock_func\0" (10) + "mock.dll\0" (9) = 39 bytes
    std::string size_str = "39        ";
    std::copy(size_str.begin(), size_str.end(), hdr.begin() + 48);
    hdr[58] = '`';
    hdr[59] = '\n';
    out.write(hdr.data(), 60);

    // Short import header: sig1=0, sig2=0xFFFF
    uint16_t sig1 = 0;
    uint16_t sig2 = 0xFFFF;
    uint16_t ver = 0;
    uint16_t machine = 0x8664;
    uint32_t timestamp = 0;
    uint32_t data_sz = 0;
    uint16_t hint = 0;
    uint16_t type = 0;
    out.write(reinterpret_cast<char *>(&sig1), 2);
    out.write(reinterpret_cast<char *>(&sig2), 2);
    out.write(reinterpret_cast<char *>(&ver), 2);
    out.write(reinterpret_cast<char *>(&machine), 2);
    out.write(reinterpret_cast<char *>(&timestamp), 4);
    out.write(reinterpret_cast<char *>(&data_sz), 4);
    out.write(reinterpret_cast<char *>(&hint), 2);
    out.write(reinterpret_cast<char *>(&type), 2);

    const char sym[] = "mock_func";
    const char dll[] = "mock.dll";
    out.write(sym, sizeof(sym));
    out.write(dll, sizeof(dll));
    out.close();

    std::unordered_map<std::string, std::string> imports;
    std::string err;
    bool ok = InternalPELinker::read_coff_archive(test_lib, imports, err);
    assert(ok);
    assert(imports["mock_func"] == "mock.dll");

    std::error_code ec;
    std::filesystem::remove(test_lib, ec);
  }

  // 5. Test real SDK kernel32.Lib if present on system
  {
    std::string sdk_lib = "C:\\Program Files (x86)\\Windows Kits\\10\\Lib\\10.0.26100.0\\um\\x64\\kernel32.Lib";
    if (std::filesystem::exists(sdk_lib)) {
      std::unordered_map<std::string, std::string> imports;
      std::string err;
      bool ok = InternalPELinker::read_coff_archive(sdk_lib, imports, err);
      assert(ok);
      assert(imports.count("ExitProcess") > 0);
    }
  }
}

static void test_calling_conventions_codegen() {
  std::cout << "Testing Calling Conventions (__cdecl, __stdcall, __fastcall) Codegen...\n";

  auto x86 = CodeGeneratorFactory::create(TargetArch::X86_64);

  // 1. __cdecl: stack passing + caller cleanup
  {
    const char *src =
        "int __cdecl add(int a, int b) {\n"
        "  return a + b;\n"
        "}\n"
        "int main() {\n"
        "  return add(2, 3);\n"
        "}\n";
    auto prog = parse(src);
    std::string asm_code = x86->generate_to_string(*prog);

    // In add: args accessed from stack
    assert(asm_code.find("16(%rbp)") != std::string::npos);
    assert(asm_code.find("24(%rbp)") != std::string::npos);
    // In add: callee does regular ret (no ret $N)
    assert(asm_code.find("ret $") == std::string::npos);
    // In main: caller cleans up 16 bytes of stack
    assert(asm_code.find("addq $16, %rsp") != std::string::npos);
  }

  // 2. __stdcall: stack passing + callee cleanup
  {
    const char *src =
        "int __stdcall sub(int a, int b) {\n"
        "  return a - b;\n"
        "}\n"
        "int main() {\n"
        "  return sub(10, 4);\n"
        "}\n";
    auto prog = parse(src);
    std::string asm_code = x86->generate_to_string(*prog);

    // In sub: args accessed from stack
    assert(asm_code.find("16(%rbp)") != std::string::npos);
    assert(asm_code.find("24(%rbp)") != std::string::npos);
    // In sub: callee cleans up 2 * 8 = 16 bytes via ret $16
    assert(asm_code.find("ret $16") != std::string::npos);
  }

  // 3. __fastcall: registers %rcx and %rdx + stack for rest + callee cleanup
  {
    const char *src =
        "int __fastcall compute(int a, int b, int c, int d) {\n"
        "  return a + b + c + d;\n"
        "}\n"
        "int main() {\n"
        "  return compute(1, 2, 3, 4);\n"
        "}\n";
    auto prog = parse(src);
    std::string asm_code = x86->generate_to_string(*prog);

    // In compute: first two in %rcx, %rdx
    assert(asm_code.find("movq %rcx, -") != std::string::npos);
    assert(asm_code.find("movq %rdx, -") != std::string::npos);
    // In compute: 3rd and 4th args from stack 16(%rbp) and 24(%rbp)
    assert(asm_code.find("16(%rbp)") != std::string::npos);
    assert(asm_code.find("24(%rbp)") != std::string::npos);
    // In compute: callee cleans up the 2 stack args (2 * 8 = 16)
    assert(asm_code.find("ret $16") != std::string::npos);
    // In main: arguments 0 and 1 loaded into %rcx, %rdx
    assert(asm_code.find("movq %rax, %rcx") != std::string::npos);
    assert(asm_code.find("movq %rax, %rdx") != std::string::npos);
  }

  // 4. C transpiler preserves calling convention
  {
    auto c_gen = CodeGeneratorFactory::create(TargetArch::C_Transpiler);
    const char *src = "int __cdecl add(int a, int b) { return a + b; }\n";
    auto prog = parse(src);
    std::string c_code = c_gen->generate_to_string(*prog);
    assert(c_code.find("__cdecl") != std::string::npos);
  }
}

static void test_local_var_storage_options() {
  std::cout << "Testing Local Variable Storage Options (Memory vs Stack)...\n";

  // 1. Check architecture defaults
  {
    auto avr = CodeGeneratorFactory::create("avr");
    assert(avr->get_effective_local_var_storage() == LocalVarStorage::Memory);

    auto x86 = CodeGeneratorFactory::create("x86_64");
    assert(x86->get_effective_local_var_storage() == LocalVarStorage::Stack);

    auto arm = CodeGeneratorFactory::create("arm64");
    assert(arm->get_effective_local_var_storage() == LocalVarStorage::Stack);

    auto c_gen = CodeGeneratorFactory::create("c");
    assert(c_gen->get_effective_local_var_storage() == LocalVarStorage::Stack);
  }

  // 2. Check explicit options via CodeGenOptions
  {
    CodeGenOptions stack_opts;
    stack_opts.local_var_storage = LocalVarStorage::Stack;
    auto avr_stack = CodeGeneratorFactory::create("avr", stack_opts);
    assert(avr_stack->get_effective_local_var_storage() == LocalVarStorage::Stack);

    CodeGenOptions mem_opts;
    mem_opts.local_var_storage = LocalVarStorage::Memory;
    auto avr_mem = CodeGeneratorFactory::create("avr", mem_opts);
    assert(avr_mem->get_effective_local_var_storage() == LocalVarStorage::Memory);
  }

  // 3. Test AVR code generation for functions and arrays in memory mode vs stack mode
  {
    const char *src =
        "int compute(int a, int b) {\n"
        "  int c = a + b;\n"
        "  int arr[3] = {10, 20, 30};\n"
        "  arr[0] = c;\n"
        "  return arr[0];\n"
        "}\n";
    auto prog = parse(src);

    // Memory mode (default)
    auto mem_gen = CodeGeneratorFactory::create("avr");
    std::string mem_asm = mem_gen->generate_to_string(*prog);

    // No Y frame setup
    assert(mem_asm.find("push r28") == std::string::npos);
    assert(mem_asm.find("sbiw r28") == std::string::npos);
    // Parameters stored directly to memory
    assert(mem_asm.find("sts __local_compute_a, r24") != std::string::npos);
    assert(mem_asm.find("sts __local_compute_b, r22") != std::string::npos);
    // Local variable c stored to memory
    assert(mem_asm.find("sts __local_compute_c, r24") != std::string::npos);
    // Array access using direct symbol address
    assert(mem_asm.find("ldi r30, lo8(__local_compute_arr)") != std::string::npos);
    assert(mem_asm.find("ldi r31, hi8(__local_compute_arr)") != std::string::npos);
    // Static memory section
    assert(mem_asm.find(".section .bss") != std::string::npos);
    assert(mem_asm.find("__local_compute_a:") != std::string::npos);
    assert(mem_asm.find("__local_compute_b:") != std::string::npos);
    assert(mem_asm.find("__local_compute_c:") != std::string::npos);
    assert(mem_asm.find("__local_compute_arr:") != std::string::npos);
    assert(mem_asm.find(".zero 6") != std::string::npos);

    // Stack mode (explicit)
    CodeGenOptions stack_opts;
    stack_opts.local_var_storage = LocalVarStorage::Stack;
    auto stack_gen = CodeGeneratorFactory::create("avr", stack_opts);
    std::string stack_asm = stack_gen->generate_to_string(*prog);

    assert(stack_asm.find("push r28") != std::string::npos);
    assert(stack_asm.find("push r29") != std::string::npos);
    assert(stack_asm.find("sbiw r28") != std::string::npos);
    assert(stack_asm.find("std Y+") != std::string::npos);
    assert(stack_asm.find("ldd r24, Y+") != std::string::npos);
  }

  // 4. Memory mode requested on x86_64 throws UnsupportedFeatureException
  {
    CodeGenOptions mem_opts;
    mem_opts.local_var_storage = LocalVarStorage::Memory;
    bool threw = false;
    try {
      CodeGeneratorFactory::create("x86_64", mem_opts);
    } catch (const UnsupportedFeatureException &e) {
      threw = true;
      assert(e.get_arch() == "x86_64");
    }
    assert(threw);
  }
}

static void test_command_line_parser() {
  std::cout << "Testing CommandLineParser and CompilerConfig...\n";

  CommandLineParser parser;
  CompilerConfig config;
  std::string err;
  bool help = false;

  // 1. Basic flags and options with spaces
  {
    std::vector<std::string> args = {
        "-t", "avr",
        "-o", "out.s",
        "-S",
        "-v",
        "--ast",
        "--local-vars", "memory",
        "-I", "include/path",
        "-D", "MAX_LEN=128",
        "-l", "m",
        "-L", "lib/dir",
        "--import", "user32.dll:MessageBoxA",
        "main.c"
    };
    bool ok = parser.parse(args, config, err, help);
    assert(ok);
    assert(!help);
    assert(config.general.target == "avr");
    assert(config.general.output_filename == "out.s");
    assert(config.general.stop_phase == StopPhase::AssembleOnly);
    assert(config.general.verbose == true);
    assert(config.general.show_ast == true);
    assert(config.codegen.local_var_storage == LocalVarStorage::Memory);
    assert(config.preprocessor.include_dirs.size() == 1 && config.preprocessor.include_dirs[0] == "include/path");
    assert(config.preprocessor.defined_macros.size() == 1 && config.preprocessor.defined_macros[0] == "MAX_LEN=128");
    assert(config.linker.lib_names.size() == 1 && config.linker.lib_names[0] == "m");
    assert(config.linker.lib_dirs.size() == 1 && config.linker.lib_dirs[0] == "lib/dir");
    assert(config.linker.explicit_imports["MessageBoxA"] == "user32.dll");
    assert(config.general.input_filename == "main.c");

    // Test CompilerConfig::print() console output
    std::ostringstream ss;
    config.print(ss);
    std::string dumped = ss.str();
    assert(dumped.find("=== Compiler Configuration ===") != std::string::npos);
    assert(dumped.find("[General]") != std::string::npos);
    assert(dumped.find("Target:         avr") != std::string::npos);
    assert(dumped.find("Stop Phase:     AssembleOnly (-S)") != std::string::npos);
    assert(dumped.find("Verbose:        true") != std::string::npos);
    assert(dumped.find("[Preprocessor]") != std::string::npos);
    assert(dumped.find("include/path") != std::string::npos);
    assert(dumped.find("MAX_LEN=128") != std::string::npos);
    assert(dumped.find("[CodeGen]") != std::string::npos);
    assert(dumped.find("Local Var Mode: memory") != std::string::npos);
    assert(dumped.find("[Linker]") != std::string::npos);
    assert(dumped.find("MessageBoxA=user32.dll") != std::string::npos);
  }

  // 2. Consistent syntax with '=' and attached options (-Ipath, -t=..., --target=..., etc.)
  {
    CompilerConfig cfg2;
    std::vector<std::string> args = {
        "--target=arm64",
        "--output=out.bin",
        "-E",
        "--local-vars=stack",
        "-Icustom/inc",
        "-DDEBUG",
        "-lkernel32",
        "-Lcustom/lib",
        "--import=user32.dll:ShowWindow",
        "custom.lib",
        "helper.a",
        "user32.dll",
        "source.c"
    };
    bool ok = parser.parse(args, cfg2, err, help);
    assert(ok);
    assert(cfg2.general.target == "arm64");
    assert(cfg2.general.output_filename == "out.bin");
    assert(cfg2.general.stop_phase == StopPhase::PreprocessOnly);
    assert(cfg2.codegen.local_var_storage == LocalVarStorage::Stack);
    assert(cfg2.preprocessor.include_dirs.size() == 1 && cfg2.preprocessor.include_dirs[0] == "custom/inc");
    assert(cfg2.preprocessor.defined_macros.size() == 1 && cfg2.preprocessor.defined_macros[0] == "DEBUG");
    assert(cfg2.linker.lib_names.size() == 1 && cfg2.linker.lib_names[0] == "kernel32");
    assert(cfg2.linker.lib_dirs.size() == 1 && cfg2.linker.lib_dirs[0] == "custom/lib");
    assert(cfg2.linker.lib_files.size() == 2 && cfg2.linker.lib_files[0] == "custom.lib" && cfg2.linker.lib_files[1] == "helper.a");
    assert(cfg2.linker.explicit_dlls.size() == 1 && cfg2.linker.explicit_dlls[0] == "user32.dll");
    assert(cfg2.general.input_filename == "source.c");
  }

  // 3. Stdin as input file "-"
  {
    CompilerConfig cfg_stdin;
    std::vector<std::string> args = {"-t=c", "-"};
    bool ok = parser.parse(args, cfg_stdin, err, help);
    assert(ok);
    assert(cfg_stdin.general.target == "c");
    assert(cfg_stdin.general.input_filename == "-");
  }

  // 4. Help flag detection
  {
    CompilerConfig cfg_help;
    std::vector<std::string> args = {"--help"};
    bool ok = parser.parse(args, cfg_help, err, help);
    assert(ok);
    assert(help == true);
  }

  // 5. Error handling: missing argument
  {
    CompilerConfig cfg_err;
    std::vector<std::string> args = {"-t"};
    bool ok = parser.parse(args, cfg_err, err, help);
    assert(!ok);
    assert(err.find("Missing argument for -t") != std::string::npos);
  }

  // 6. Error handling: unknown option
  {
    CompilerConfig cfg_err;
    std::vector<std::string> args = {"--unknown-flag", "main.c"};
    bool ok = parser.parse(args, cfg_err, err, help);
    assert(!ok);
    assert(err.find("Unknown option '--unknown-flag'") != std::string::npos);
  }

  // 7. Multiple input files supported
  {
    CompilerConfig cfg_multi;
    std::vector<std::string> args = {"file1.c", "file2.c", "file3.c"};
    bool ok = parser.parse(args, cfg_multi, err, help);
    assert(ok);
    assert(cfg_multi.general.input_filenames.size() == 3);
    assert(cfg_multi.general.input_filenames[0] == "file1.c");
    assert(cfg_multi.general.input_filenames[1] == "file2.c");
    assert(cfg_multi.general.input_filenames[2] == "file3.c");
    assert(cfg_multi.general.input_filename == "file1.c");
  }

  // 8. Compile-only (-c) and target file type options
  {
    CompilerConfig cfg_c;
    std::vector<std::string> args = {"-c", "--type=obj", "mod.c"};
    bool ok = parser.parse(args, cfg_c, err, help);
    assert(ok);
    assert(cfg_c.general.stop_phase == StopPhase::CompileOnly);
    assert(cfg_c.general.target_file_type == TargetFileType::ObjectFile);
  }

  // 10. Intermediate representation flags (--emit-ir, --ir, --show-ir)
  {
    CompilerConfig cfg_ir;
    std::vector<std::string> args = {"--emit-ir", "--show-ir", "input.c"};
    bool ok = parser.parse(args, cfg_ir, err, help);
    assert(ok);
    assert(cfg_ir.general.stop_phase == StopPhase::IROnly);
    assert(cfg_ir.general.show_ir);

    CompilerConfig cfg_ir2;
    std::vector<std::string> args2 = {"--ir", "input.c"};
    ok = parser.parse(args2, cfg_ir2, err, help);
    assert(ok);
    assert(cfg_ir2.general.stop_phase == StopPhase::IROnly);
  }

  // 11. Error handling: no input file
  {
    CompilerConfig cfg_err;
    std::vector<std::string> args = {"-t", "x86_64"};
    bool ok = parser.parse(args, cfg_err, err, help);
    assert(!ok);
    assert(err.find("No input file specified") != std::string::npos);
  }
}

static void test_multi_module_compilation_and_linking() {
  std::cout << "Testing Multi-Module Compilation and Embedded Linking...\n";

  auto x86_linker = AssemblerLinkerFactory::create(TargetArch::X86_64);
  assert(x86_linker != nullptr);

  // 1. Link two modules together and verify execution:
  //    Module A defines add(a, b)
  //    Module B defines main() calling add(15, 27) -> returns 42
  {
    auto gen = CodeGeneratorFactory::create("x86_64");

    auto prog_a = parse("int add(int a, int b) {\n"
                        "  return a + b;\n"
                        "}\n");
    std::string asm_a = gen->generate_to_string(*prog_a);

    auto prog_b = parse("int add(int a, int b);\n"
                        "int main() {\n"
                        "  return add(15, 27);\n"
                        "}\n");
    std::string asm_b = gen->generate_to_string(*prog_b);

    std::string exe_path = "test_multi_module.exe";
    auto res = x86_linker->assemble_and_link_multiple({asm_a, asm_b}, exe_path);
    assert(res.success);
    assert(std::filesystem::exists(exe_path));

    int code = std::system(".\\test_multi_module.exe");
    assert(code == 42);

    std::error_code ec;
    std::filesystem::remove(exe_path, ec);
  }

  // 2. Target type identification depending on presence of 'main'
  {
    X86_64InternalAssembler asm_obj;

    auto gen = CodeGeneratorFactory::create("x86_64");
    auto prog1 = parse("int helper1() { return 1; }\n");
    auto prog2 = parse("int helper2() { return 2; }\n");
    std::string s1 = gen->generate_to_string(*prog1);
    std::string s2 = gen->generate_to_string(*prog2);

    std::vector<AssembledModule> modules;
    modules.push_back(asm_obj.assemble(s1));
    modules.push_back(asm_obj.assemble(s2));

    // Neither has 'main' -> Target type is ObjectFile
    TargetFileType type_no_main = InternalPELinker::identify_target_type(modules);
    assert(type_no_main == TargetFileType::ObjectFile);

    // Linking without 'main' should fail gracefully with descriptive error
    std::string err;
    bool ok = InternalPELinker::link(modules, "invalid.exe", err);
    assert(!ok);
    assert(err.find("no 'main' entry point function found") != std::string::npos);

    // Add module with 'main' -> Target type is Executable
    auto prog_main = parse("int main() { return 0; }\n");
    modules.push_back(asm_obj.assemble(gen->generate_to_string(*prog_main)));
    TargetFileType type_with_main = InternalPELinker::identify_target_type(modules);
    assert(type_with_main == TargetFileType::Executable);
  }

  // 3. Static functions across modules: internal linkage, NO duplicate symbol errors
  {
    auto gen = CodeGeneratorFactory::create("x86_64");

    // Module 1 defines a static helper returning 10
    auto prog_1 = parse("static int helper() { return 10; }\n"
                        "int get_a() { return helper(); }\n");
    std::string asm_1 = gen->generate_to_string(*prog_1);

    // Module 2 defines a static helper returning 25
    auto prog_2 = parse("static int helper() { return 25; }\n"
                        "int get_b() { return helper(); }\n"
                        "int get_a();\n"
                        "int main() {\n"
                        "  return get_a() + get_b();\n" // 10 + 25 = 35
                        "}\n");
    std::string asm_2 = gen->generate_to_string(*prog_2);

    std::string exe_path = "test_static_linkage.exe";
    auto res = x86_linker->assemble_and_link_multiple({asm_1, asm_2}, exe_path);
    assert(res.success);
    assert(std::filesystem::exists(exe_path));

    int code = std::system(".\\test_static_linkage.exe");
    assert(code == 35);

    std::error_code ec;
    std::filesystem::remove(exe_path, ec);
  }

  // 4. Duplicate global symbol detection:
  //    Two modules defining the same non-static function must produce an error
  {
    auto gen = CodeGeneratorFactory::create("x86_64");

    auto prog_dup1 = parse("int conflict() { return 1; }\n");
    auto prog_dup2 = parse("int conflict() { return 2; }\n"
                           "int main() { return conflict(); }\n");
    std::string asm_dup1 = gen->generate_to_string(*prog_dup1);
    std::string asm_dup2 = gen->generate_to_string(*prog_dup2);

    std::string exe_path = "test_dup.exe";
    auto res = x86_linker->assemble_and_link_multiple({asm_dup1, asm_dup2}, exe_path);
    assert(!res.success);
    assert(res.message.find("multiple definition of symbol 'conflict'") != std::string::npos);
  }
}

static void test_debug_information_codegen() {
  std::cout << "Testing Debug Information Generation (-g / -g0 / --no-debug)...\n";

  auto prog = parse("int main() {\n"
                    "  int a = 10;\n"
                    "  int b = 20;\n"
                    "  return a + b;\n"
                    "}\n");

  // 1. x86_64 target
  {
    auto x86_gen = CodeGeneratorFactory::create("x86_64");
    x86_gen->set_source_filename("test_debug.c");
    assert(x86_gen->get_emit_debug_info() == true);

    std::string asm_dbg = x86_gen->generate_to_string(*prog);
    assert(asm_dbg.find(".file 1 \"test_debug.c\"") != std::string::npos);
    assert(asm_dbg.find(".loc 1 1 0") != std::string::npos);
    assert(asm_dbg.find(".loc 1 2 0") != std::string::npos);
    assert(asm_dbg.find(".loc 1 4 0") != std::string::npos);

    // Excluded with emit_debug_info = false (-g0)
    x86_gen->set_emit_debug_info(false);
    std::string asm_no_dbg = x86_gen->generate_to_string(*prog);
    assert(asm_no_dbg.find(".file") == std::string::npos);
    assert(asm_no_dbg.find(".loc") == std::string::npos);
  }

  // 2. AVR target
  {
    auto avr_gen = CodeGeneratorFactory::create("avr");
    avr_gen->set_source_filename("blink.c");
    assert(avr_gen->get_emit_debug_info() == true);

    std::string avr_dbg = avr_gen->generate_to_string(*prog);
    assert(avr_dbg.find(".file 1 \"blink.c\"") != std::string::npos);
    assert(avr_dbg.find(".loc 1 1 0") != std::string::npos);
    assert(avr_dbg.find(".loc 1 2 0") != std::string::npos);
    assert(avr_dbg.find(".loc 1 4 0") != std::string::npos);

    // Excluded with emit_debug_info = false (-g0)
    avr_gen->set_emit_debug_info(false);
    std::string avr_no_dbg = avr_gen->generate_to_string(*prog);
    assert(avr_no_dbg.find(".file") == std::string::npos);
    assert(avr_no_dbg.find(".loc") == std::string::npos);
  }

  // 3. ARM64 target
  {
    auto arm_gen = CodeGeneratorFactory::create("arm64");
    arm_gen->set_source_filename("arm_app.c");
    assert(arm_gen->get_emit_debug_info() == true);

    std::string arm_dbg = arm_gen->generate_to_string(*prog);
    assert(arm_dbg.find(".file 1 \"arm_app.c\"") != std::string::npos);
    assert(arm_dbg.find(".loc 1 1 0") != std::string::npos);
    assert(arm_dbg.find(".loc 1 2 0") != std::string::npos);
    assert(arm_dbg.find(".loc 1 4 0") != std::string::npos);

    // Excluded with emit_debug_info = false (-g0)
    arm_gen->set_emit_debug_info(false);
    std::string arm_no_dbg = arm_gen->generate_to_string(*prog);
    assert(arm_no_dbg.find(".file") == std::string::npos);
    assert(arm_no_dbg.find(".loc") == std::string::npos);
  }

  // 4. Command Line Parser flags (-g, -g0, --no-debug, -s, --strip)
  {
    CommandLineParser parser;
    std::string err;
    bool help = false;

    // Default: enabled
    CompilerConfig cfg1;
    assert(parser.parse({"main.c"}, cfg1, err, help));
    assert(cfg1.codegen.emit_debug_info == true);
    assert(cfg1.general.emit_debug_info == true);

    // -g0: disabled
    CompilerConfig cfg2;
    assert(parser.parse({"-g0", "main.c"}, cfg2, err, help));
    assert(cfg2.codegen.emit_debug_info == false);
    assert(cfg2.general.emit_debug_info == false);

    // --no-debug: disabled
    CompilerConfig cfg3;
    assert(parser.parse({"--no-debug", "main.c"}, cfg3, err, help));
    assert(cfg3.codegen.emit_debug_info == false);

    // -g explicit: enabled
    CompilerConfig cfg4;
    assert(parser.parse({"-g", "main.c"}, cfg4, err, help));
    assert(cfg4.codegen.emit_debug_info == true);

    // -s / --strip: disabled
    CompilerConfig cfg5;
    assert(parser.parse({"-s", "main.c"}, cfg5, err, help));
    assert(cfg5.codegen.emit_debug_info == false);
  }

  // 5. Internal Assembler .file and .loc parsing
  {
    X86_64InternalAssembler asm_eng;
    std::string sample_asm =
        "    .file 1 \"test_mod.c\"\n"
        ".globl main\n"
        "main:\n"
        "    .loc 1 10 0\n"
        "    movq $42, %rax\n"
        "    ret\n";
    AssembledModule mod = asm_eng.assemble(sample_asm);
    assert(mod.file_table.count(1) > 0);
    assert(mod.file_table[1] == "test_mod.c");
    assert(!mod.source_locs.empty());
    assert(mod.source_locs[0].line == 10);
    assert(mod.source_locs[0].file_id == 1);

    // 6. Internal PE Linker emission of DWARF sections and symbols
    std::string pe_out = "build_test_debug.exe";
    std::string err;
    LinkerConfig cfg;
    bool pe_ok = InternalPELinker::link(mod, pe_out, err, cfg);
    assert(pe_ok);
    std::ifstream pe_file(pe_out, std::ios::binary);
    assert(pe_file.is_open());
    std::vector<char> pe_bytes((std::istreambuf_iterator<char>(pe_file)), std::istreambuf_iterator<char>());
    pe_file.close();
    std::filesystem::remove(pe_out);
    std::string pe_data(pe_bytes.begin(), pe_bytes.end());
    assert(pe_data.find(".debug_line") != std::string::npos);
    assert(pe_data.find(".debug_info") != std::string::npos);
    assert(pe_data.find(".debug_abbrev") != std::string::npos);

    // Verify stripped PE linker output
    cfg.strip_debug_info = true;
    std::string pe_stripped = "build_test_stripped.exe";
    pe_ok = InternalPELinker::link(mod, pe_stripped, err, cfg);
    assert(pe_ok);
    std::ifstream pe_str_file(pe_stripped, std::ios::binary);
    assert(pe_str_file.is_open());
    std::vector<char> pe_str_bytes((std::istreambuf_iterator<char>(pe_str_file)), std::istreambuf_iterator<char>());
    pe_str_file.close();
    std::filesystem::remove(pe_stripped);
    std::string pe_str_data(pe_str_bytes.begin(), pe_str_bytes.end());
    assert(pe_str_data.find(".debug_line") == std::string::npos);
    assert(pe_str_data.find(".debug_info") == std::string::npos);

    // 7. Internal ELF Linker emission of DWARF sections
    std::string elf_out = "build_test_debug.elf";
    LinkerConfig elf_cfg;
    std::vector<AssembledModule> elf_mods = {mod};
    bool elf_ok = InternalElfLinker::link(elf_mods, elf_out, err, elf_cfg);
    assert(elf_ok);
    std::ifstream elf_file(elf_out, std::ios::binary);
    assert(elf_file.is_open());
    std::vector<char> elf_bytes((std::istreambuf_iterator<char>(elf_file)), std::istreambuf_iterator<char>());
    elf_file.close();
    std::filesystem::remove(elf_out);
    std::string elf_data(elf_bytes.begin(), elf_bytes.end());
    assert(elf_data.find(".debug_line") != std::string::npos);
    assert(elf_data.find(".debug_info") != std::string::npos);
    assert(elf_data.find(".debug_abbrev") != std::string::npos);

    // Verify stripped ELF linker output
    elf_cfg.strip_debug_info = true;
    std::string elf_stripped = "build_test_stripped.elf";
    elf_ok = InternalElfLinker::link(elf_mods, elf_stripped, err, elf_cfg);
    assert(elf_ok);
    std::ifstream elf_str_file(elf_stripped, std::ios::binary);
    assert(elf_str_file.is_open());
    std::vector<char> elf_str_bytes((std::istreambuf_iterator<char>(elf_str_file)), std::istreambuf_iterator<char>());
    elf_str_file.close();
    std::filesystem::remove(elf_stripped);
    std::string elf_str_data(elf_str_bytes.begin(), elf_str_bytes.end());
    assert(elf_str_data.find(".debug_line") == std::string::npos);
    assert(elf_str_data.find(".debug_info") == std::string::npos);
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
  test_functions_codegen();
  test_arrays_codegen();
  test_pointers_codegen();
  test_func_ptrs_codegen();
  test_struct_codegen();
  test_union_codegen();
  test_code_review_fixes();
  test_char_and_string_literals_codegen();
  test_assembler_linker();
  test_inline_asm_codegen();
  test_linker_libraries();
  test_calling_conventions_codegen();
  test_local_var_storage_options();
  test_command_line_parser();
  test_multi_module_compilation_and_linking();
  test_debug_information_codegen();

  std::cout << "========================================\n";
  std::cout << "  ALL CODEGEN TESTS PASSED! (22/22)\n";
  std::cout << "========================================\n";
  return 0;
}

