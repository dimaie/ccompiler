#include "ir_builder.hpp"
#include "lexer.hpp"
#include "parser.hpp"
#include "target_info.hpp"
#include <cassert>
#include <iostream>
#include <sstream>

static std::unique_ptr<ProgramNode> parse(const char *source) {
  Lexer lexer;
  lexer.init(source);
  Parser parser(&lexer);
  return parser.parse_program();
}

static void test_target_info() {
  std::cout << "Testing TargetInfo...\n";

  auto x86 = TargetInfo::get(TargetArch::X86_64);
  assert(x86.is_64bit);
  assert(x86.int_size == 4);
  assert(x86.long_size == 8);
  assert(x86.pointer_size == 8);

  auto avr = TargetInfo::get(TargetArch::AVR);
  assert(!avr.is_64bit);
  assert(avr.int_size == 2);
  assert(avr.long_size == 4);
  assert(avr.pointer_size == 2);

  auto arm = TargetInfo::get(TargetArch::ARM64);
  assert(arm.is_64bit);
  assert(arm.pointer_size == 8);

  auto from_str_x86 = TargetInfo::from_string("x86_64");
  assert(from_str_x86.arch == TargetArch::X86_64);
  assert(from_str_x86.pointer_size == 8);

  auto from_str_avr = TargetInfo::from_string("avr");
  assert(from_str_avr.arch == TargetArch::AVR);
  assert(from_str_avr.pointer_size == 2);

  auto from_str_arm = TargetInfo::from_string("arm64");
  assert(from_str_arm.arch == TargetArch::ARM64);
}

static void test_ir_expression_lowering() {
  std::cout << "Testing IR Expression Lowering...\n";

  auto prog = parse("int main() {\n"
                    "  int a = 10;\n"
                    "  int b = a + 20;\n"
                    "  return b * 2;\n"
                    "}\n");
  IRBuilder builder;
  IRModule ir_mod = builder.build(*prog);

  assert(ir_mod.functions.size() == 1);
  const auto &fn = ir_mod.functions[0];
  assert(fn.name == "main");

  // Verify that Add, Mul, and Return IR instructions exist
  bool has_add = false;
  bool has_mul = false;
  bool has_ret = false;

  for (const auto &inst : fn.instructions) {
    if (inst.op == IROpKind::Add) has_add = true;
    if (inst.op == IROpKind::Mul) has_mul = true;
    if (inst.op == IROpKind::Return) has_ret = true;
  }

  assert(has_add);
  assert(has_mul);
  assert(has_ret);
}

static void test_ir_control_flow_lowering() {
  std::cout << "Testing IR Control Flow Lowering...\n";

  auto prog = parse("int test(int x) {\n"
                    "  if (x > 0) {\n"
                    "    return 1;\n"
                    "  } else {\n"
                    "    return 0;\n"
                    "  }\n"
                    "}\n");
  IRBuilder builder;
  IRModule ir_mod = builder.build(*prog);

  assert(ir_mod.functions.size() == 1);
  const auto &fn = ir_mod.functions[0];
  assert(fn.name == "test");

  bool has_br_if = false;
  bool has_gt = false;
  int label_count = 0;

  for (const auto &inst : fn.instructions) {
    if (inst.op == IROpKind::BranchIf) has_br_if = true;
    if (inst.op == IROpKind::Gt) has_gt = true;
    if (inst.op == IROpKind::Label) label_count++;
  }

  assert(has_br_if);
  assert(has_gt);
  assert(label_count >= 2);
}

static void test_ir_short_circuit_lowering() {
  std::cout << "Testing IR Short-Circuit Logic Lowering...\n";

  auto prog = parse("int check(int a, int b) {\n"
                    "  return a && b;\n"
                    "}\n");
  IRBuilder builder;
  IRModule ir_mod = builder.build(*prog);

  assert(ir_mod.functions.size() == 1);
  const auto &fn = ir_mod.functions[0];

  bool has_br_if = false;
  for (const auto &inst : fn.instructions) {
    if (inst.op == IROpKind::BranchIf) has_br_if = true;
  }
  assert(has_br_if);
}

static void test_ir_pretty_printing() {
  std::cout << "Testing IR Pretty Printing...\n";

  auto prog = parse("int add(int a, int b) { return a + b; }\n");
  IRBuilder builder;
  IRModule ir_mod = builder.build(*prog);

  std::ostringstream ss;
  ir_mod.print(ss);
  std::string output = ss.str();

  assert(output.find("function @add") != std::string::npos);
  assert(output.find("add") != std::string::npos);
  assert(output.find("ret") != std::string::npos);

  // Variadic function IR
  auto vprog = parse("int sum(int count, ...) { return count; }\n");
  IRModule vir_mod = builder.build(*vprog);
  assert(vir_mod.functions.size() == 1);
  assert(vir_mod.functions[0].is_variadic);
  std::ostringstream vss;
  vir_mod.print(vss);
  std::string vout = vss.str();
  assert(vout.find("function @sum") != std::string::npos);
  assert(vout.find("...") != std::string::npos);
}

static void test_ir_local_var_storage_modes() {
  std::cout << "Testing IR Local Variable Storage Modes (Stack vs Memory)...\n";

  auto prog = parse("int main() {\n"
                    "  int x = 42;\n"
                    "  x = x + 1;\n"
                    "  return x;\n"
                    "}\n");

  // 1. Stack mode (e.g. x86_64 default, or explicit --local-vars stack)
  {
    TargetInfo stack_target = TargetInfo::from_string("x86_64", LocalVarStorage::Stack);
    assert(!stack_target.is_memory_mode());
    IRBuilder builder(stack_target);
    IRModule mod = builder.build(*prog);

    assert(mod.global_vars.empty()); // No static SRAM variables
    std::ostringstream ss;
    mod.print(ss);
    std::string out = ss.str();
    assert(out.find("global @_static_main_x") == std::string::npos);
    assert(out.find("ret") != std::string::npos);
  }

  // 2. Memory mode (e.g. AVR default, or explicit --local-vars memory)
  {
    TargetInfo mem_target = TargetInfo::from_string("avr", LocalVarStorage::Memory);
    assert(mem_target.is_memory_mode());
    IRBuilder builder(mem_target);
    IRModule mod = builder.build(*prog);

    assert(!mod.global_vars.empty());
    assert(mod.global_vars[0].first == "_static_main_x");
    assert(mod.global_vars[0].second == 2); // 16-bit int on AVR = 2 bytes

    std::ostringstream ss;
    mod.print(ss);
    std::string out = ss.str();
    assert(out.find("global @_static_main_x : 2 bytes") != std::string::npos);
    assert(out.find("store @_static_main_x") != std::string::npos);
    assert(out.find("load @_static_main_x") != std::string::npos);
  }
}

static void test_ir_ternary_lowering() {
  std::cout << "Testing IR Ternary Lowering...\n";

  auto prog = parse("int main() {\n"
                    "  int a = 10;\n"
                    "  int b = a > 5 ? 1 : 2;\n"
                    "  return b;\n"
                    "}\n");
  IRBuilder builder(TargetInfo::get(TargetArch::X86_64));
  IRModule mod = builder.build(*prog);

  std::ostringstream ss;
  mod.print(ss);
  std::string out = ss.str();

  // Verify that BranchIf, labels, and jumps are generated for the ternary expression
  assert(out.find("br_if") != std::string::npos);
  assert(out.find("tern_then") != std::string::npos);
  assert(out.find("tern_else") != std::string::npos);
  assert(out.find("tern_end") != std::string::npos);
  assert(out.find("jump") != std::string::npos);
}

static void test_ir_inc_dec_and_compound() {
  std::cout << "Testing IR Inc/Dec and Compound Assignment Lowering...\n";

  auto prog = parse("int main() {\n"
                    "  int a = 10;\n"
                    "  a++;\n"
                    "  ++a;\n"
                    "  a += 5;\n"
                    "  return a;\n"
                    "}\n");
  IRBuilder builder(TargetInfo::get(TargetArch::X86_64));
  IRModule mod = builder.build(*prog);

  std::ostringstream ss;
  mod.print(ss);
  std::string out = ss.str();

  assert(out.find("add") != std::string::npos);
}

static void test_ir_register_storage_class() {
  std::cout << "Testing IR Lowering with 'register' Storage Class...\n";

  auto prog = parse("int sum(register int a, register int b) {\n"
                    "  register int total = a + b;\n"
                    "  return total;\n"
                    "}\n");
  IRBuilder builder(TargetInfo::get(TargetArch::X86_64));
  IRModule mod = builder.build(*prog);

  std::ostringstream ss;
  mod.print(ss);
  std::string out = ss.str();

  assert(out.find("function @sum") != std::string::npos);
  assert(out.find("add") != std::string::npos);
  assert(out.find("ret") != std::string::npos);
}

int main() {
  std::cout << "========================================\n";
  std::cout << "  Running Intermediate Stage (IR) Tests\n";
  std::cout << "========================================\n";

  test_target_info();
  test_ir_expression_lowering();
  test_ir_control_flow_lowering();
  test_ir_short_circuit_lowering();
  test_ir_pretty_printing();
  test_ir_local_var_storage_modes();
  test_ir_ternary_lowering();
  test_ir_inc_dec_and_compound();
  test_ir_register_storage_class();

  std::cout << "========================================\n";
  std::cout << "  ALL IR TESTS PASSED!\n";
  std::cout << "========================================\n";
  return 0;
}
