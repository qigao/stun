/*
 * Meta Editor - Connector Tool
 *
 * Draw lines that connect shapes. When shapes move, connectors follow.
 * Essential for whiteboard/diagram applications.
 *
 * Usage:
 * - Click on shape edge to start connector
 * - Drag to another shape edge to connect
 * - Or release in empty space for free endpoint
 */

#pragma once

#include "../tool.h"
#include "../connector.h"
#include <flex.h>

namespace meta_editor {

class ConnectorTool : public Tool {
public:
    ConnectorTool() = default;

    void set_connector_manager(ConnectorManager* mgr) { connector_mgr_ = mgr; }

    bool on_pointer_down(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_move(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;
    bool on_pointer_up(const flex::Vec2& screen_pos, const flex::Vec2& world_pos) override;

    void render_overlay(flex::Renderer& renderer) override;

    const char* name() const override { return "Connector"; }
    const char* icon() const override { return "connector"; }

    void set_stroke_color(const flex::Color& color) { stroke_color_ = color; }
    void set_stroke_width(float width) { stroke_width_ = width; }

private:
    // Find shape under cursor and determine which edge is closest
    struct HitResult {
        flex::Node* node = nullptr;
        ConnectionSide side = ConnectionSide::Center;
        flex::Vec2 point;
    };
    
    HitResult find_connection_target(const flex::Vec2& world_pos);
    ConnectionSide closest_side(flex::Node* node, const flex::Vec2& pos);

    ConnectorManager* connector_mgr_ = nullptr;
    
    bool is_drawing_ = false;
    flex::Vec2 start_pos_;
    flex::Vec2 current_pos_;
    HitResult start_target_;
    HitResult hover_target_;

    flex::Color stroke_color_ = flex::Color::Black;
    float stroke_width_ = 2.0f;
};

} // namespace meta_editor
