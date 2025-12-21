/*
 * Path Editor Implementation
 */

#include <editor/tool/path_editor.h>
#include <editor/viewmodel/editor_vm.h>
#include <cmath>
#include <algorithm>

namespace editor {

PathEditor::PathEditor() {}

void PathEditor::setEditingNode(ShapeNode* node) {
    editing_node_ = node;
    points_.clear();

    if (!node) return;

    // Extract path points from the shape
    // This is a simplified version - real implementation would parse SVG path data
    auto bounds = node->bounds();

    // For now, create corner points for a rectangle shape
    // TODO: Parse actual path data from the shape
    points_.push_back({{bounds.x, bounds.y}, {-20, 0}, {20, 0}, PointType::Corner});
    points_.push_back({{bounds.x + bounds.width, bounds.y}, {-20, 0}, {20, 0}, PointType::Corner});
    points_.push_back({{bounds.x + bounds.width, bounds.y + bounds.height}, {-20, 0}, {20, 0}, PointType::Corner});
    points_.push_back({{bounds.x, bounds.y + bounds.height}, {-20, 0}, {20, 0}, PointType::Corner});
}

void PathEditor::endEditing() {
    if (editing_node_) {
        updateNodeFromPoints();
    }
    editing_node_ = nullptr;
    points_.clear();
    deselectAll();
}

void PathEditor::setPoints(const std::vector<PathEditPoint>& pts) {
    points_ = pts;
}

void PathEditor::selectPoint(size_t index, bool addToSelection) {
    if (index >= points_.size()) return;

    if (!addToSelection) {
        deselectAll();
    }
    points_[index].selected = true;
}

void PathEditor::selectAll() {
    for (auto& pt : points_) {
        pt.selected = true;
    }
}

void PathEditor::deselectAll() {
    for (auto& pt : points_) {
        pt.selected = false;
    }
}

std::vector<size_t> PathEditor::selectedIndices() const {
    std::vector<size_t> result;
    for (size_t i = 0; i < points_.size(); ++i) {
        if (points_[i].selected) {
            result.push_back(i);
        }
    }
    return result;
}

void PathEditor::deleteSelectedPoints() {
    points_.erase(
        std::remove_if(points_.begin(), points_.end(),
            [](const PathEditPoint& p) { return p.selected; }),
        points_.end());
    updateNodeFromPoints();
}

void PathEditor::setSelectedPointType(PointType type) {
    for (auto& pt : points_) {
        if (pt.selected) {
            pt.type = type;
        }
    }
}

void PathEditor::insertPointAt(const Point& position, size_t afterIndex) {
    if (afterIndex >= points_.size()) {
        points_.push_back({position, {-20, 0}, {20, 0}, PointType::Smooth});
    } else {
        points_.insert(points_.begin() + afterIndex + 1,
            {position, {-20, 0}, {20, 0}, PointType::Smooth});
    }
    updateNodeFromPoints();
}

PathEditor::HitResult PathEditor::hitTest(const Point& worldPos) const {
    HitResult result;
    float hitRadius = style_.hit_radius;

    for (size_t i = 0; i < points_.size(); ++i) {
        const auto& pt = points_[i];

        // Check handle in (only if point is selected)
        if (pt.selected) {
            Point handleInWorld = {pt.position.x + pt.handleIn.x, pt.position.y + pt.handleIn.y};
            float dIn = std::sqrt(std::pow(worldPos.x - handleInWorld.x, 2) +
                                  std::pow(worldPos.y - handleInWorld.y, 2));
            if (dIn < hitRadius) {
                result.pointIndex = static_cast<int>(i);
                result.handleType = PathHandleType::HandleIn;
                return result;
            }

            Point handleOutWorld = {pt.position.x + pt.handleOut.x, pt.position.y + pt.handleOut.y};
            float dOut = std::sqrt(std::pow(worldPos.x - handleOutWorld.x, 2) +
                                   std::pow(worldPos.y - handleOutWorld.y, 2));
            if (dOut < hitRadius) {
                result.pointIndex = static_cast<int>(i);
                result.handleType = PathHandleType::HandleOut;
                return result;
            }
        }

        // Check main point
        float d = std::sqrt(std::pow(worldPos.x - pt.position.x, 2) +
                           std::pow(worldPos.y - pt.position.y, 2));
        if (d < hitRadius) {
            result.pointIndex = static_cast<int>(i);
            result.handleType = PathHandleType::Point;
            return result;
        }
    }

    return result;
}

void PathEditor::applyHandleConstraints(size_t pointIndex, PathHandleType movedHandle) {
    if (pointIndex >= points_.size()) return;

    auto& pt = points_[pointIndex];

    if (pt.type == PointType::Corner) {
        // No constraints for corners
        return;
    }

    if (pt.type == PointType::Smooth || pt.type == PointType::Symmetric) {
        // Align opposite handle
        Point* moved = (movedHandle == PathHandleType::HandleIn) ? &pt.handleIn : &pt.handleOut;
        Point* other = (movedHandle == PathHandleType::HandleIn) ? &pt.handleOut : &pt.handleIn;

        float movedLen = std::sqrt(moved->x * moved->x + moved->y * moved->y);
        if (movedLen > 0.001f) {
            float otherLen = (pt.type == PointType::Symmetric) ? movedLen :
                std::sqrt(other->x * other->x + other->y * other->y);

            // Opposite direction
            other->x = -moved->x / movedLen * otherLen;
            other->y = -moved->y / movedLen * otherLen;
        }
    }
}

void PathEditor::updateNodeFromPoints() {
    if (!editing_node_ || points_.empty()) return;

    // Build SVG path string from points
    // TODO: Implement when ShapeNode supports path modification
    // For now, path editing is visual only - changes are not persisted

    /*
    std::string path = "M " + std::to_string(points_[0].position.x) + " " +
                       std::to_string(points_[0].position.y);

    for (size_t i = 1; i < points_.size(); ++i) {
        const auto& prev = points_[i - 1];
        const auto& curr = points_[i];

        // Use cubic bezier
        Point cp1 = {prev.position.x + prev.handleOut.x, prev.position.y + prev.handleOut.y};
        Point cp2 = {curr.position.x + curr.handleIn.x, curr.position.y + curr.handleIn.y};

        path += " C " + std::to_string(cp1.x) + " " + std::to_string(cp1.y) + " " +
                std::to_string(cp2.x) + " " + std::to_string(cp2.y) + " " +
                std::to_string(curr.position.x) + " " + std::to_string(curr.position.y);
    }

    // Close path
    if (points_.size() > 2) {
        const auto& last = points_.back();
        const auto& first = points_[0];
        Point cp1 = {last.position.x + last.handleOut.x, last.position.y + last.handleOut.y};
        Point cp2 = {first.position.x + first.handleIn.x, first.position.y + first.handleIn.y};

        path += " C " + std::to_string(cp1.x) + " " + std::to_string(cp1.y) + " " +
                std::to_string(cp2.x) + " " + std::to_string(cp2.y) + " " +
                std::to_string(first.position.x) + " " + std::to_string(first.position.y);
        path += " Z";
    }

    editing_node_->setPath(path);
    */
}

void PathEditor::render(flex::Renderer& renderer, const Transform2D& viewTransform) {
    if (!isEditing() || points_.empty()) return;

    // Draw path outline
    std::string pathStr = "M " + std::to_string(points_[0].position.x) + " " +
                          std::to_string(points_[0].position.y);
    for (size_t i = 1; i < points_.size(); ++i) {
        pathStr += " L " + std::to_string(points_[i].position.x) + " " +
                   std::to_string(points_[i].position.y);
    }
    pathStr += " Z";

    // Transform and draw
    // Note: For simplicity, drawing in world coords - real impl would transform

    renderer.stroke_path(pathStr, flex::Paint::solid({style_.path_color.r, style_.path_color.g,
        style_.path_color.b, style_.path_color.a}), 1.5f);

    // Draw handles and points
    for (size_t i = 0; i < points_.size(); ++i) {
        const auto& pt = points_[i];
        bool isHovered = (static_cast<int>(i) == hovered_point_);

        // Draw handle lines and circles for selected points
        if (pt.selected) {
            Point hIn = {pt.position.x + pt.handleIn.x, pt.position.y + pt.handleIn.y};
            Point hOut = {pt.position.x + pt.handleOut.x, pt.position.y + pt.handleOut.y};

            // Handle lines
            std::string lineIn = "M " + std::to_string(pt.position.x) + " " +
                std::to_string(pt.position.y) + " L " +
                std::to_string(hIn.x) + " " + std::to_string(hIn.y);
            renderer.stroke_path(lineIn, flex::Paint::solid({style_.handle_line.r,
                style_.handle_line.g, style_.handle_line.b, style_.handle_line.a}),
                style_.handle_line_width);

            std::string lineOut = "M " + std::to_string(pt.position.x) + " " +
                std::to_string(pt.position.y) + " L " +
                std::to_string(hOut.x) + " " + std::to_string(hOut.y);
            renderer.stroke_path(lineOut, flex::Paint::solid({style_.handle_line.r,
                style_.handle_line.g, style_.handle_line.b, style_.handle_line.a}),
                style_.handle_line_width);

            // Handle circles
            float hs = style_.handle_size / 2;
            std::string hInCircle = "M " + std::to_string(hIn.x - hs) + " " + std::to_string(hIn.y) +
                " a " + std::to_string(hs) + " " + std::to_string(hs) + " 0 1 1 " +
                std::to_string(hs * 2) + " 0 a " + std::to_string(hs) + " " + std::to_string(hs) +
                " 0 1 1 " + std::to_string(-hs * 2) + " 0";
            renderer.fill_path(hInCircle, flex::Paint::solid({style_.handle_color.r,
                style_.handle_color.g, style_.handle_color.b, 1}));

            std::string hOutCircle = "M " + std::to_string(hOut.x - hs) + " " + std::to_string(hOut.y) +
                " a " + std::to_string(hs) + " " + std::to_string(hs) + " 0 1 1 " +
                std::to_string(hs * 2) + " 0 a " + std::to_string(hs) + " " + std::to_string(hs) +
                " 0 1 1 " + std::to_string(-hs * 2) + " 0";
            renderer.fill_path(hOutCircle, flex::Paint::solid({style_.handle_color.r,
                style_.handle_color.g, style_.handle_color.b, 1}));
        }

        // Draw main point
        float ps = style_.point_size / 2;
        Color pointColor = pt.selected ? style_.point_selected :
                          (isHovered ? style_.point_hover : style_.point_normal);

        std::string pointRect = "M " + std::to_string(pt.position.x - ps) + " " +
            std::to_string(pt.position.y - ps) + " h " + std::to_string(ps * 2) +
            " v " + std::to_string(ps * 2) + " h " + std::to_string(-ps * 2) + " Z";
        renderer.fill_path(pointRect, flex::Paint::solid({pointColor.r, pointColor.g,
            pointColor.b, pointColor.a}));
        renderer.stroke_path(pointRect, flex::Paint::solid({0, 0, 0, 1}), 1.0f);
    }
}

bool PathEditor::onMouseDown(const Point& worldPos, int button, bool shift) {
    if (!isEditing()) return false;

    if (button == 0) {
        auto hit = hitTest(worldPos);

        if (hit.pointIndex >= 0) {
            if (hit.handleType == PathHandleType::Point) {
                selectPoint(hit.pointIndex, shift);
            }

            dragging_ = true;
            drag_point_ = hit.pointIndex;
            drag_handle_ = hit.handleType;
            drag_start_ = worldPos;
            return true;
        } else {
            // Click on empty space - deselect
            if (!shift) {
                deselectAll();
            }
        }
    }

    return false;
}

bool PathEditor::onMouseMove(const Point& worldPos) {
    if (!isEditing()) return false;

    if (dragging_ && drag_point_ >= 0) {
        auto& pt = points_[drag_point_];
        float dx = worldPos.x - drag_start_.x;
        float dy = worldPos.y - drag_start_.y;

        if (drag_handle_ == PathHandleType::Point) {
            // Move the point (and all selected points)
            for (auto& p : points_) {
                if (p.selected) {
                    p.position.x += dx;
                    p.position.y += dy;
                }
            }
        } else if (drag_handle_ == PathHandleType::HandleIn) {
            pt.handleIn.x = worldPos.x - pt.position.x;
            pt.handleIn.y = worldPos.y - pt.position.y;
            applyHandleConstraints(drag_point_, PathHandleType::HandleIn);
        } else if (drag_handle_ == PathHandleType::HandleOut) {
            pt.handleOut.x = worldPos.x - pt.position.x;
            pt.handleOut.y = worldPos.y - pt.position.y;
            applyHandleConstraints(drag_point_, PathHandleType::HandleOut);
        }

        drag_start_ = worldPos;
        return true;
    }

    // Update hover state
    auto hit = hitTest(worldPos);
    hovered_point_ = hit.pointIndex;
    hovered_handle_ = hit.handleType;

    return hovered_point_ >= 0;
}

bool PathEditor::onMouseUp(const Point& worldPos, int button) {
    (void)worldPos; (void)button;

    if (dragging_) {
        dragging_ = false;
        drag_point_ = -1;
        drag_handle_ = PathHandleType::None;
        updateNodeFromPoints();
        return true;
    }

    return false;
}

bool PathEditor::onKeyDown(int key) {
    if (!isEditing()) return false;

    // Delete key
    if (key == 127 || key == 8) {
        deleteSelectedPoints();
        return true;
    }

    // 1, 2, 3 for point types
    if (key == '1') {
        setSelectedPointType(PointType::Corner);
        return true;
    }
    if (key == '2') {
        setSelectedPointType(PointType::Smooth);
        return true;
    }
    if (key == '3') {
        setSelectedPointType(PointType::Symmetric);
        return true;
    }

    // A for select all
    if (key == 'A' || key == 'a') {
        selectAll();
        return true;
    }

    return false;
}

} // namespace editor
