#pragma once

#include <ir/unified_diagram.h>
#include <string>
#include <vector>

namespace flex::modules::flexmaid {

struct Token;
struct ParseResult;

// Strategy Pattern: 图表解析器接口
// 现在作为 DiagramInterpreter 的适配器
class IChartParser {
public:
    virtual ~IChartParser() = default;
    virtual bool parse(const std::vector<Token>& tokens, UnifiedDiagram& diagram) = 0;
};

} // namespace flex::modules::flexmaid
