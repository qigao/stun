#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace flexUI::detail {

inline float css_nan() {
  return std::numeric_limits<float>::quiet_NaN();
}

struct CssLengthContext {
  float percent_reference = css_nan();
  float viewport_width = css_nan();
  float viewport_height = css_nan();
};

inline std::string trim_css_copy(const std::string& value) {
  size_t start = 0;
  while (start < value.size() &&
         std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  size_t end = value.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(start, end - start);
}

inline std::string lower_css_copy(const std::string& value) {
  std::string lowered = value;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                 [](unsigned char c) {
                   return static_cast<char>(std::tolower(c));
                 });
  return lowered;
}

inline std::vector<std::string> split_css_tokens(const std::string& value) {
  std::vector<std::string> tokens;
  std::string current;
  int paren_depth = 0;

  for (char ch : value) {
    if (ch == '(') {
      ++paren_depth;
    } else if (ch == ')' && paren_depth > 0) {
      --paren_depth;
    }

    if (std::isspace(static_cast<unsigned char>(ch)) && paren_depth == 0) {
      const std::string token = trim_css_copy(current);
      if (!token.empty()) {
        tokens.push_back(token);
      }
      current.clear();
      continue;
    }

    current.push_back(ch);
  }

  const std::string tail = trim_css_copy(current);
  if (!tail.empty()) {
    tokens.push_back(tail);
  }
  return tokens;
}

inline bool is_css_math_length(const std::string& value) {
  const std::string lowered = lower_css_copy(trim_css_copy(value));
  return lowered.rfind("calc(", 0) == 0 || lowered.rfind("min(", 0) == 0 ||
         lowered.rfind("max(", 0) == 0 || lowered.rfind("clamp(", 0) == 0;
}

inline bool is_css_length_token(const std::string& value) {
  const std::string trimmed = lower_css_copy(trim_css_copy(value));
  if (trimmed.empty()) {
    return false;
  }
  if (is_css_math_length(trimmed)) {
    return true;
  }
  char* end = nullptr;
  std::strtof(trimmed.c_str(), &end);
  if (end == trimmed.c_str() || end == nullptr || *end == '\0') {
    return end != trimmed.c_str();
  }

  const std::string unit(end);
  return unit == "px" || unit == "rem" || unit == "em" || unit == "%" ||
         unit == "vw" || unit == "vh" || unit == "vi" || unit == "vb" ||
         unit == "vmin" || unit == "vmax" || unit == "svw" || unit == "svh" ||
         unit == "svi" || unit == "svb" || unit == "lvw" || unit == "lvh" ||
         unit == "lvi" || unit == "lvb" || unit == "dvw" || unit == "dvh" ||
         unit == "dvi" || unit == "dvb";
}

class CssMathLengthParser {
public:
  CssMathLengthParser(std::string value, CssLengthContext context)
      : text_(std::move(value)), context_(context) {}

  float parse() {
    const float value = parse_expression();
    skip_ws();
    if (pos_ != text_.size()) {
      return css_nan();
    }
    return value;
  }

private:
  void skip_ws() {
    while (pos_ < text_.size() &&
           std::isspace(static_cast<unsigned char>(text_[pos_]))) {
      ++pos_;
    }
  }

  bool consume(char ch) {
    skip_ws();
    if (pos_ < text_.size() && text_[pos_] == ch) {
      ++pos_;
      return true;
    }
    return false;
  }

  char peek() const {
    return pos_ < text_.size() ? text_[pos_] : '\0';
  }

  std::string parse_identifier() {
    const size_t start = pos_;
    while (pos_ < text_.size()) {
      const unsigned char ch = static_cast<unsigned char>(text_[pos_]);
      if (!std::isalnum(ch) && text_[pos_] != '-' && text_[pos_] != '_') {
        break;
      }
      ++pos_;
    }
    return text_.substr(start, pos_ - start);
  }

  float parse_expression() {
    float value = parse_term();
    for (;;) {
      if (consume('+')) {
        value += parse_term();
      } else if (consume('-')) {
        value -= parse_term();
      } else {
        return value;
      }
    }
  }

  float parse_term() {
    float value = parse_factor();
    for (;;) {
      if (consume('*')) {
        value *= parse_factor();
      } else if (consume('/')) {
        const float divisor = parse_factor();
        if (std::fabs(divisor) <= 0.000001f) {
          return css_nan();
        }
        value /= divisor;
      } else {
        return value;
      }
    }
  }

  float parse_function(const std::string& name) {
    if (!consume('(')) {
      return css_nan();
    }
    if (name == "calc") {
      const float value = parse_expression();
      return consume(')') ? value : css_nan();
    }

    std::vector<float> args;
    for (;;) {
      args.push_back(parse_expression());
      if (consume(')')) {
        break;
      }
      if (!consume(',')) {
        return css_nan();
      }
    }

    if (args.empty()) {
      return css_nan();
    }
    if (name == "min") {
      return *std::min_element(args.begin(), args.end());
    }
    if (name == "max") {
      return *std::max_element(args.begin(), args.end());
    }
    if (name == "clamp") {
      if (args.size() < 3) {
        return css_nan();
      }
      const float lower = std::min(args[0], args[2]);
      const float upper = std::max(args[0], args[2]);
      return std::clamp(args[1], lower, upper);
    }
    return css_nan();
  }

  float parse_factor() {
    skip_ws();
    if (consume('+')) {
      return parse_factor();
    }
    if (consume('-')) {
      return -parse_factor();
    }
    if (consume('(')) {
      const float value = parse_expression();
      return consume(')') ? value : css_nan();
    }

    if (std::isalpha(static_cast<unsigned char>(peek())) || peek() == '-') {
      const std::string word = parse_identifier();
      if (word == "calc" || word == "min" || word == "max" ||
          word == "clamp") {
        return parse_function(word);
      }
      return css_nan();
    }

    return parse_number_with_unit();
  }

  float parse_number_with_unit() {
    skip_ws();
    const size_t number_start = pos_;
    if (pos_ < text_.size() && (text_[pos_] == '+' || text_[pos_] == '-')) {
      ++pos_;
    }

    bool saw_digit = false;
    while (pos_ < text_.size() &&
           (std::isdigit(static_cast<unsigned char>(text_[pos_])) ||
            text_[pos_] == '.')) {
      saw_digit = saw_digit ||
                  std::isdigit(static_cast<unsigned char>(text_[pos_]));
      ++pos_;
    }
    if (!saw_digit) {
      return css_nan();
    }

    char* end = nullptr;
    const std::string raw_number = text_.substr(number_start, pos_ - number_start);
    const float number = std::strtof(raw_number.c_str(), &end);
    if (end == raw_number.c_str() || !std::isfinite(number)) {
      return css_nan();
    }

    const size_t unit_start = pos_;
    while (pos_ < text_.size() &&
           (std::isalpha(static_cast<unsigned char>(text_[pos_])) ||
            text_[pos_] == '%')) {
      ++pos_;
    }
    const std::string unit = text_.substr(unit_start, pos_ - unit_start);
    if (!unit.empty() && !is_css_length_token(raw_number + unit)) {
      return css_nan();
    }
    if (unit == "rem" || unit == "em") {
      return number * 16.0f;
    }
    if (unit == "%") {
      return std::isnan(context_.percent_reference)
                 ? number
                 : context_.percent_reference * number / 100.0f;
    }
    if (unit == "vw" || unit == "svw" || unit == "lvw" || unit == "dvw" ||
        unit == "vi" || unit == "svi" || unit == "lvi" || unit == "dvi") {
      return std::isnan(context_.viewport_width)
                 ? number
                 : context_.viewport_width * number / 100.0f;
    }
    if (unit == "vh" || unit == "svh" || unit == "lvh" || unit == "dvh" ||
        unit == "vb" || unit == "svb" || unit == "lvb" || unit == "dvb") {
      return std::isnan(context_.viewport_height)
                 ? number
                 : context_.viewport_height * number / 100.0f;
    }
    if (unit == "vmin") {
      const float viewport_min =
          std::min(context_.viewport_width, context_.viewport_height);
      return std::isnan(viewport_min) ? number : viewport_min * number / 100.0f;
    }
    if (unit == "vmax") {
      const float viewport_max =
          std::max(context_.viewport_width, context_.viewport_height);
      return std::isnan(viewport_max) ? number : viewport_max * number / 100.0f;
    }
    return unit.empty() || unit == "px" ? number : css_nan();
  }

  std::string text_;
  size_t pos_ = 0;
  CssLengthContext context_{};
};

inline float parse_css_math_length(const std::string& value,
                                   CssLengthContext context = {}) {
  const std::string normalized = lower_css_copy(trim_css_copy(value));
  if (normalized.empty()) {
    return css_nan();
  }
  CssMathLengthParser parser(normalized, context);
  return parser.parse();
}

inline float parse_css_length(const std::string& value,
                              CssLengthContext context = {}) {
  const std::string trimmed = lower_css_copy(trim_css_copy(value));
  if (trimmed.empty() || trimmed == "auto" || trimmed == "none") {
    return css_nan();
  }
  if (is_css_math_length(trimmed)) {
    return parse_css_math_length(trimmed, context);
  }
  CssMathLengthParser parser(trimmed, context);
  return parser.parse();
}

inline float parse_css_math_length(const std::string& value,
                                   float percent_reference) {
  CssLengthContext context;
  context.percent_reference = percent_reference;
  return parse_css_math_length(value, context);
}

inline float parse_css_length(const std::string& value,
                              float percent_reference) {
  CssLengthContext context;
  context.percent_reference = percent_reference;
  return parse_css_length(value, context);
}

} // namespace flexUI::detail
