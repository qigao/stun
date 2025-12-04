#pragma once
#include <vector>
#include <string>

namespace cssbox {

struct PathCommand {
    char type;  // M, L, C, Q, A, Z, etc.
    std::vector<float> params;
    bool relative;  // lowercase = relative, uppercase = absolute
};

class SVGPathParser {
public:
    static std::vector<PathCommand> parse(const std::string& d_attr);
    
private:
    static void skip_whitespace(const char*& p);
    static float parse_number(const char*& p);
    static std::vector<float> parse_params(const char*& p, int count);
};

} // namespace cssbox
