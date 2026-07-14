#pragma once

#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include "flex.h"
#include "flex/runtime/allocator.h"
#include "flex/runtime/expr.h"
#include "flex/runtime/group.h"
#include "flex/runtime/shape.h"
#include "flex/runtime/text.h"
#include "flex/runtime/timeline.h"
#include "flexchart/chart_ast.h"
#include "flexchart/flexchart.h"
#include "flexchart/mark_renderer.h"


namespace flex {
class Instance;

namespace chart {

struct Record {
  std::map<std::string, AstValue> fields;
  AstValue get(const std::string &name) const {
    auto it = fields.find(name);
    return it != fields.end() ? it->second : AstValue(0.0);
  }
};

struct MarkRenderContext {
  ArenaAllocator &arena;
  Group *plot;    // Flex layer for marks
  Group *overlay; // Absolute layer for lines/axes
  float available_plot_w;
  float estimated_plot_h; // Data area height
  float y_scale;
  const std::vector<std::string> &x_labels;
  Instance *instance = nullptr; // For animations

  std::map<std::string, float> stack_offsets; // Track stacking per category
  int mark_index = 0;                         // Current mark being rendered
};

// Inline definition of get_x_pos (needs complete MarkRenderContext)
inline float MarkRenderer::get_x_pos(const std::string &label, const MarkRenderContext &ctx) {
    float inner_w = ctx.available_plot_w - 20.0f; // matches 2x10 padding
    if (ctx.x_labels.empty())
      return 10.0f;
    float band_w = inner_w / ctx.x_labels.size();
    for (size_t i = 0; i < ctx.x_labels.size(); ++i) {
      if (ctx.x_labels[i] == label) {
        return 10.0f + (i + 0.5f) * band_w;
      }
    }
    return 10.0f;
}

// Internal helpers
std::string get_string_val(const AstValue &v);
double get_double_val(const AstValue &v);
std::shared_ptr<AstData> find_dataset(const std::shared_ptr<AstChart> &chart,
                                      const std::string &name);
std::vector<Record> get_records(const std::shared_ptr<AstData> &data);
std::vector<Record> apply_transforms(
    std::vector<Record> records,
    const std::vector<std::shared_ptr<AstTransform>> &transforms);
void apply_computed_fields(
    std::vector<Record> &records,
    const std::vector<std::shared_ptr<AstEncoding>> &encodings);
bool is_expression(const std::string &s);
std::vector<Record> generate_expr_records(
    const std::string &expr,
    const std::map<std::string, std::vector<double>> &ranges);
std::shared_ptr<AstAxis> find_axis(const std::shared_ptr<AstChart> &chart,
                                   const std::string &name);

// Returns the actual field name to use when querying a record for a given
// encoding channel.  If apply_computed_fields generated a synthetic field for
// an expression encoding (stored in the record as __synthetic_channel_<ch>),
// that synthetic name is returned; otherwise the encoding's original field
// name is returned unchanged.  This avoids reading a potentially mutated
// enc->field value.
inline std::string resolved_field_name(const AstEncoding &enc,
                                       const Record &rec) {
    const std::string sentinel = "__synthetic_channel_" + enc.channel;
    auto it = rec.fields.find(sentinel);
    if (it != rec.fields.end()) {
        if (std::holds_alternative<std::string>(it->second))
            return std::get<std::string>(it->second);
    }
    return enc.field;
}


} // namespace chart
} // namespace flex
