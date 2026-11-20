#include "codegen_z80.hpp"
#include <cstdint>
#include <sstream>

void Z80CodeGenerator::emit(const std::string &text) { *out_ << "    " << text << "\n"; }
void Z80CodeGenerator::label(const std::string &name) { *out_ << name << ":\n"; }
std::string Z80CodeGenerator::new_label(const std::string &prefix) {
  return "__z80_" + prefix + "_" + std::to_string(++label_counter_);
}
void Z80CodeGenerator::test_zero() { emit("ld a,h"); emit("or l"); }

int Z80CodeGenerator::size(Type type) const {
  return type.pointers || type.array || type.base != DataType::Char ? 2 : 1;
}

Z80CodeGenerator::Type Z80CodeGenerator::checked_type(
    DataType base, int pointers, const ASTNode &node) const {
  if (base != DataType::Char && base != DataType::Int &&
      base != DataType::Short && base != DataType::Void)
    unsupported(node, "type " + std::string(data_type_str(base)));
  return {base, pointers, false};
}

Z80CodeGenerator::Type Z80CodeGenerator::pointee(Type type, const ASTNode &node) const {
  if (type.array) type.array = false;
  else if (type.pointers) --type.pointers;
  else unsupported(node, "dereference of a non-pointer");
  if (!type.pointers && type.base == DataType::Void)
    unsupported(node, "void pointer arithmetic or dereference");
  return type;
}

const Z80CodeGenerator::Variable &Z80CodeGenerator::lookup(const IdentifierNode &node) const {
  for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
    auto it = scope->find(node.name);
    if (it != scope->end()) return it->second;
  }
  auto it = globals_.find(node.name);
  if (it != globals_.end()) return it->second;
  throw std::runtime_error("Unknown Z80 variable '" + node.name +
                           "' (line " + std::to_string(node.line) + ")");
}

Z80CodeGenerator::Type Z80CodeGenerator::expression_type(const ASTNode &node) const {
  if (auto id = dynamic_cast<const IdentifierNode *>(&node)) return lookup(*id).type;
  if (auto access = dynamic_cast<const ArrayAccessNode *>(&node))
    return pointee(expression_type(*access->array), node);
  if (auto unary = dynamic_cast<const UnaryOpNode *>(&node)) {
    Type type = expression_type(*unary->expr);
    if (unary->op == TOK_STAR) return pointee(type, node);
    if (unary->op == TOK_AMP) {
      if (type.array) unsupported(node, "pointer to array");
      ++type.pointers;
      return type;
    }
    if (unary->op == TOK_INC || unary->op == TOK_DEC) return type;
  }
  if (auto assign = dynamic_cast<const AssignNode *>(&node)) return expression_type(*assign->left);
  if (auto binary = dynamic_cast<const BinaryOpNode *>(&node)) {
    if (binary->op == TOK_PLUS || binary->op == TOK_MINUS) {
      Type left = expression_type(*binary->left), right = expression_type(*binary->right);
      if (left.pointers || left.array) {
        left.pointers += left.array ? 1 : 0;
        left.array = false;
        return left;
      }
      if (binary->op == TOK_PLUS && (right.pointers || right.array)) {
        right.pointers += right.array ? 1 : 0;
        right.array = false;
        return right;
      }
    }
  }
  if (auto call = dynamic_cast<const CallNode *>(&node)) {
    auto it = functions_.find(call->callee);
    if (it != functions_.end())
      return {it->second->return_type, it->second->return_pointer_level, false};
  }
  if (dynamic_cast<const StringLitNode *>(&node)) return {DataType::Char, 1, false};
  if (auto ternary = dynamic_cast<const TernaryOpNode *>(&node)) {
    Type a = expression_type(*ternary->then_expr), b = expression_type(*ternary->else_expr);
    if (a.pointers || a.array) return {a.base, a.pointers + (a.array ? 1 : 0), false};
    if (b.pointers || b.array) return {b.base, b.pointers + (b.array ? 1 : 0), false};
  }
  return {};
}

void Z80CodeGenerator::address(const Variable &variable) {
  if (!variable.symbol.empty()) emit("ld hl," + variable.symbol);
  else {
    emit("push ix"); emit("pop hl");
    emit("ld de," + std::to_string(variable.offset)); emit("add hl,de");
  }
}

void Z80CodeGenerator::address(const ASTNode &node) {
  if (auto id = dynamic_cast<const IdentifierNode *>(&node)) {
    address(lookup(*id));
  } else if (auto access = dynamic_cast<const ArrayAccessNode *>(&node)) {
    Type element = pointee(expression_type(*access->array), node);
    access->array->accept(*this);
    emit("push hl");
    access->index->accept(*this);
    if (size(element) == 2) emit("add hl,hl");
    emit("pop de"); emit("add hl,de");
  } else if (auto unary = dynamic_cast<const UnaryOpNode *>(&node);
             unary && unary->op == TOK_STAR) {
    pointee(expression_type(*unary->expr), node);
    unary->expr->accept(*this);
  } else unsupported(node, "lvalue");
}

void Z80CodeGenerator::normalize(Type type) {
  if (size(type) == 1) {
    emit("ld a,l"); emit("add a,a"); emit("sbc a,a"); emit("ld h,a");
  }
}

void Z80CodeGenerator::negate() {
  emit("xor a"); emit("sub l"); emit("ld l,a");
  emit("sbc a,a"); emit("sub h"); emit("ld h,a");
}

uint16_t Z80CodeGenerator::literal_constant(const ASTNode &node) const {
  const ASTNode *value = &node;
  int sign = 1;
  if (auto unary = dynamic_cast<const UnaryOpNode *>(value)) {
    if (unary->op != TOK_MINUS && unary->op != TOK_PLUS)
      unsupported(node, "non-literal constant initializer or case");
    sign = unary->op == TOK_MINUS ? -1 : 1;
    value = unary->expr.get();
  }
  auto literal = dynamic_cast<const IntLitNode *>(value);
  if (!literal) unsupported(node, "non-literal constant initializer or case");
  if (literal->val < -32768 || literal->val > 65535 ||
      sign * literal->val < -32768 || sign * literal->val > 65535)
    unsupported(node, "integer literal wider than 16 bits");
  return static_cast<uint16_t>(sign * literal->val);
}

void Z80CodeGenerator::load(Type type) {
  emit("ld a,(hl)");
  if (size(type) == 2) { emit("inc hl"); emit("ld h,(hl)"); }
  emit("ld l,a");
  normalize(type);
}

// DE is the destination, HL is both the stored value and assignment result.
void Z80CodeGenerator::store(Type type) {
  emit("ld a,l"); emit("ld (de),a");
  if (size(type) == 2) { emit("inc de"); emit("ld a,h"); emit("ld (de),a"); }
  normalize(type);
}

void Z80CodeGenerator::branch_zero(const ASTNode &expr, const std::string &destination) {
  expr.accept(*this); test_zero(); emit("jp z," + destination);
}

void Z80CodeGenerator::generate(const ProgramNode &program, std::ostream &out) {
  out_ = &out;
  globals_.clear(); scopes_.clear(); functions_.clear(); strings_.clear();
  breaks_.clear(); continues_.clear();
  function_.clear(); frame_size_ = 0; label_counter_ = 0;
  program.accept(*this);
}

void Z80CodeGenerator::visit(const ProgramNode &node) {
  for (const auto &statement : node.statements) {
    if (auto fn = dynamic_cast<const FuncDefNode *>(statement.get())) {
      checked_type(fn->return_type, fn->return_pointer_level, *fn);
      if (fn->is_variadic || (fn->calling_convention != CallingConvention::Default &&
                              fn->calling_convention != CallingConvention::Cdecl))
        unsupported(*fn, "variadic or non-cdecl function");
      for (const auto &param : fn->params) {
        checked_type(param.type, param.pointer_level, *fn);
        if (param.is_func_ptr) unsupported(*fn, "function pointer parameter");
      }
      auto existing = functions_.find(fn->name);
      if (existing == functions_.end() || fn->body) functions_[fn->name] = fn;
    } else if (auto var = dynamic_cast<const VarDeclNode *>(statement.get())) {
      Type type = checked_type(var->data_type, var->pointer_level, *var);
      if (type.base == DataType::Void && !type.pointers) unsupported(*var, "void variable");
      type.array = var->is_array;
      if (var->is_func_ptr) unsupported(*var, "function pointer");
      globals_[var->name] = {type, 0, "_g_" + var->name};
    } else unsupported(*statement, "top-level statement outside a function");
  }
  *out_ << "; ZX Spectrum 48K, sjasmplus syntax\n";
  emit("DEVICE ZXSPECTRUM48"); emit("ORG 32768");
  auto main = functions_.find("main");
  if (main != functions_.end() && main->second->body) {
    if (!main->second->params.empty()) unsupported(*main->second, "main parameters");
    label("__z80_start");
    emit("call _fn_main");
    emit("ld b,h"); emit("ld c,l"); emit("ret");
  }
  for (const auto &statement : node.statements)
    if (auto fn = dynamic_cast<const FuncDefNode *>(statement.get())) fn->accept(*this);
  for (const auto &statement : node.statements)
    if (auto var = dynamic_cast<const VarDeclNode *>(statement.get())) emit_global(*var);
  for (const auto &string : strings_) {
    label(string.first);
    std::string bytes = "db ";
    for (unsigned char ch : string.second) bytes += std::to_string(ch) + ",";
    emit(bytes + "0");
  }
}

void Z80CodeGenerator::emit_global(const VarDeclNode &node) {
  if (node.is_extern || node.storage_class == StorageClass::Extern) return;
  Type type = globals_.at(node.name).type;
  type.array = false;
  int count = node.is_array ? node.array_size : 1;
  if (count <= 0 || count > 32767 / size(type))
    unsupported(node, "invalid or oversized array");
  label(globals_.at(node.name).symbol);
  if (node.init) {
    auto list = dynamic_cast<const InitListNode *>(node.init.get());
    if (node.is_array && !list) unsupported(node, "non-list array initializer");
    int initialized = list ? static_cast<int>(list->elements.size()) : 1;
    if (initialized > count) unsupported(node, "too many array initializers");
    for (int i = 0; i < initialized; ++i) {
      auto bits = literal_constant(list ? *list->elements[i] : *node.init);
      emit(std::string(size(type) == 1 ? "db " : "dw ") +
           std::to_string(size(type) == 1 ? bits & 255 : bits));
    }
    if (initialized < count)
      emit("defs " + std::to_string((count - initialized) * size(type)) + ",0");
  } else {
    emit("defs " + std::to_string(count * size(type)) + ",0");
  }
}

void Z80CodeGenerator::visit(const FuncDefNode &node) {
  if (!node.body) return;
  function_ = node.name; frame_size_ = 0;
  return_type_ = checked_type(node.return_type, node.return_pointer_level, node);
  return_label_ = new_label("return");
  scopes_.clear(); scopes_.emplace_back();
  int offset = 4; // Saved IX and return address precede the first argument.
  for (const auto &param : node.params) {
    scopes_.back()[param.name] = {checked_type(param.type, param.pointer_level, node), offset, ""};
    offset += 2;
  }
  std::ostringstream body;
  auto output = out_;
  out_ = &body;
  node.body->accept(*this);
  emit("ld hl,0");
  out_ = output;
  label("_fn_" + node.name);
  emit("push ix"); emit("ld ix,0"); emit("add ix,sp");
  if (frame_size_) {
    emit("ld hl," + std::to_string(-frame_size_)); emit("add hl,sp"); emit("ld sp,hl");
  }
  *out_ << body.str();
  label(return_label_);
  emit("ld sp,ix"); emit("pop ix"); emit("ret");
  scopes_.clear(); function_.clear();
}

void Z80CodeGenerator::visit(const CompoundNode &node) {
  scopes_.emplace_back();
  for (const auto &statement : node.statements) statement->accept(*this);
  scopes_.pop_back();
}

void Z80CodeGenerator::visit(const VarDeclNode &node) {
  if (node.is_static || node.is_extern || node.storage_class == StorageClass::Static ||
      node.storage_class == StorageClass::Extern || node.is_func_ptr)
    unsupported(node, "static/extern local or function pointer");
  Type type = checked_type(node.data_type, node.pointer_level, node);
  if (type.base == DataType::Void && !type.pointers) unsupported(node, "void variable");
  int count = node.is_array ? node.array_size : 1;
  if (count <= 0 || count > (16384 - frame_size_) / size(type))
    unsupported(node, "stack frame larger than 16384 bytes or invalid array");
  frame_size_ += count * size(type);
  type.array = node.is_array;
  Variable variable{type, -frame_size_, ""};
  if (!scopes_.back().emplace(node.name, variable).second)
    throw std::runtime_error("Duplicate Z80 local variable '" + node.name + "'");
  if (node.init) {
    auto list = dynamic_cast<const InitListNode *>(node.init.get());
    if (node.is_array && !list) unsupported(node, "non-list array initializer");
    if (list && static_cast<int>(list->elements.size()) > count)
      unsupported(node, "too many array initializers");
    type.array = false;
    for (int i = 0; i < count; ++i) {
      Variable element{type, variable.offset + i * size(type), ""};
      address(element); emit("push hl");
      if (list) {
        if (i < static_cast<int>(list->elements.size())) list->elements[i]->accept(*this);
        else emit("ld hl,0");
      } else node.init->accept(*this);
      emit("pop de"); store(type);
    }
  }
}

void Z80CodeGenerator::visit(const IdentifierNode &node) {
  const auto &variable = lookup(node);
  address(variable);
  if (!variable.type.array) load(variable.type);
}
void Z80CodeGenerator::visit(const IntLitNode &node) {
  if (node.val < -32768 || node.val > 65535) unsupported(node, "integer literal wider than 16 bits");
  emit("ld hl," + std::to_string(static_cast<uint16_t>(node.val)));
}
void Z80CodeGenerator::visit(const StringLitNode &node) {
  std::string symbol = new_label("string");
  strings_.emplace_back(symbol, node.value); emit("ld hl," + symbol);
}
void Z80CodeGenerator::visit(const ArrayAccessNode &node) {
  address(node); load(expression_type(node));
}
void Z80CodeGenerator::visit(const AssignNode &node) {
  Type type = expression_type(*node.left);
  if (type.array) unsupported(node, "array assignment");
  address(*node.left); emit("push hl");
  node.right->accept(*this); emit("pop de"); store(type);
}

void Z80CodeGenerator::visit(const CallNode &node) {
  if (node.callee_expr) unsupported(node, "indirect function call");
  auto fn = functions_.find(node.callee);
  if (fn == functions_.end())
    throw std::runtime_error("Undeclared Z80 function '" + node.callee + "'");
  if (node.args.size() != fn->second->params.size()) unsupported(node, "argument count mismatch");
  for (auto it = node.args.rbegin(); it != node.args.rend(); ++it) {
    (*it)->accept(*this); emit("push hl");
  }
  emit("call _fn_" + node.callee);
  for (size_t i = 0; i < node.args.size(); ++i) emit("pop de");
  normalize(expression_type(node));
}

void Z80CodeGenerator::visit(const ReturnNode &node) {
  if (node.expr) { node.expr->accept(*this); normalize(return_type_); }
  emit("jp " + return_label_);
}
void Z80CodeGenerator::visit(const IfNode &node) {
  auto otherwise = new_label("else"), end = new_label("endif");
  branch_zero(*node.cond, otherwise);
  node.then_branch->accept(*this); emit("jp " + end); label(otherwise);
  if (node.else_branch) node.else_branch->accept(*this);
  label(end);
}
void Z80CodeGenerator::visit(const TernaryOpNode &node) {
  auto otherwise = new_label("ternary_else"), end = new_label("ternary_end");
  branch_zero(*node.cond, otherwise);
  node.then_expr->accept(*this); emit("jp " + end); label(otherwise);
  node.else_expr->accept(*this); label(end);
}
void Z80CodeGenerator::visit(const ForNode &node) {
  auto cond = new_label("for_cond"), step = new_label("for_step"), end = new_label("for_end");
  scopes_.emplace_back();
  breaks_.push_back(end); continues_.push_back(step);
  if (node.init) node.init->accept(*this);
  label(cond);
  if (node.cond) branch_zero(*node.cond, end);
  node.body->accept(*this); label(step);
  if (node.step) node.step->accept(*this);
  emit("jp " + cond); label(end);
  breaks_.pop_back(); continues_.pop_back(); scopes_.pop_back();
}
void Z80CodeGenerator::visit(const WhileNode &node) {
  auto cond = new_label("while_cond"), end = new_label("while_end");
  breaks_.push_back(end); continues_.push_back(cond);
  label(cond); branch_zero(*node.cond, end);
  node.body->accept(*this); emit("jp " + cond); label(end);
  breaks_.pop_back(); continues_.pop_back();
}
void Z80CodeGenerator::visit(const DoWhileNode &node) {
  auto body = new_label("do_body"), cond = new_label("do_cond"), end = new_label("do_end");
  breaks_.push_back(end); continues_.push_back(cond);
  label(body); node.body->accept(*this); label(cond);
  branch_zero(*node.cond, end); emit("jp " + body); label(end);
  breaks_.pop_back(); continues_.pop_back();
}
void Z80CodeGenerator::visit(const SwitchNode &node) {
  auto end = new_label("switch_end");
  std::string otherwise = end;
  std::vector<std::string> cases;
  for (const auto &item : node.cases) {
    cases.push_back(new_label("case"));
    if (item->is_default) otherwise = cases.back();
  }
  node.cond->accept(*this);
  for (size_t i = 0; i < node.cases.size(); ++i) {
    if (node.cases[i]->is_default) continue;
    emit("push hl");
    emit("ld de," + std::to_string(literal_constant(*node.cases[i]->value)));
    emit("or a"); emit("sbc hl,de"); emit("pop hl"); emit("jp z," + cases[i]);
  }
  emit("jp " + otherwise);
  scopes_.emplace_back(); breaks_.push_back(end);
  for (size_t i = 0; i < node.cases.size(); ++i) {
    label(cases[i]);
    for (const auto &statement : node.cases[i]->statements) statement->accept(*this);
  }
  label(end); breaks_.pop_back(); scopes_.pop_back();
}
void Z80CodeGenerator::visit(const BreakNode &node) {
  if (breaks_.empty()) unsupported(node, "break outside loop or switch");
  emit("jp " + breaks_.back());
}
void Z80CodeGenerator::visit(const ContinueNode &node) {
  if (continues_.empty()) unsupported(node, "continue outside loop");
  emit("jp " + continues_.back());
}
void Z80CodeGenerator::visit(const GotoNode &node) { emit("jp _label_" + function_ + "_" + node.label); }
void Z80CodeGenerator::visit(const LabelNode &node) { label("_label_" + function_ + "_" + node.name); }
void Z80CodeGenerator::visit(const AsmStmtNode &node) {
  std::istringstream lines(node.asm_code);
  std::string line;
  while (std::getline(lines, line)) {
    auto start = line.find_first_not_of(" \t");
    auto end = start == std::string::npos ? std::string::npos : line.find_first_of(" \t:;", start);
    if (end != std::string::npos && line[end] == ':') *out_ << line.substr(start) << "\n";
    else emit(line);
  }
}

void Z80CodeGenerator::visit(const UnaryOpNode &node) {
  Type type = expression_type(*node.expr);
  if (node.op == TOK_AMP) { expression_type(node); address(*node.expr); return; }
  if (node.op == TOK_STAR) { address(node); load(expression_type(node)); return; }
  if (node.op == TOK_INC || node.op == TOK_DEC) {
    if (type.array) unsupported(node, "array increment");
    address(*node.expr); emit("push hl"); load(type);
    if (node.is_postfix) emit("push hl");
    int step = type.pointers ? size(pointee(type, node)) : 1;
    for (int i = 0; i < step; ++i) emit(node.op == TOK_INC ? "inc hl" : "dec hl");
    if (node.is_postfix) { emit("pop bc"); emit("pop de"); store(type); emit("ld h,b"); emit("ld l,c"); }
    else { emit("pop de"); store(type); }
    return;
  }
  if (type.pointers || type.array) {
    if (node.op != TOK_LOGIC_NOT) unsupported(node, "unary arithmetic on pointer");
  }
  node.expr->accept(*this);
  switch (node.op) {
  case TOK_PLUS: break;
  case TOK_MINUS:
    negate(); break;
  case TOK_TILDE:
    emit("ld a,h"); emit("cpl"); emit("ld h,a"); emit("ld a,l"); emit("cpl"); emit("ld l,a"); break;
  case TOK_LOGIC_NOT: {
    auto end = new_label("not_end");
    test_zero(); emit("ld hl,0"); emit("jp nz," + end); emit("inc hl"); label(end); break;
  }
  default: unsupported(node, "unary operator " + std::string(token_op_str(node.op)));
  }
}

void Z80CodeGenerator::visit(const BinaryOpNode &node) {
  if (node.op == TOK_LOGIC_AND || node.op == TOK_LOGIC_OR) {
    auto shortcut = new_label("logic_short"), end = new_label("logic_end");
    node.left->accept(*this); test_zero();
    emit(std::string(node.op == TOK_LOGIC_AND ? "jp z," : "jp nz,") + shortcut);
    node.right->accept(*this); test_zero();
    emit(std::string(node.op == TOK_LOGIC_AND ? "jp z," : "jp nz,") + shortcut);
    emit(std::string("ld hl,") + (node.op == TOK_LOGIC_AND ? "1" : "0"));
    emit("jp " + end); label(shortcut);
    emit(std::string("ld hl,") + (node.op == TOK_LOGIC_AND ? "0" : "1")); label(end);
    return;
  }
  Type left = expression_type(*node.left), right = expression_type(*node.right);
  bool lp = left.pointers || left.array, rp = right.pointers || right.array;
  bool comparison = node.op == TOK_EQUAL || node.op == TOK_NOT_EQUAL ||
                    node.op == TOK_LESS || node.op == TOK_LESS_EQ ||
                    node.op == TOK_GREATER || node.op == TOK_GREATER_EQ;
  if ((lp || rp) && !comparison &&
      !((node.op == TOK_PLUS && lp != rp) || (node.op == TOK_MINUS && lp && !rp)))
    unsupported(node, "pointer operator " + std::string(token_op_str(node.op)));
  node.left->accept(*this);
  if (rp && node.op == TOK_PLUS && size(pointee(right, node)) == 2) emit("add hl,hl");
  emit("push hl"); node.right->accept(*this);
  if (lp && !comparison && size(pointee(left, node)) == 2) emit("add hl,hl");
  emit("ex de,hl"); emit("pop hl"); // HL = left, DE = right.
  switch (node.op) {
  case TOK_PLUS: emit("add hl,de"); break;
  case TOK_MINUS: emit("or a"); emit("sbc hl,de"); break;
  case TOK_AMP: case TOK_PIPE: case TOK_CARET: {
    std::string op = node.op == TOK_AMP ? "and" : node.op == TOK_PIPE ? "or" : "xor";
    emit("ld a,h"); emit(op + " d"); emit("ld h,a");
    emit("ld a,l"); emit(op + " e"); emit("ld l,a"); break;
  }
  case TOK_STAR: {
    auto loop = new_label("mul"), skip = new_label("mul_skip");
    emit("ld b,h"); emit("ld c,l"); emit("ld hl,0"); emit("ld a,16");
    label(loop); emit("srl d"); emit("rr e"); emit("jp nc," + skip);
    emit("add hl,bc"); label(skip); emit("sla c"); emit("rl b");
    emit("dec a"); emit("jp nz," + loop); break;
  }
  case TOK_SLASH: case TOK_PERCENT: {
    auto positive_left = new_label("div_left"), positive_right = new_label("div_right");
    auto loop = new_label("div_loop"), fits = new_label("div_fits");
    auto next = new_label("div_next"), end = new_label("div_end");
    emit("ld a,h"); emit("xor d"); emit("and 128"); emit("push af");
    emit("ld a,h"); emit("and 128"); emit("push af");
    emit("bit 7,h"); emit("jp z," + positive_left); negate(); label(positive_left);
    emit("bit 7,d"); emit("jp z," + positive_right);
    emit("ex de,hl"); negate(); emit("ex de,hl"); label(positive_right);
    emit("ld b,h"); emit("ld c,l"); emit("ld hl,0"); emit("ld a,16");
    label(loop); emit("sla c"); emit("rl b"); emit("adc hl,hl");
    emit("or a"); emit("sbc hl,de"); emit("jp nc," + fits);
    emit("add hl,de"); emit("jp " + next);
    label(fits); emit("inc c"); label(next); emit("dec a"); emit("jp nz," + loop);
    emit("pop af");
    if (node.op == TOK_SLASH) {
      emit("pop af"); emit("ld h,b"); emit("ld l,c");
    } else {
      emit("ld b,a"); emit("pop af"); emit("ld a,b");
    }
    emit("or a"); emit("jp z," + end); negate(); label(end); break;
  }
  case TOK_LSHIFT: case TOK_RSHIFT: {
    auto loop = new_label("shift"), end = new_label("shift_end");
    emit("ld a,e"); emit("and 15"); emit("jp z," + end); label(loop);
    if (node.op == TOK_LSHIFT) emit("add hl,hl");
    else { emit("sra h"); emit("rr l"); }
    emit("dec a"); emit("jp nz," + loop); label(end); break;
  }
  default: {
    if (!comparison) unsupported(node, "binary operator " + std::string(token_op_str(node.op)));
    auto yes = new_label("compare_true"), end = new_label("compare_end");
    bool reverse = node.op == TOK_GREATER || node.op == TOK_LESS_EQ;
    if (reverse) emit("ex de,hl");
    if (!lp && !rp && node.op != TOK_EQUAL && node.op != TOK_NOT_EQUAL) {
      // Bias signed operands so an unsigned carry comparison handles overflow.
      emit("ld a,h"); emit("xor 128"); emit("ld h,a");
      emit("ld a,d"); emit("xor 128"); emit("ld d,a");
    }
    emit("or a"); emit("sbc hl,de"); emit("ld hl,0");
    std::string condition = node.op == TOK_EQUAL ? "z" : node.op == TOK_NOT_EQUAL ? "nz" :
                            node.op == TOK_GREATER_EQ || node.op == TOK_LESS_EQ ? "nc" : "c";
    emit("jp " + condition + "," + yes); emit("jp " + end);
    label(yes); emit("inc hl"); label(end); break;
  }
  }
}
