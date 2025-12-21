/*
 * Pen Tool Implementation
 */

#include <editor/tool/pen_tool.h>
#include <cmath>
#include <algorithm>

namespace editor {

void PenTool::activate(EditorViewModel* vm) {
    Tool::activate(vm);
    reset();
}

void PenTool::deactivate() {
    finishPath();
    Tool::deactivate();
}

bool PenTool::onMouseDown(const MouseEvent& e) {
    if (e.button != 0) return false;

    // Check if clicking on first point to close path
    if (points_.size() > 2 && hitTestPoint(e.position, points_[0].anchor)) {
        closePath();
        return true;
    }

    // Check if clicking on last point to adjust handles
    if (!points_.empty() && hitTestPoint(e.position, points_.back().anchor)) {
        adjusting_handles_ = true;
        return true;
    }

    // Start new point
    is_dragging_ = true;
    PathPoint pt;
    pt.anchor = e.position;
    points_.push_back(pt);

    return true;
}

bool PenTool::onMouseDrag(const MouseEvent& e) {
    if (!is_dragging_ && !adjusting_handles_) return false;

    if (adjusting_handles_ && !points_.empty()) {
        // Adjust handles of last point
        auto& pt = points_.back();
        pt.handleOut = e.position;
        pt.hasHandleOut = true;

        // Mirror handle for smooth curves
        float dx = e.position.x - pt.anchor.x;
        float dy = e.position.y - pt.anchor.y;
        pt.handleIn = {pt.anchor.x - dx, pt.anchor.y - dy};
        pt.hasHandleIn = true;
        pt.smooth = true;
    } else if (!points_.empty()) {
        // Set handles while creating point
        auto& pt = points_.back();
        pt.handleOut = e.position;
        pt.hasHandleOut = true;

        // Mirror for smooth curve
        float dx = e.position.x - pt.anchor.x;
        float dy = e.position.y - pt.anchor.y;
        pt.handleIn = {pt.anchor.x - dx, pt.anchor.y - dy};
        pt.hasHandleIn = true;
        pt.smooth = true;
    }

    preview_point_ = e.position;
    return true;
}

bool PenTool::onMouseUp(const MouseEvent& e) {
    (void)e;
    is_dragging_ = false;
    adjusting_handles_ = false;
    return true;
}

bool PenTool::onMouseMove(const MouseEvent& e) {
    preview_point_ = e.position;
    return !points_.empty();  // Consume event only when drawing
}

bool PenTool::onKeyDown(const KeyEvent& e) {
    // Escape or Enter to finish path
    if (e.key == 27 || e.key == 13) {  // ESC or Enter
        finishPath();
        return true;
    }

    // Backspace to remove last point
    if (e.key == 8 && !points_.empty()) {
        points_.pop_back();
        return true;
    }

    return false;
}

void PenTool::render(flex::Renderer& renderer) {
    if (points_.empty()) return;

    float viewScale = vm_->camera().screenToWorldScale();
    float lineWidth = 1.5f * viewScale;
    float handleSize = 4.0f * viewScale;

    flex::Color pathColor = {0.0f, 0.6f, 1.0f, 1.0f};
    flex::Color handleColor = {0.4f, 0.4f, 0.4f, 1.0f};
    flex::Color pointColor = {1.0f, 1.0f, 1.0f, 1.0f};

    // Draw path so far
    if (points_.size() > 1) {
        std::string pathStr = buildPathString();
        renderer.stroke_path(pathStr, flex::Paint::solid(pathColor), lineWidth);
    }

    // Draw preview segment to cursor
    if (!is_dragging_ && points_.size() >= 1) {
        std::string previewPath = buildPreviewPath();
        renderer.stroke_path(previewPath, flex::Paint::solid({pathColor.r, pathColor.g, pathColor.b, 0.5f}), lineWidth);
    }

    // Draw points and handles
    for (size_t i = 0; i < points_.size(); ++i) {
        const auto& pt = points_[i];

        // Draw handle lines
        if (pt.hasHandleIn) {
            std::string handleLine = "M " + std::to_string(pt.anchor.x) + " " + std::to_string(pt.anchor.y) +
                " L " + std::to_string(pt.handleIn.x) + " " + std::to_string(pt.handleIn.y);
            renderer.stroke_path(handleLine, flex::Paint::solid(handleColor), lineWidth * 0.5f);
            drawHandle(renderer, pt.handleIn, handleSize * 0.7f, handleColor);
        }
        if (pt.hasHandleOut) {
            std::string handleLine = "M " + std::to_string(pt.anchor.x) + " " + std::to_string(pt.anchor.y) +
                " L " + std::to_string(pt.handleOut.x) + " " + std::to_string(pt.handleOut.y);
            renderer.stroke_path(handleLine, flex::Paint::solid(handleColor), lineWidth * 0.5f);
            drawHandle(renderer, pt.handleOut, handleSize * 0.7f, handleColor);
        }

        // Draw anchor point
        bool isFirst = (i == 0) && points_.size() > 2;
        drawAnchor(renderer, pt.anchor, handleSize, pointColor, pathColor, isFirst);
    }
}

void PenTool::reset() {
    points_.clear();
    is_dragging_ = false;
    adjusting_handles_ = false;
    is_closed_ = false;
}

bool PenTool::hitTestPoint(const Point& p, const Point& target) const {
    float threshold = 8.0f * vm_->camera().screenToWorldScale();
    float dx = p.x - target.x;
    float dy = p.y - target.y;
    return (dx * dx + dy * dy) < (threshold * threshold);
}

std::string PenTool::buildPathString() const {
    if (points_.size() < 2) return "";

    std::string path = "M " + std::to_string(points_[0].anchor.x) + " " + std::to_string(points_[0].anchor.y);

    for (size_t i = 1; i < points_.size(); ++i) {
        const auto& prev = points_[i - 1];
        const auto& curr = points_[i];

        if (prev.hasHandleOut || curr.hasHandleIn) {
            // Cubic bezier
            Point cp1 = prev.hasHandleOut ? prev.handleOut : prev.anchor;
            Point cp2 = curr.hasHandleIn ? curr.handleIn : curr.anchor;

            path += " C " + std::to_string(cp1.x) + " " + std::to_string(cp1.y) +
                " " + std::to_string(cp2.x) + " " + std::to_string(cp2.y) +
                " " + std::to_string(curr.anchor.x) + " " + std::to_string(curr.anchor.y);
        } else {
            // Line
            path += " L " + std::to_string(curr.anchor.x) + " " + std::to_string(curr.anchor.y);
        }
    }

    if (is_closed_) {
        path += " Z";
    }

    return path;
}

std::string PenTool::buildPreviewPath() const {
    if (points_.empty()) return "";

    const auto& last = points_.back();

    if (last.hasHandleOut) {
        // Cubic bezier preview
        return "M " + std::to_string(last.anchor.x) + " " + std::to_string(last.anchor.y) +
            " C " + std::to_string(last.handleOut.x) + " " + std::to_string(last.handleOut.y) +
            " " + std::to_string(preview_point_.x) + " " + std::to_string(preview_point_.y) +
            " " + std::to_string(preview_point_.x) + " " + std::to_string(preview_point_.y);
    } else {
        // Line preview
        return "M " + std::to_string(last.anchor.x) + " " + std::to_string(last.anchor.y) +
            " L " + std::to_string(preview_point_.x) + " " + std::to_string(preview_point_.y);
    }
}

void PenTool::drawAnchor(flex::Renderer& renderer, const Point& p, float size,
                         const flex::Color& fill, const flex::Color& stroke, bool highlight) {
    std::string path = "M " + std::to_string(p.x - size) + " " + std::to_string(p.y - size) +
        " h " + std::to_string(size * 2) +
        " v " + std::to_string(size * 2) +
        " h " + std::to_string(-size * 2) + " Z";

    renderer.fill_path(path, flex::Paint::solid(highlight ? flex::Color{0.4f, 0.8f, 0.4f, 1.0f} : fill));
    renderer.stroke_path(path, flex::Paint::solid(stroke), vm_->camera().screenToWorldScale());
}

void PenTool::drawHandle(flex::Renderer& renderer, const Point& p, float size, const flex::Color& color) {
    // Draw circle for handle
    float k = 0.5522848f * size;
    std::string path =
        "M " + std::to_string(p.x) + " " + std::to_string(p.y - size) +
        " C " + std::to_string(p.x + k) + " " + std::to_string(p.y - size) +
        " " + std::to_string(p.x + size) + " " + std::to_string(p.y - k) +
        " " + std::to_string(p.x + size) + " " + std::to_string(p.y) +
        " C " + std::to_string(p.x + size) + " " + std::to_string(p.y + k) +
        " " + std::to_string(p.x + k) + " " + std::to_string(p.y + size) +
        " " + std::to_string(p.x) + " " + std::to_string(p.y + size) +
        " C " + std::to_string(p.x - k) + " " + std::to_string(p.y + size) +
        " " + std::to_string(p.x - size) + " " + std::to_string(p.y + k) +
        " " + std::to_string(p.x - size) + " " + std::to_string(p.y) +
        " C " + std::to_string(p.x - size) + " " + std::to_string(p.y - k) +
        " " + std::to_string(p.x - k) + " " + std::to_string(p.y - size) +
        " " + std::to_string(p.x) + " " + std::to_string(p.y - size) + " Z";

    renderer.fill_path(path, flex::Paint::solid(color));
}

void PenTool::closePath() {
    if (points_.size() < 3) return;
    is_closed_ = true;
    finishPath();
}

void PenTool::finishPath() {
    if (points_.size() < 2) {
        reset();
        return;
    }

    // Calculate bounds for position
    float minX = points_[0].anchor.x, maxX = minX;
    float minY = points_[0].anchor.y, maxY = minY;
    for (const auto& pt : points_) {
        minX = std::min(minX, pt.anchor.x);
        maxX = std::max(maxX, pt.anchor.x);
        minY = std::min(minY, pt.anchor.y);
        maxY = std::max(maxY, pt.anchor.y);
        if (pt.hasHandleIn) {
            minX = std::min(minX, pt.handleIn.x);
            maxX = std::max(maxX, pt.handleIn.x);
            minY = std::min(minY, pt.handleIn.y);
            maxY = std::max(maxY, pt.handleIn.y);
        }
        if (pt.hasHandleOut) {
            minX = std::min(minX, pt.handleOut.x);
            maxX = std::max(maxX, pt.handleOut.x);
            minY = std::min(minY, pt.handleOut.y);
            maxY = std::max(maxY, pt.handleOut.y);
        }
    }

    // Build path string with coordinates relative to (minX, minY)
    auto offsetX = [minX](float x) { return x - minX; };
    auto offsetY = [minY](float y) { return y - minY; };

    std::string pathData = "M " + std::to_string(offsetX(points_[0].anchor.x)) + " " +
                           std::to_string(offsetY(points_[0].anchor.y));

    for (size_t i = 1; i < points_.size(); ++i) {
        const auto& prev = points_[i - 1];
        const auto& curr = points_[i];

        if (prev.hasHandleOut || curr.hasHandleIn) {
            Point cp1 = prev.hasHandleOut ? prev.handleOut : prev.anchor;
            Point cp2 = curr.hasHandleIn ? curr.handleIn : curr.anchor;

            pathData += " C " + std::to_string(offsetX(cp1.x)) + " " + std::to_string(offsetY(cp1.y)) +
                " " + std::to_string(offsetX(cp2.x)) + " " + std::to_string(offsetY(cp2.y)) +
                " " + std::to_string(offsetX(curr.anchor.x)) + " " + std::to_string(offsetY(curr.anchor.y));
        } else {
            pathData += " L " + std::to_string(offsetX(curr.anchor.x)) + " " + std::to_string(offsetY(curr.anchor.y));
        }
    }

    if (is_closed_) {
        pathData += " Z";
    }

    // Create path shape
    auto shape = flex::Shape::create();
    shape->set_position(minX, minY);
    shape->set_path(pathData, maxX - minX, maxY - minY);
    shape->set_stroke(flex::Color{0.0f, 0.0f, 0.0f, 1.0f}, 2.0f);

    if (is_closed_) {
        shape->set_fill(flex::Color{0.8f, 0.8f, 0.8f, 0.5f});
    }

    auto node = ShapeNode::create(shape);
    node->setName("Path");
    vm_->executeCommand(std::make_unique<CreateNodeCommand>(vm_, node));

    reset();
}

} // namespace editor
