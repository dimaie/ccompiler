#ifndef CODEGEN_HPP
#define CODEGEN_HPP

#include "ast.hpp"
#include <memory>
#include <ostream>
#include <sstream>
#include <string>

enum class TargetArch {
  X86_64,
  AVR,
  ARM64,
  C_Transpiler
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
};

class CodeGeneratorFactory {
public:
  static std::unique_ptr<ICodeGenerator> create(TargetArch arch);
  static std::unique_ptr<ICodeGenerator> create(const std::string &arch_name);
};

#endif // CODEGEN_HPP
