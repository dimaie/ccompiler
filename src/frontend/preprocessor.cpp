#include "preprocessor.hpp"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

namespace {

std::string trim(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  size_t end = s.find_last_not_of(" \t\r\n");
  return s.substr(start, end - start + 1);
}

bool is_ident_start(char c) {
  return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool is_ident_part(char c) {
  return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

struct CondFrame {
  bool parent_active{true};
  bool branch_taken{false};
  bool current_active{true};
};

} // namespace

Preprocessor::Preprocessor() {
  init_virtual_headers();
}

void Preprocessor::add_include_dir(const std::string &dir) {
  include_dirs_.push_back(dir);
}

void Preprocessor::define_macro(const std::string &name, const std::string &value) {
  MacroDef def;
  def.name = name;
  def.is_function_like = false;
  def.replacement = value;
  macros_[name] = def;
}

void Preprocessor::undef_macro(const std::string &name) {
  macros_.erase(name);
}

bool Preprocessor::is_defined(const std::string &name) const {
  return macros_.find(name) != macros_.end();
}

void Preprocessor::init_virtual_headers() {
  virtual_headers_["stdio.h"] =
      "#ifndef _STDIO_H\n"
      "#define _STDIO_H\n"
      "int puts(const char *s);\n"
      "int printf(const char *format);\n"
      "int putchar(int c);\n"
      "int getchar(void);\n"
      "#endif\n";

  virtual_headers_["stdlib.h"] =
      "#ifndef _STDLIB_H\n"
      "#define _STDLIB_H\n"
      "void exit(int status);\n"
      "void *malloc(unsigned long size);\n"
      "void free(void *ptr);\n"
      "#endif\n";

  virtual_headers_["stdbool.h"] =
      "#ifndef _STDBOOL_H\n"
      "#define _STDBOOL_H\n"
      "#define bool int\n"
      "#define true 1\n"
      "#define false 0\n"
      "#endif\n";

  virtual_headers_["string.h"] =
      "#ifndef _STRING_H\n"
      "#define _STRING_H\n"
      "unsigned long strlen(const char *s);\n"
      "char *strcpy(char *dest, const char *src);\n"
      "int strcmp(const char *s1, const char *s2);\n"
      "#endif\n";
}

std::string Preprocessor::process(const std::string &source, const std::string &current_filepath) {
  include_depth_ = 0;
  return process_internal(source, current_filepath);
}

std::string Preprocessor::resolve_include_path(const std::string &header, bool is_quote, const std::string &current_file_dir) {
  if (is_quote && !current_file_dir.empty()) {
    std::filesystem::path local_p = std::filesystem::path(current_file_dir) / header;
    if (std::filesystem::exists(local_p)) {
      return local_p.string();
    }
  }

  for (const auto &dir : include_dirs_) {
    std::filesystem::path p = std::filesystem::path(dir) / header;
    if (std::filesystem::exists(p)) {
      return p.string();
    }
  }

  // Check default include directories: ./include, ../include, and relative to current file
  std::vector<std::string> default_dirs = {
      "include",
      "../include",
      "../../include"
  };
  if (!current_file_dir.empty()) {
    default_dirs.push_back((std::filesystem::path(current_file_dir) / "include").string());
    default_dirs.push_back((std::filesystem::path(current_file_dir) / "../include").string());
  }

  for (const auto &dir : default_dirs) {
    std::filesystem::path p = std::filesystem::path(dir) / header;
    if (std::filesystem::exists(p)) {
      return p.string();
    }
  }

  // Check virtual headers fallback (guarantees standalone operation if include folder missing)
  auto vit = virtual_headers_.find(header);
  if (vit != virtual_headers_.end()) {
    return "__virtual__:" + header;
  }

  return "";
}

// Simple recursive-descent integer constant expression evaluator for #if
namespace {
class ExprParser {
public:
  ExprParser(const std::string &s, const Preprocessor &pp) : s_(s), pp_(pp), pos_(0) {}

  int64_t parse() {
    skip_ws();
    if (pos_ >= s_.size()) return 0;
    int64_t val = parse_logical_or();
    return val;
  }

private:
  const std::string &s_;
  const Preprocessor &pp_;
  size_t pos_;

  void skip_ws() {
    while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) pos_++;
  }

  int64_t parse_logical_or() {
    int64_t left = parse_logical_and();
    while (true) {
      skip_ws();
      if (pos_ + 1 < s_.size() && s_[pos_] == '|' && s_[pos_ + 1] == '|') {
        pos_ += 2;
        int64_t right = parse_logical_and();
        left = (left || right) ? 1 : 0;
      } else {
        break;
      }
    }
    return left;
  }

  int64_t parse_logical_and() {
    int64_t left = parse_equality();
    while (true) {
      skip_ws();
      if (pos_ + 1 < s_.size() && s_[pos_] == '&' && s_[pos_ + 1] == '&') {
        pos_ += 2;
        int64_t right = parse_equality();
        left = (left && right) ? 1 : 0;
      } else {
        break;
      }
    }
    return left;
  }

  int64_t parse_equality() {
    int64_t left = parse_relational();
    while (true) {
      skip_ws();
      if (pos_ + 1 < s_.size() && s_[pos_] == '=' && s_[pos_ + 1] == '=') {
        pos_ += 2;
        int64_t right = parse_relational();
        left = (left == right) ? 1 : 0;
      } else if (pos_ + 1 < s_.size() && s_[pos_] == '!' && s_[pos_ + 1] == '=') {
        pos_ += 2;
        int64_t right = parse_relational();
        left = (left != right) ? 1 : 0;
      } else {
        break;
      }
    }
    return left;
  }

  int64_t parse_relational() {
    int64_t left = parse_additive();
    while (true) {
      skip_ws();
      if (pos_ + 1 < s_.size() && s_[pos_] == '<' && s_[pos_ + 1] == '=') {
        pos_ += 2;
        int64_t right = parse_additive();
        left = (left <= right) ? 1 : 0;
      } else if (pos_ + 1 < s_.size() && s_[pos_] == '>' && s_[pos_ + 1] == '=') {
        pos_ += 2;
        int64_t right = parse_additive();
        left = (left >= right) ? 1 : 0;
      } else if (pos_ < s_.size() && s_[pos_] == '<') {
        pos_++;
        int64_t right = parse_additive();
        left = (left < right) ? 1 : 0;
      } else if (pos_ < s_.size() && s_[pos_] == '>') {
        pos_++;
        int64_t right = parse_additive();
        left = (left > right) ? 1 : 0;
      } else {
        break;
      }
    }
    return left;
  }

  int64_t parse_additive() {
    int64_t left = parse_unary();
    while (true) {
      skip_ws();
      if (pos_ < s_.size() && s_[pos_] == '+') {
        pos_++;
        left += parse_unary();
      } else if (pos_ < s_.size() && s_[pos_] == '-') {
        pos_++;
        left -= parse_unary();
      } else {
        break;
      }
    }
    return left;
  }

  int64_t parse_unary() {
    skip_ws();
    if (pos_ < s_.size() && s_[pos_] == '!') {
      pos_++;
      return !parse_unary();
    }
    if (pos_ < s_.size() && s_[pos_] == '+') {
      pos_++;
      return parse_unary();
    }
    if (pos_ < s_.size() && s_[pos_] == '-') {
      pos_++;
      return -parse_unary();
    }
    return parse_primary();
  }

  int64_t parse_primary() {
    skip_ws();
    if (pos_ >= s_.size()) return 0;

    if (s_[pos_] == '(') {
      pos_++;
      int64_t val = parse_logical_or();
      skip_ws();
      if (pos_ < s_.size() && s_[pos_] == ')') pos_++;
      return val;
    }

    // Check "defined(X)" or "defined X"
    if (s_.compare(pos_, 7, "defined") == 0 && (pos_ + 7 >= s_.size() || !is_ident_part(s_[pos_ + 7]))) {
      pos_ += 7;
      skip_ws();
      bool has_paren = false;
      if (pos_ < s_.size() && s_[pos_] == '(') {
        has_paren = true;
        pos_++;
        skip_ws();
      }
      std::string id;
      while (pos_ < s_.size() && is_ident_part(s_[pos_])) {
        id += s_[pos_++];
      }
      skip_ws();
      if (has_paren && pos_ < s_.size() && s_[pos_] == ')') {
        pos_++;
      }
      return pp_.is_defined(id) ? 1 : 0;
    }

    // Number literal
    if (std::isdigit(static_cast<unsigned char>(s_[pos_]))) {
      size_t end = pos_;
      while (end < s_.size() && (std::isalnum(static_cast<unsigned char>(s_[end])) || s_[end] == 'x' || s_[end] == 'X')) {
        end++;
      }
      std::string num_str = s_.substr(pos_, end - pos_);
      pos_ = end;
      try {
        return std::stoll(num_str, nullptr, 0);
      } catch (...) {
        return 0;
      }
    }

    // Identifier: if defined as a macro, could evaluate its expansion, otherwise undefined -> 0
    if (is_ident_start(s_[pos_])) {
      size_t end = pos_;
      while (end < s_.size() && is_ident_part(s_[end])) end++;
      std::string id = s_.substr(pos_, end - pos_);
      pos_ = end;
      // In C preprocessor, undefined identifiers in #if evaluate to 0
      return 0;
    }

    pos_++;
    return 0;
  }
};
} // namespace

bool Preprocessor::eval_condition(const std::string &expr) {
  // 1. Replace defined(X) and defined X with 1 or 0 before macro expansion
  std::string replaced_defined;
  for (size_t i = 0; i < expr.size();) {
    if (expr.compare(i, 7, "defined") == 0 && (i + 7 >= expr.size() || !is_ident_part(expr[i + 7]))) {
      size_t pos = i + 7;
      while (pos < expr.size() && std::isspace(static_cast<unsigned char>(expr[pos]))) pos++;
      bool has_paren = false;
      if (pos < expr.size() && expr[pos] == '(') {
        has_paren = true;
        pos++;
        while (pos < expr.size() && std::isspace(static_cast<unsigned char>(expr[pos]))) pos++;
      }
      std::string id;
      while (pos < expr.size() && is_ident_part(expr[pos])) {
        id += expr[pos++];
      }
      while (pos < expr.size() && std::isspace(static_cast<unsigned char>(expr[pos]))) pos++;
      if (has_paren && pos < expr.size() && expr[pos] == ')') {
        pos++;
      }
      replaced_defined += is_defined(id) ? "1" : "0";
      i = pos;
    } else {
      replaced_defined += expr[i++];
    }
  }

  // 2. Expand remaining macros
  std::set<std::string> active;
  std::string expanded = expand_macros_in_text(replaced_defined, active);

  // 3. Evaluate expression
  ExprParser parser(expanded, *this);
  return parser.parse() != 0;
}

std::string Preprocessor::process_internal(const std::string &source, const std::string &current_filepath) {
  if (++include_depth_ > 64) {
    throw std::runtime_error("Preprocessor error: #include recursion depth exceeded 64");
  }

  std::string current_dir;
  if (!current_filepath.empty()) {
    std::filesystem::path p(current_filepath);
    current_dir = p.parent_path().string();
  }

  // 1. Line continuations: join lines ending with '\'
  std::string joined_source;
  joined_source.reserve(source.size());
  for (size_t i = 0; i < source.size(); ++i) {
    if (source[i] == '\\' && i + 1 < source.size()) {
      if (source[i + 1] == '\n') {
        i++; // skip both
        continue;
      }
      if (source[i + 1] == '\r' && i + 2 < source.size() && source[i + 2] == '\n') {
        i += 2; // skip all three
        continue;
      }
    }
    joined_source += source[i];
  }

  std::istringstream stream(joined_source);
  std::string line;
  std::string result;
  std::vector<CondFrame> cond_stack;

  auto is_active = [&cond_stack]() -> bool {
    return cond_stack.empty() || cond_stack.back().current_active;
  };

  while (std::getline(stream, line)) {
    std::string trimmed = trim(line);

    // Check for preprocessor directive (#...)
    if (!trimmed.empty() && trimmed[0] == '#') {
      std::string directive_content = trim(trimmed.substr(1));
      std::istringstream dstream(directive_content);
      std::string directive;
      dstream >> directive;

      if (directive == "ifdef") {
        std::string macro_name;
        dstream >> macro_name;
        bool parent_act = is_active();
        bool cond = is_defined(macro_name);
        bool act = parent_act && cond;
        cond_stack.push_back({parent_act, act, act});
        continue;
      } else if (directive == "ifndef") {
        std::string macro_name;
        dstream >> macro_name;
        bool parent_act = is_active();
        bool cond = !is_defined(macro_name);
        bool act = parent_act && cond;
        cond_stack.push_back({parent_act, act, act});
        continue;
      } else if (directive == "if") {
        std::string rest;
        std::getline(dstream, rest);
        bool parent_act = is_active();
        bool cond = parent_act && eval_condition(trim(rest));
        cond_stack.push_back({parent_act, cond, cond});
        continue;
      } else if (directive == "elif") {
        if (cond_stack.empty()) {
          throw std::runtime_error("Preprocessor error: #elif without matching #if");
        }
        CondFrame &frame = cond_stack.back();
        std::string rest;
        std::getline(dstream, rest);
        if (frame.parent_active && !frame.branch_taken) {
          bool cond = eval_condition(trim(rest));
          frame.current_active = cond;
          if (cond) frame.branch_taken = true;
        } else {
          frame.current_active = false;
        }
        continue;
      } else if (directive == "else") {
        if (cond_stack.empty()) {
          throw std::runtime_error("Preprocessor error: #else without matching #if");
        }
        CondFrame &frame = cond_stack.back();
        if (frame.parent_active && !frame.branch_taken) {
          frame.current_active = true;
          frame.branch_taken = true;
        } else {
          frame.current_active = false;
        }
        continue;
      } else if (directive == "endif") {
        if (cond_stack.empty()) {
          throw std::runtime_error("Preprocessor error: #endif without matching #if");
        }
        cond_stack.pop_back();
        continue;
      }

      // Other directives only execute if current block is active
      if (!is_active()) {
        continue;
      }

      if (directive == "define") {
        // Skip leading whitespace after #define
        size_t name_start = directive_content.find("define");
        name_start += 6;
        while (name_start < directive_content.size() && std::isspace(static_cast<unsigned char>(directive_content[name_start]))) {
          name_start++;
        }
        if (name_start >= directive_content.size()) continue;

        size_t name_end = name_start;
        while (name_end < directive_content.size() && is_ident_part(directive_content[name_end])) {
          name_end++;
        }
        std::string macro_name = directive_content.substr(name_start, name_end - name_start);

        MacroDef def;
        def.name = macro_name;

        // Function-like macro: #define NAME(p1, p2)
        if (name_end < directive_content.size() && directive_content[name_end] == '(') {
          def.is_function_like = true;
          size_t paren_close = directive_content.find(')', name_end);
          if (paren_close == std::string::npos) {
            throw std::runtime_error("Preprocessor error: unterminated macro parameter list for " + macro_name);
          }
          std::string params_str = directive_content.substr(name_end + 1, paren_close - name_end - 1);
          std::istringstream pstream(params_str);
          std::string param;
          while (std::getline(pstream, param, ',')) {
            std::string tp = trim(param);
            if (!tp.empty()) def.params.push_back(tp);
          }
          def.replacement = trim(directive_content.substr(paren_close + 1));
        } else {
          def.is_function_like = false;
          def.replacement = trim(directive_content.substr(name_end));
          if (def.replacement.empty()) def.replacement = "1";
        }

        macros_[macro_name] = def;
        continue;
      } else if (directive == "undef") {
        std::string macro_name;
        dstream >> macro_name;
        macros_.erase(macro_name);
        continue;
      } else if (directive == "include") {
        size_t open_quote = directive_content.find('"');
        size_t close_quote = (open_quote != std::string::npos) ? directive_content.find('"', open_quote + 1) : std::string::npos;
        size_t open_angle = directive_content.find('<');
        size_t close_angle = (open_angle != std::string::npos) ? directive_content.find('>', open_angle + 1) : std::string::npos;

        std::string header;
        bool is_quote = false;

        if (open_quote != std::string::npos && close_quote != std::string::npos) {
          header = directive_content.substr(open_quote + 1, close_quote - open_quote - 1);
          is_quote = true;
        } else if (open_angle != std::string::npos && close_angle != std::string::npos) {
          header = directive_content.substr(open_angle + 1, close_angle - open_angle - 1);
          is_quote = false;
        } else {
          throw std::runtime_error("Preprocessor error: invalid #include syntax: " + line);
        }

        std::string resolved = resolve_include_path(header, is_quote, current_dir);
        if (resolved.empty()) {
          throw std::runtime_error("Preprocessor error: cannot find include file: " + header);
        }

        if (resolved.rfind("__virtual__:", 0) == 0) {
          std::string vname = resolved.substr(12);
          std::string inc_res = process_internal(virtual_headers_[vname], vname);
          result += inc_res + "\n";
        } else {
          std::ifstream inc_file(resolved);
          if (!inc_file.is_open()) {
            throw std::runtime_error("Preprocessor error: failed to open file: " + resolved);
          }
          std::stringstream buf;
          buf << inc_file.rdbuf();
          std::string inc_res = process_internal(buf.str(), resolved);
          result += inc_res + "\n";
        }
        continue;
      }
    }

    // Non-directive lines: output only if active
    if (is_active()) {
      std::set<std::string> active_macros;
      std::string expanded = expand_macros_in_text(line, active_macros);
      result += expanded + "\n";
    } else {
      // Keep newline count for accurate line numbers in error reporting
      result += "\n";
    }
  }

  include_depth_--;
  return result;
}

std::string Preprocessor::expand_macros_in_text(const std::string &text, std::set<std::string> &active_macros) {
  std::string output;
  output.reserve(text.size());

  for (size_t i = 0; i < text.size();) {
    // 1. Skip string literals "..."
    if (text[i] == '"') {
      output += text[i++];
      while (i < text.size()) {
        if (text[i] == '\\' && i + 1 < text.size()) {
          output += text[i++];
          output += text[i++];
        } else if (text[i] == '"') {
          output += text[i++];
          break;
        } else {
          output += text[i++];
        }
      }
      continue;
    }

    // 2. Skip char literals '...'
    if (text[i] == '\'') {
      output += text[i++];
      while (i < text.size()) {
        if (text[i] == '\\' && i + 1 < text.size()) {
          output += text[i++];
          output += text[i++];
        } else if (text[i] == '\'') {
          output += text[i++];
          break;
        } else {
          output += text[i++];
        }
      }
      continue;
    }

    // 3. Skip line comments //
    if (text[i] == '/' && i + 1 < text.size() && text[i + 1] == '/') {
      output += text.substr(i);
      break;
    }

    // 4. Skip block comments /* ... */
    if (text[i] == '/' && i + 1 < text.size() && text[i + 1] == '*') {
      size_t end_comment = text.find("*/", i + 2);
      if (end_comment != std::string::npos) {
        output += text.substr(i, end_comment + 2 - i);
        i = end_comment + 2;
      } else {
        output += text.substr(i);
        break;
      }
      continue;
    }

    // 5. Identifier tokens: could be macro invocation
    if (is_ident_start(text[i])) {
      size_t ident_start = i;
      while (i < text.size() && is_ident_part(text[i])) i++;
      std::string ident = text.substr(ident_start, i - ident_start);

      auto it = macros_.find(ident);
      if (it != macros_.end() && active_macros.find(ident) == active_macros.end()) {
        const MacroDef &m = it->second;
        if (!m.is_function_like) {
          // Object-like macro expansion
          active_macros.insert(ident);
          std::string expanded = expand_macros_in_text(m.replacement, active_macros);
          active_macros.erase(ident);
          output += expanded;
          continue;
        } else {
          // Function-like macro: check for '('
          size_t peek = i;
          while (peek < text.size() && std::isspace(static_cast<unsigned char>(text[peek]))) peek++;
          if (peek < text.size() && text[peek] == '(') {
            peek++; // skip '('
            // Parse arguments
            std::vector<std::string> args;
            std::string current_arg;
            int paren_depth = 0;
            bool inside_str = false;

            while (peek < text.size()) {
              char c = text[peek++];
              if (c == '"' && (peek < 2 || text[peek - 2] != '\\')) {
                inside_str = !inside_str;
                current_arg += c;
              } else if (!inside_str && c == '(') {
                paren_depth++;
                current_arg += c;
              } else if (!inside_str && c == ')') {
                if (paren_depth > 0) {
                  paren_depth--;
                  current_arg += c;
                } else {
                  // End of arguments
                  args.push_back(trim(current_arg));
                  break;
                }
              } else if (!inside_str && c == ',' && paren_depth == 0) {
                args.push_back(trim(current_arg));
                current_arg.clear();
              } else {
                current_arg += c;
              }
            }

            // Substitute parameters in replacement
            std::string substituted;
            for (size_t r = 0; r < m.replacement.size();) {
              if (is_ident_start(m.replacement[r])) {
                size_t p_start = r;
                while (r < m.replacement.size() && is_ident_part(m.replacement[r])) r++;
                std::string p_name = m.replacement.substr(p_start, r - p_start);
                auto param_it = std::find(m.params.begin(), m.params.end(), p_name);
                if (param_it != m.params.end()) {
                  size_t idx = std::distance(m.params.begin(), param_it);
                  if (idx < args.size()) {
                    substituted += args[idx];
                  }
                } else {
                  substituted += p_name;
                }
              } else {
                substituted += m.replacement[r++];
              }
            }

            i = peek;
            active_macros.insert(ident);
            std::string expanded = expand_macros_in_text(substituted, active_macros);
            active_macros.erase(ident);
            output += expanded;
            continue;
          }
        }
      }

      // Not a macro or cannot expand
      output += ident;
      continue;
    }

    // Non-identifier character
    output += text[i++];
  }

  return output;
}
