#pragma once

#include <string>

namespace flex::parser {
struct AstProgram;
}

namespace flex::lowering {

struct SemanticValidationResult {
  bool ok = true;
  std::string message;
};

// Validates invariants required by the runtime lowering path. Parsing remains
// syntax-only; this boundary rejects ASTs that the runtime cannot execute
// without silently dropping data or constructing partial state.
SemanticValidationResult validate_runtime_program(const parser::AstProgram &program);

} // namespace flex::lowering
