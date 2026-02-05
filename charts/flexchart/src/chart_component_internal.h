#pragma once

#include <map>
#include <memory>
#include <string>
#include <variant>
#include <vector>


#include "flex.h"
#include "flex/runtime/allocator.h"
#include "flex/runtime/group.h"
#include "flex/runtime/shape.h"
#include "flex/runtime/text.h"
#include "flex/runtime/timeline.h"
#include "flexchart/chart_ast.h"
#include "flexchart/flexchart.h"


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

class MarkRenderer {
public:
  virtual ~MarkRenderer() = default;
  virtual void render(const std::shared_ptr<AstMark> &mark, const std::vector<Record> &records,
                      MarkRenderContext &ctx) = 0;

protected:
  // Helper to find relative x-axis position for absolute elements (like lines)
  float get_x_pos(const std::string &label, const MarkRenderContext &ctx) {
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
};

// Internal helpers
std::string get_string_val(const AstValue &v);
double get_double_val(const AstValue &v);
std::shared_ptr<AstData> find_dataset(const std::shared_ptr<AstChart> &chart,
                                      const std::string &name);
std::vector<Record> get_records(const std::shared_ptr<AstData> &data);

// Renderer declarations
class BarMarkRenderer : public MarkRenderer {
public:
  void render(const std::shared_ptr<AstMark> &mark, const std::vector<Record> &records,
              MarkRenderContext &ctx) override;
};

class PieMarkRenderer : public MarkRenderer {
public:
  void render(const std::shared_ptr<AstMark> &mark, const std::vector<Record> &records,
              MarkRenderContext &ctx) override;
};

class LineMarkRenderer : public MarkRenderer {
public:
  void render(const std::shared_ptr<AstMark> &mark, const std::vector<Record> &records,
              MarkRenderContext &ctx) override;
};

class AreaMarkRenderer : public MarkRenderer {
public:
  void render(const std::shared_ptr<AstMark> &mark, const std::vector<Record> &records,
              MarkRenderContext &ctx) override;
};

class PointMarkRenderer : public MarkRenderer {
public:
  void render(const std::shared_ptr<AstMark> &mark, const std::vector<Record> &records,
              MarkRenderContext &ctx) override;
};

class RectMarkRenderer : public MarkRenderer {
public:
  void render(const std::shared_ptr<AstMark> &mark, const std::vector<Record> &records,
              MarkRenderContext &ctx) override;
};

class BoxplotMarkRenderer : public MarkRenderer {
public:
  void render(const std::shared_ptr<AstMark> &mark, const std::vector<Record> &records,
              MarkRenderContext &ctx) override;
};

class RadarMarkRenderer : public MarkRenderer {
public:
  void render(const std::shared_ptr<AstMark> &mark, const std::vector<Record> &records,
              MarkRenderContext &ctx) override;
};

class TextMarkRenderer : public MarkRenderer {
public:
  void render(const std::shared_ptr<AstMark> &mark, const std::vector<Record> &records,
              MarkRenderContext &ctx) override;
};

class RuleMarkRenderer : public MarkRenderer {
public:
  void render(const std::shared_ptr<AstMark> &mark, const std::vector<Record> &records,
              MarkRenderContext &ctx) override;
};

class ArcMarkRenderer : public MarkRenderer {
public:
  void render(const std::shared_ptr<AstMark> &mark, const std::vector<Record> &records,
              MarkRenderContext &ctx) override;
};

class TickMarkRenderer : public MarkRenderer {
public:
  void render(const std::shared_ptr<AstMark> &mark, const std::vector<Record> &records,
              MarkRenderContext &ctx) override;
};

class ErrorbarMarkRenderer : public MarkRenderer {
public:
  void render(const std::shared_ptr<AstMark> &mark, const std::vector<Record> &records,
              MarkRenderContext &ctx) override;
};

class MarkRendererFactory {
public:
  static std::unique_ptr<MarkRenderer> create(const std::string &type);
};

} // namespace chart
} // namespace flex
