#pragma once

#include <ir/unified_infographic.h>
#include <flex/runtime/allocator.h>
#include <flex/runtime/group.h>
#include <memory>
#include <string>

namespace flex {
class Instance;
}

namespace flex::modules::infographic {

// Render context passed to all layout renderers
struct LayoutRenderContext {
    ArenaAllocator& arena;
    Group* root;
    float width;
    float height;
    Instance* instance;
};

// Base class for layout renderers
class LayoutRenderer {
public:
    virtual ~LayoutRenderer() = default;
    virtual void render(const UnifiedInfographic& info, LayoutRenderContext& ctx) = 0;
};

// Factory for creating layout renderers
class LayoutRendererFactory {
public:
    static std::unique_ptr<LayoutRenderer> create(LayoutType type);
};

// Concrete renderers
class GridLayoutRenderer : public LayoutRenderer {
public:
    void render(const UnifiedInfographic& info, LayoutRenderContext& ctx) override;
};

class TimelineLayoutRenderer : public LayoutRenderer {
public:
    void render(const UnifiedInfographic& info, LayoutRenderContext& ctx) override;
};

class FunnelLayoutRenderer : public LayoutRenderer {
public:
    void render(const UnifiedInfographic& info, LayoutRenderContext& ctx) override;
};

class PieLayoutRenderer : public LayoutRenderer {
public:
    void render(const UnifiedInfographic& info, LayoutRenderContext& ctx) override;
};

class DonutLayoutRenderer : public LayoutRenderer {
public:
    void render(const UnifiedInfographic& info, LayoutRenderContext& ctx) override;
};

class BarLayoutRenderer : public LayoutRenderer {
public:
    void render(const UnifiedInfographic& info, LayoutRenderContext& ctx) override;
};

class SwotLayoutRenderer : public LayoutRenderer {
public:
    void render(const UnifiedInfographic& info, LayoutRenderContext& ctx) override;
};

class VsCompareLayoutRenderer : public LayoutRenderer {
public:
    void render(const UnifiedInfographic& info, LayoutRenderContext& ctx) override;
};

class TreeLayoutRenderer : public LayoutRenderer {
public:
    void render(const UnifiedInfographic& info, LayoutRenderContext& ctx) override;
};

class ZigzagLayoutRenderer : public LayoutRenderer {
public:
    void render(const UnifiedInfographic& info, LayoutRenderContext& ctx) override;
};

class CircularLayoutRenderer : public LayoutRenderer {
public:
    void render(const UnifiedInfographic& info, LayoutRenderContext& ctx) override;
};

class RoadmapLayoutRenderer : public LayoutRenderer {
public:
    void render(const UnifiedInfographic& info, LayoutRenderContext& ctx) override;
};

} // namespace flex::modules::infographic
