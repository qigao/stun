#pragma once

#include "ir/unified_diagram.h"
#include <memory>
#include <string>
#include <optional>
#include <vector>

namespace flex {
namespace modules {
namespace flexmaid {

struct ParseResult {
    std::unique_ptr<UnifiedDiagram> diagram;
    bool success = false;
    std::optional<std::string> error;
    int error_line = 0;
    int error_column = 0;
    
    bool has_error() const { return !success; }
    std::string get_error() const { return error.value_or("Unknown error"); }
};

struct Token;
class DiagramInterpreter;

class UnifiedParser {
public:
    UnifiedParser();
    ~UnifiedParser();
    
    ParseResult parse(const std::string& text);
    std::vector<Token> tokenize(const std::string& text);
    
private:
    std::unique_ptr<DiagramInterpreter> interpreter_;
};

} // namespace flexmaid
} // namespace modules
} // namespace flex
