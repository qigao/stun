/*
 * Flex DSL parser context shared by the Lemon parser and parser driver.
 * AST construction is intentionally owned by AstBuilder in the driver;
 * Lemon only validates the token stream and reports syntax failures.
 */

#pragma once

#include <string>

namespace flex::parser {

struct ParseContext {
  std::string error_message;
  int error_line = 0;
  int error_column = 0;
};

} // namespace flex::parser
