#pragma once

#include <parser/mermaid_tokens.h>
#include <string>

namespace flex::modules::flexmaid {

class Lexer {
public:
    Lexer(const std::string& input);
    
    Token next_token();
    
    int line() const;
    int column() const;
    
private:
    const std::string& input_;
    const char* cursor_;
    const char* marker_;
    const char* token_start_;
    int line_;
    int column_;
};

} // namespace flex::modules::flexmaid
