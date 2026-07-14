#pragma once
#include <functional>
#include <memory>
#include <string>
#include <map>
#include "flexchart/mark_renderer.h"

namespace flex {
namespace chart {

class MarkRendererRegistry {
public:
    using Creator = std::function<std::unique_ptr<MarkRenderer>()>;
    static MarkRendererRegistry& instance();
    void register_renderer(const std::string& type, Creator creator);
    std::unique_ptr<MarkRenderer> create(const std::string& type) const;
private:
    std::map<std::string, Creator> registry_;
};

// Pulls the built-in renderer translation units into static-library consumers.
// Registration remains module-local, but consumers no longer need private
// force-link declarations in application code.
void ensure_builtin_mark_renderers_linked();

// Helper macro for auto-registration in each module
#define REGISTER_MARK_RENDERER_CONCAT_(a, b) a##b
#define REGISTER_MARK_RENDERER_CONCAT(a, b) REGISTER_MARK_RENDERER_CONCAT_(a, b)
#define REGISTER_MARK_RENDERER(type, ClassName) \
    static bool REGISTER_MARK_RENDERER_CONCAT(_reg_, __LINE__) = [] { \
        flex::chart::MarkRendererRegistry::instance().register_renderer( \
            type, [] { return std::make_unique<ClassName>(); }); \
        return true; \
    }();

} // namespace chart
} // namespace flex
