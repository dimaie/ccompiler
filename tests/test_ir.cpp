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

  std::cout << "========================================\n";
  std::cout << "  ALL IR TESTS PASSED!\n";
  std::cout << "========================================\n";
  return 0;
}
