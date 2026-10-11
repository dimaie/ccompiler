#ifndef CODEGEN_HPP
#define CODEGEN_HPP

#include "ast.hpp"
#include <memory>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>

enum class TargetArch {
  X86_64,
  AVR,
  ARM64,
  C_Transpiler
};

/**
 * @brief Exception thrown when a code generator encounters an AST feature or
 *        construct not supported on that target architecture.
 */
class UnsupportedFeatureException : public std::runtime_error {
public:
  UnsupportedFeatureException(const std::string &feature,
                              const std::string &arch,
                              int line = 0)
      : std::runtime_error(format_message(feature, arch, line)),
        feature_(feature), arch_(arch), line_(line) {}

  const std::string &get_feature() const { return feature_; }
  const std::string &get_arch() const { return arch_; }
  int get_line() const { return line_; }

private:
  static std::string format_message(const std::string &feature,
                                    const std::string &arch, int line) {
    std::string msg = "Feature '" + feature + "' is not supported by " + arch +
                      " code generator";
    if (line > 0) {
      msg += " (line " + std::to_string(line) + ")";
    }
    return msg;
  }

  std::string feature_;
  std::string arch_;
  int line_{0};
};

class ICodeGenerator : public ASTVisitor {
public:
  virtual ~ICodeGenerator() = default;

  virtual TargetArch get_arch() const = 0;
  virtual const char *get_arch_name() const = 0;
  virtual void generate(const ProgramNode &program, std::ostream &out) = 0;

  virtual std::string generate_to_string(const ProgramNode &program) {
    std::ostringstream ss;
    generate(program, ss);
    return ss.str();
  }

  // Helpers to throw UnsupportedFeatureException for this generator
  [[noreturn]] virtual void unsupported(const std::string &feature,
                                        int line = 0) const {
    throw UnsupportedFeatureException(feature, get_arch_name(), line);
  }

  [[noreturn]] virtual void unsupported(const ASTNode &node,
                                        const std::string &feature) const {
    throw UnsupportedFeatureException(feature, get_arch_name(), node.line);
  }

  // Default visitor implementations for AST nodes:
  // Throws UnsupportedFeatureException so new/unimplemented features
  // require zero code in unsupported backends.
  void visit(const ProgramNode &node) override;
  void visit(const CompoundNode &node) override;
  void visit(const VarDeclNode &node) override;
  void visit(const FuncDefNode &node) override;
  void visit(const CallNode &node) override;
  void visit(const ArrayAccessNode &node) override;
  void visit(const InitListNode &node) override;
  void visit(const IfNode &node) override;
  void visit(const ForNode &node) override;
  void visit(const WhileNode &node) override;
  void visit(const DoWhileNode &node) override;
  void visit(const SwitchNode &node) override;
  void visit(const CaseNode &node) override;
  void visit(const BreakNode &node) override;
  void visit(const ContinueNode &node) override;
  void visit(const GotoNode &node) override;
  void visit(const LabelNode &node) override;
  void visit(const ReturnNode &node) override;
  void visit(const AssignNode &node) override;
  void visit(const BinaryOpNode &node) override;
  void visit(const UnaryOpNode &node) override;
  void visit(const IntLitNode &node) override;
  void visit(const StringLitNode &node) override;
  void visit(const IdentifierNode &node) override;
  void visit(const StructDefNode &node) override;
  void visit(const MemberAccessNode &node) override;
  void visit(const AsmStmtNode &node) override;
};

class CodeGeneratorFactory {
public:
  static std::unique_ptr<ICodeGenerator> create(TargetArch arch);
  static std::unique_ptr<ICodeGenerator> create(const std::string &arch_name);
};

#endif // CODEGEN_HPP
