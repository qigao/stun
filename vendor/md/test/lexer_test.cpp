#include "md_re2c.h"
#include "md_parser_gen.h"
#include <iostream>
#include <string>

namespace md_re2c {
    extern int lex(LexerState* state, std::string& text);
}

int main() {
    std::string input = "Normal **Bold** *Italic*\n";
    std::cout << "Testing lexer with: \"" << input << "\"" << std::endl;
    
    md_re2c::LexerState state;
    state.start = input.c_str();
    state.cursor = input.c_str();
    state.marker = input.c_str();
    
    std::string text;
    int token;
    int count = 0;
    while ((token = md_re2c::lex(&state, text)) > 0 && count++ < 20) {
        std::cout << "Token " << token << ": [" << text << "]" << std::endl;
        text.clear();
    }
    std::cout << "Final token: " << token << std::endl;
    
    return 0;
}
