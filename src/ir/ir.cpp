#include "ir.hpp"

const char *ir_op_str(IROpKind op) {
  switch (op) {
  case IROpKind::Nop: return "nop";
  case IROpKind::Add: return "add";
  case IROpKind::Sub: return "sub";
  case IROpKind::Mul: return "mul";
  case IROpKind::Div: return "div";
  case IROpKind::Mod: return "mod";
  case IROpKind::Neg: return "neg";
  case IROpKind::Not: return "not";
  case IROpKind::And: return "and";
  case IROpKind::Or:  return "or";
  case IROpKind::Xor: return "xor";
  case IROpKind::Shl: return "shl";
  case IROpKind::Shr: return "shr";
  case IROpKind::Eq:  return "eq";
  case IROpKind::Ne:  return "ne";
  case IROpKind::Lt:  return "lt";
  case IROpKind::Le:  return "le";
  case IROpKind::Gt:  return "gt";
  case IROpKind::Ge:  return "ge";
  case IROpKind::Copy: return "copy";
  case IROpKind::Load: return "load";
  case IROpKind::Store: return "store";
  case IROpKind::Alloca: return "alloca";
  case IROpKind::GetElementPtr: return "gep";
  case IROpKind::Label: return "label";
  case IROpKind::Jump: return "jump";
  case IROpKind::BranchIf: return "br_if";
  case IROpKind::Call: return "call";
  case IROpKind::Return: return "ret";
  }
  return "unknown";
}

std::string IROperand::to_string() const {
  switch (kind) {
  case IROperandKind::None: return "";
  case IROperandKind::Var: return "%v" + std::to_string(var_id);
  case IROperandKind::Imm: return "$" + std::to_string(imm_val);
  case IROperandKind::Label: return name;
  case IROperandKind::Global: return "@" + name;
  }
  return "";
}

void IRInstruction::print(std::ostream &out) const {
  if (op == IROpKind::Label) {
    out << dst.to_string() << ":";
  } else if (op == IROpKind::Jump) {
    out << "    jump " << dst.to_string();
  } else if (op == IROpKind::BranchIf) {
    out << "    br_if " << src1.to_string() << ", " << dst.to_string() << ", " << src2.to_string();
  } else if (op == IROpKind::Return) {
    out << "    ret";
    if (src1.kind != IROperandKind::None) {
      out << " " << src1.to_string();
    }
  } else if (op == IROpKind::Call) {
    out << "    ";
    if (dst.kind != IROperandKind::None) {
      out << dst.to_string() << " = ";
    }
    out << "call " << src1.to_string() << "(";
    for (size_t i = 0; i < args.size(); ++i) {
      if (i > 0) out << ", ";
      out << args[i].to_string();
    }
    out << ")";
  } else if (op == IROpKind::Copy) {
    out << "    " << dst.to_string() << " = " << src1.to_string();
  } else if (op == IROpKind::Load) {
    out << "    " << dst.to_string() << " = load " << src1.to_string();
  } else if (op == IROpKind::Store) {
    out << "    store " << dst.to_string() << ", " << src1.to_string();
  } else if (op == IROpKind::Alloca) {
    out << "    " << dst.to_string() << " = alloca " << src1.to_string();
  } else if (op == IROpKind::GetElementPtr) {
    out << "    " << dst.to_string() << " = gep " << src1.to_string() << ", " << src2.to_string();
  } else {
    out << "    " << dst.to_string() << " = " << ir_op_str(op) << " " << src1.to_string();
    if (src2.kind != IROperandKind::None) {
      out << ", " << src2.to_string();
    }
  }
  if (!comment.empty()) {
    out << "    ; " << comment;
  }
  out << "\n";
}

void IRFunction::print(std::ostream &out) const {
  out << (is_static ? "static " : "function ") << "@" << name << "(";
  for (size_t i = 0; i < params.size(); ++i) {
    if (i > 0) out << ", ";
    out << params[i].to_string();
  }
  if (is_variadic) {
    if (!params.empty()) out << ", ";
    out << "...";
  }
  out << ") {\n";
  for (const auto &inst : instructions) {
    inst.print(out);
  }
  out << "}\n\n";
}

void IRModule::print(std::ostream &out) const {
  if (!global_vars.empty()) {
    out << "; Global Variables\n";
    for (const auto &g : global_vars) {
      out << "global @" << g.first << " : " << g.second << " bytes\n";
    }
    out << "\n";
  }

  if (!string_literals.empty()) {
    out << "; String Literals\n";
    for (const auto &s : string_literals) {
      out << "@" << s.first << " = \"" << s.second << "\"\n";
    }
    out << "\n";
  }

  for (const auto &f : functions) {
    f.print(out);
  }
}
