#ifndef ASSEMBLER_LINKER_HPP
#define ASSEMBLER_LINKER_HPP

#include "codegen.hpp"
#include <memory>
#include <string>

/**
 * @brief Result of an assemble and link operation.
 */
struct AssemblerLinkerResult {
  bool success{false};
  int exit_code{0};
  std::string output_path;
  std::string message;
};

#include "x86_64/internal_pe_linker.hpp"
#include "x86_64/internal_elf_linker.hpp"

/**
 * @brief Abstract interface for target-specific assembler and linker drivers.
 */
class IAssemblerLinker {
public:
  virtual ~IAssemblerLinker() = default;

  virtual TargetArch get_arch() const = 0;
  virtual const char *get_arch_name() const = 0;
  virtual bool is_supported() const = 0;
  virtual void set_linker_config(const LinkerConfig &config) { (void)config; }

  virtual AssemblerLinkerResult link_modules(
      const std::vector<AssembledModule> &modules,
      const std::string &output_filepath,
      TargetFileType target_file_type = TargetFileType::Auto) = 0;

  virtual AssemblerLinkerResult assemble_and_link_multiple(
      const std::vector<std::string> &assembly_codes,
      const std::string &output_filepath,
      TargetFileType target_file_type = TargetFileType::Auto) = 0;

  virtual AssemblerLinkerResult assemble_and_link(
      const std::string &assembly_code,
      const std::string &output_filepath) = 0;
};

/**
 * @brief Embedded assembler and linker implementation for the x86_64 target.
 */
class X86_64AssemblerLinker : public IAssemblerLinker {
public:
  TargetArch get_arch() const override { return TargetArch::X86_64; }
  const char *get_arch_name() const override { return "x86_64"; }
  bool is_supported() const override { return true; }
  void set_linker_config(const LinkerConfig &config) override { config_ = config; }

  AssemblerLinkerResult link_modules(
      const std::vector<AssembledModule> &modules,
      const std::string &output_filepath,
      TargetFileType target_file_type = TargetFileType::Auto) override;

  AssemblerLinkerResult assemble_and_link_multiple(
      const std::vector<std::string> &assembly_codes,
      const std::string &output_filepath,
      TargetFileType target_file_type = TargetFileType::Auto) override;

  AssemblerLinkerResult assemble_and_link(
      const std::string &assembly_code,
      const std::string &output_filepath) override;

private:
  LinkerConfig config_;
};

/**
 * @brief Embedded assembler and linker implementation for Linux / ELF64 target.
 * Produces standalone static 64-bit ELF executables for Linux (x86_64).
 */
class LinuxAssemblerLinker : public IAssemblerLinker {
public:
  TargetArch get_arch() const override { return TargetArch::X86_64; }
  const char *get_arch_name() const override { return "linux"; }
  bool is_supported() const override { return true; }
  void set_linker_config(const LinkerConfig &config) override { config_ = config; }

  AssemblerLinkerResult link_modules(
      const std::vector<AssembledModule> &modules,
      const std::string &output_filepath,
      TargetFileType target_file_type = TargetFileType::Auto) override;

  AssemblerLinkerResult assemble_and_link_multiple(
      const std::vector<std::string> &assembly_codes,
      const std::string &output_filepath,
      TargetFileType target_file_type = TargetFileType::Auto) override;

  AssemblerLinkerResult assemble_and_link(
      const std::string &assembly_code,
      const std::string &output_filepath) override;

private:
  LinkerConfig config_;
};

/**
 * @brief Embedded assembler and linker implementation for the AVR target.
 * Produces standalone Intel HEX (.hex) flash images for Arduino Uno / ATmega328P.
 */
class AVRAssemblerLinker : public IAssemblerLinker {
public:
  TargetArch get_arch() const override { return TargetArch::AVR; }
  const char *get_arch_name() const override { return "avr"; }
  bool is_supported() const override { return true; }

  AssemblerLinkerResult link_modules(
      const std::vector<AssembledModule> &modules,
      const std::string &output_filepath,
      TargetFileType target_file_type = TargetFileType::Auto) override;

  AssemblerLinkerResult assemble_and_link_multiple(
      const std::vector<std::string> &assembly_codes,
      const std::string &output_filepath,
      TargetFileType target_file_type = TargetFileType::Auto) override;

  AssemblerLinkerResult assemble_and_link(
      const std::string &assembly_code,
      const std::string &output_filepath) override;
};

/**
 * @brief Stub assembler and linker for architectures where binary generation
 * is not yet available.
 */
class StubAssemblerLinker : public IAssemblerLinker {
public:
  StubAssemblerLinker(TargetArch arch, std::string arch_name)
      : arch_(arch), arch_name_(std::move(arch_name)) {}

  TargetArch get_arch() const override { return arch_; }
  const char *get_arch_name() const override { return arch_name_.c_str(); }
  bool is_supported() const override { return false; }

  AssemblerLinkerResult link_modules(
      const std::vector<AssembledModule> &modules,
      const std::string &output_filepath,
      TargetFileType target_file_type = TargetFileType::Auto) override;

  AssemblerLinkerResult assemble_and_link_multiple(
      const std::vector<std::string> &assembly_codes,
      const std::string &output_filepath,
      TargetFileType target_file_type = TargetFileType::Auto) override;

  AssemblerLinkerResult assemble_and_link(
      const std::string &assembly_code,
      const std::string &output_filepath) override;

private:
  TargetArch arch_;
  std::string arch_name_;
};

/**
 * @brief Factory for creating assembler & linker instances based on architecture.
 */
class AssemblerLinkerFactory {
public:
  static std::unique_ptr<IAssemblerLinker> create(const TargetSpec &spec);
  static std::unique_ptr<IAssemblerLinker> create(TargetArch arch);
  static std::unique_ptr<IAssemblerLinker> create(const std::string &arch_name);
};

#endif // ASSEMBLER_LINKER_HPP
