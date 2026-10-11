#ifndef PREPROCESSOR_HPP
#define PREPROCESSOR_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <set>

struct MacroDef {
  std::string name;
  bool is_function_like{false};
  std::vector<std::string> params;
  std::string replacement;
};

class Preprocessor {
public:
  Preprocessor();

  void add_include_dir(const std::string &dir);
  void define_macro(const std::string &name, const std::string &value = "1");
  void undef_macro(const std::string &name);
  bool is_defined(const std::string &name) const;

  /**
   * @brief Preprocess source code.
   * @param source The input C source text.
   * @param current_filepath Path to the source file (used for relative #include).
   * @return The preprocessed source string ready for lexing/parsing.
   */
  std::string process(const std::string &source, const std::string &current_filepath = "");

private:
  std::vector<std::string> include_dirs_;
  std::unordered_map<std::string, MacroDef> macros_;
  std::unordered_map<std::string, std::string> virtual_headers_;
  int include_depth_{0};

  void init_virtual_headers();
  std::string process_internal(const std::string &source, const std::string &current_filepath);
  std::string expand_macros_in_text(const std::string &text, std::set<std::string> &active_macros);
  std::string resolve_include_path(const std::string &header, bool is_quote, const std::string &current_file_dir);
  bool eval_condition(const std::string &expr);
};

#endif // PREPROCESSOR_HPP
