#pragma once
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <variant>
#include "flexchart/chart_ast.h"

namespace flex {
class Instance;
class ArenaAllocator;
class Group;

namespace chart {

struct Record;
struct MarkRenderContext;

class MarkRenderer {
public:
    virtual ~MarkRenderer() = default;
    virtual void render(const std::shared_ptr<AstMark>& mark,
                        const std::vector<Record>& records,
                        MarkRenderContext& ctx) = 0;

protected:
    // Helper to find relative x-axis position for absolute elements (like lines)
    float get_x_pos(const std::string& label, const MarkRenderContext& ctx);
};

} // namespace chart
} // namespace flex
