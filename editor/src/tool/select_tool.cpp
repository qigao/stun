/*
 * Select Tool Implementation
 */

#include <editor/tool/select_tool.h>
#include <editor/model/node.h>
#include <cmath>

namespace editor {

void SelectTool::activate(EditorViewModel* vm) {
    Tool::activate(vm);
    updateGizmo();
}

bool SelectTool::onMouseDown(const MouseEvent& e) {
    if (e.button != 0) return false;

    drag_start_ = e.position;
    last_pos_ = e.position;
    is_dragging_ = false;

    // Update gizmo first
    updateGizmo();

    // Hit test transform handles first (using screen-to-world scale)
    if (!vm_->selection().isEmpty()) {
        active_handle_ = gizmo_.hitTest(e.position, vm_->camera().screenToWorldScale());

        if (active_handle_ != HandleType::None) {
            is_dragging_ = true;

            if (active_handle_ == HandleType::Move) {
                mode_ = Mode::Move;
                // Collect snap targets for move operation
                snapHelper_.collectTargets(vm_->document()->allNodes(),
                                          vm_->selection().selection());
                snapHelper_.addCanvasBounds(vm_->document()->width(),
                                           vm_->document()->height());
            } else if (active_handle_ == HandleType::Rotate) {
                mode_ = Mode::Rotate;
                // Calculate initial angle
                start_angle_ = std::atan2(e.position.y - gizmo_.pivot.y,
                                          e.position.x - gizmo_.pivot.x);
            } else {
                mode_ = Mode::Resize;
            }

            // Store original transforms
            storeOriginalTransforms();
            return true;
        }
    }

    // Hit test for nodes
    auto* doc = vm_->document();
    auto node = doc->nodeAt(e.position);

    if (node) {
        if (e.shift) {
            vm_->selection().select(node, SelectionManager::Mode::Toggle);
        } else if (!vm_->selection().isSelected(node)) {
            vm_->selection().select(node, SelectionManager::Mode::Replace);
        }
        mode_ = Mode::Move;
        storeOriginalTransforms();
        // Collect snap targets for move operation
        snapHelper_.collectTargets(vm_->document()->allNodes(),
                                  vm_->selection().selection());
        snapHelper_.addCanvasBounds(vm_->document()->width(),
                                   vm_->document()->height());
    } else {
        if (!e.shift) {
            vm_->selection().clear();
        }
        mode_ = Mode::SelectBox;
        selection_box_ = {e.position.x, e.position.y, 0, 0};
    }

    updateGizmo();
    return true;
}

bool SelectTool::onMouseDrag(const MouseEvent& e) {
    if (e.button != 0) return false;

    float dx = e.position.x - drag_start_.x;
    float dy = e.position.y - drag_start_.y;

    if (!is_dragging_) {
        if (std::abs(dx) > 2 || std::abs(dy) > 2) {
            is_dragging_ = true;
            if (mode_ == Mode::Move) {
                storeOriginalTransforms();
            }
        }
    }

    if (!is_dragging_) {
        last_pos_ = e.position;
        return true;
    }

    switch (mode_) {
        case Mode::Move:
            performMove(e);
            break;
        case Mode::Resize:
            performResize(e);
            break;
        case Mode::Rotate:
            performRotate(e);
            break;
        case Mode::SelectBox:
            selection_box_.width = e.position.x - selection_box_.x;
            selection_box_.height = e.position.y - selection_box_.y;
            break;
        default:
            break;
    }

    last_pos_ = e.position;
    updateGizmo();
    return true;
}

bool SelectTool::onMouseUp(const MouseEvent& e) {
    if (e.button != 0) return false;

    if (is_dragging_) {
        commitTransform(e);
    }

    // Handle selection box
    if (mode_ == Mode::SelectBox && is_dragging_) {
        Rect rect = normalizeRect(selection_box_);
        auto mode = e.shift ? SelectionManager::Mode::Add : SelectionManager::Mode::Replace;
        vm_->selection().selectRect(rect, vm_->document()->allNodes(), mode);
    }

    // Clear snap state
    activeGuides_.clear();
    snapHelper_.clearTargets();

    is_dragging_ = false;
    mode_ = Mode::None;
    active_handle_ = HandleType::None;
    updateGizmo();
    return true;
}

bool SelectTool::onMouseMove(const MouseEvent& e) {
    // Update cursor based on handle hover
    if (!vm_->selection().isEmpty()) {
        auto handle = gizmo_.hitTest(e.position, vm_->camera().screenToWorldScale());
        // Could set cursor here based on handle type
        (void)handle;
    }
    return false;
}

bool SelectTool::onKeyDown(const KeyEvent& e) {
    // Delete
    if (e.key == 127 || e.key == 8) {
        if (!vm_->selection().isEmpty()) {
            auto nodes = vm_->selection().selection();
            vm_->selection().clear();
            vm_->executeCommand(std::make_unique<DeleteNodesCommand>(nodes));
        }
        return true;
    }

    // Arrow keys for nudge
    float nudge = e.shift ? 10.0f : 1.0f;
    if (e.key == 1073741903) { // Right
        nudgeSelection(nudge, 0);
        return true;
    }
    if (e.key == 1073741904) { // Left
        nudgeSelection(-nudge, 0);
        return true;
    }
    if (e.key == 1073741906) { // Up
        nudgeSelection(0, -nudge);
        return true;
    }
    if (e.key == 1073741905) { // Down
        nudgeSelection(0, nudge);
        return true;
    }

    return false;
}

void SelectTool::render(flex::Renderer& renderer) {
    float viewScale = vm_->camera().screenToWorldScale();

    // Draw selection box
    if (is_dragging_ && mode_ == Mode::SelectBox) {
        Rect rect = normalizeRect(selection_box_);
        std::string path = "M " + std::to_string(rect.x) + " " + std::to_string(rect.y) +
            " h " + std::to_string(rect.width) +
            " v " + std::to_string(rect.height) +
            " h " + std::to_string(-rect.width) + " Z";

        renderer.fill_path(path, flex::Paint::solid({0.2f, 0.5f, 1.0f, 0.1f}));
        renderer.stroke_path(path, flex::Paint::solid({0.2f, 0.5f, 1.0f, 0.8f}), viewScale);
    }

    // Draw snap guides during move
    if (is_dragging_ && mode_ == Mode::Move && !activeGuides_.empty()) {
        flex::Color guideColor = {1.0f, 0.0f, 0.5f, 0.8f};  // Magenta

        // Get visible bounds for guide lines (extend beyond viewport)
        float docW = vm_->document()->width();
        float docH = vm_->document()->height();

        for (float x : activeGuides_.vertical) {
            std::string path = "M " + std::to_string(x) + " 0 v " + std::to_string(docH);
            renderer.stroke_path(path, flex::Paint::solid(guideColor), viewScale);
        }
        for (float y : activeGuides_.horizontal) {
            std::string path = "M 0 " + std::to_string(y) + " h " + std::to_string(docW);
            renderer.stroke_path(path, flex::Paint::solid(guideColor), viewScale);
        }
    }

    // Draw transform gizmo
    gizmo_.render(renderer, vm_->camera());
}

void SelectTool::updateGizmo() {
    if (vm_->selection().isEmpty()) {
        gizmo_.visible = false;
        return;
    }

    gizmo_.visible = true;
    gizmo_.bounds = vm_->selection().bounds();

    // Get average rotation if single selection
    if (vm_->selection().count() == 1) {
        gizmo_.rotation = vm_->selection().primary()->rotation();
    } else {
        gizmo_.rotation = 0;
    }

    gizmo_.updateHandles();
}

void SelectTool::storeOriginalTransforms() {
    original_transforms_.clear();
    for (auto& node : vm_->selection().selection()) {
        original_transforms_.push_back(node->transform());
    }
    original_bounds_ = vm_->selection().bounds();
}

void SelectTool::performMove(const MouseEvent& e) {
    // Calculate total delta from drag start
    float totalDx = e.position.x - drag_start_.x;
    float totalDy = e.position.y - drag_start_.y;

    // Apply snapping to the total movement
    float snappedDx = totalDx;
    float snappedDy = totalDy;

    // Set threshold based on view scale (8 pixels in screen space)
    float viewScale = vm_->camera().screenToWorldScale();
    snapHelper_.setThreshold(8.0f * viewScale);

    // Calculate snap (use original bounds, not current)
    activeGuides_ = snapHelper_.calculateSnap(original_bounds_, totalDx, totalDy,
                                              &snappedDx, &snappedDy);

    // Apply snapped position (restore to original then apply total delta)
    auto& sel = vm_->selection().selection();
    for (size_t i = 0; i < sel.size(); ++i) {
        Transform2D snappedTransform = original_transforms_[i];
        snappedTransform.tx += snappedDx;
        snappedTransform.ty += snappedDy;
        sel[i]->setTransform(snappedTransform);
    }
}

void SelectTool::performResize(const MouseEvent& e) {
    auto& sel = vm_->selection().selection();
    if (sel.empty()) return;

    // Calculate scale factors based on handle and movement
    Point pivot = gizmo_.pivot;
    float dx = e.position.x - drag_start_.x;
    float dy = e.position.y - drag_start_.y;

    float scaleX = 1.0f, scaleY = 1.0f;
    float w = original_bounds_.width;
    float h = original_bounds_.height;

    if (w < 1) w = 1;
    if (h < 1) h = 1;

    switch (active_handle_) {
        case HandleType::TopLeft:
            scaleX = (w - dx) / w;
            scaleY = (h - dy) / h;
            break;
        case HandleType::TopRight:
            scaleX = (w + dx) / w;
            scaleY = (h - dy) / h;
            break;
        case HandleType::BottomLeft:
            scaleX = (w - dx) / w;
            scaleY = (h + dy) / h;
            break;
        case HandleType::BottomRight:
            scaleX = (w + dx) / w;
            scaleY = (h + dy) / h;
            break;
        case HandleType::Top:
            scaleY = (h - dy) / h;
            break;
        case HandleType::Bottom:
            scaleY = (h + dy) / h;
            break;
        case HandleType::Left:
            scaleX = (w - dx) / w;
            break;
        case HandleType::Right:
            scaleX = (w + dx) / w;
            break;
        default:
            break;
    }

    // Constrain proportions with Shift
    if (e.shift) {
        float scale = std::max(scaleX, scaleY);
        scaleX = scaleY = scale;
    }

    // Prevent negative scale
    scaleX = std::max(0.01f, scaleX);
    scaleY = std::max(0.01f, scaleY);

    // For single shape selection, resize the geometry directly
    if (sel.size() == 1) {
        if (auto* shapeNode = dynamic_cast<ShapeNode*>(sel[0].get())) {
            auto* shape = shapeNode->shape();
            auto geomType = shape->geometry_type();

            // Calculate new position based on pivot
            float newX = pivot.x + (original_bounds_.x - pivot.x) * scaleX;
            float newY = pivot.y + (original_bounds_.y - pivot.y) * scaleY;

            switch (geomType) {
                case flex::GeometryType::Rect: {
                    float newW = original_bounds_.width * scaleX;
                    float newH = original_bounds_.height * scaleY;
                    shape->set_rect(newW, newH);
                    shape->set_position(newX, newY);
                    break;
                }
                case flex::GeometryType::Circle: {
                    float avgScale = (scaleX + scaleY) / 2.0f;
                    float originalRadius = original_bounds_.width / 2.0f;
                    shape->set_circle(originalRadius * avgScale);
                    // For circle, position is center
                    float cx = original_bounds_.x + original_bounds_.width / 2;
                    float cy = original_bounds_.y + original_bounds_.height / 2;
                    float newCx = pivot.x + (cx - pivot.x) * scaleX;
                    float newCy = pivot.y + (cy - pivot.y) * scaleY;
                    shape->set_position(newCx, newCy);
                    break;
                }
                case flex::GeometryType::Ellipse: {
                    float originalRx = original_bounds_.width / 2.0f;
                    float originalRy = original_bounds_.height / 2.0f;
                    shape->set_ellipse(originalRx * scaleX, originalRy * scaleY);
                    // For ellipse, position is center
                    float cx = original_bounds_.x + original_bounds_.width / 2;
                    float cy = original_bounds_.y + original_bounds_.height / 2;
                    float newCx = pivot.x + (cx - pivot.x) * scaleX;
                    float newCy = pivot.y + (cy - pivot.y) * scaleY;
                    shape->set_position(newCx, newCy);
                    break;
                }
                default:
                    // For other types, use transform scaling
                    Transform2D scaleTransform = Transform2D::scaleAround(scaleX, scaleY, pivot);
                    sel[0]->setTransform(scaleTransform * original_transforms_[0]);
                    break;
            }
            // Sync transform from flex after geometry change
            shapeNode->syncTransformFromFlex();
            return;
        }
    }

    // For multiple selection or non-shape nodes, use transform scaling
    Transform2D scaleTransform = Transform2D::scaleAround(scaleX, scaleY, pivot);
    for (size_t i = 0; i < sel.size(); ++i) {
        sel[i]->setTransform(scaleTransform * original_transforms_[i]);
    }
}

void SelectTool::performRotate(const MouseEvent& e) {
    auto& sel = vm_->selection().selection();
    if (sel.empty()) return;

    // Calculate rotation angle
    float current_angle = std::atan2(e.position.y - gizmo_.pivot.y,
                                     e.position.x - gizmo_.pivot.x);
    float delta_angle = current_angle - start_angle_;

    // Snap to 15 degree increments with Shift
    if (e.shift) {
        float degrees = delta_angle * 180.0f / 3.14159265f;
        degrees = std::round(degrees / 15.0f) * 15.0f;
        delta_angle = degrees * 3.14159265f / 180.0f;
    }

    // Apply rotation around pivot using Transform2D
    Transform2D rotateTransform = Transform2D::rotationAround(delta_angle, gizmo_.pivot);

    for (size_t i = 0; i < sel.size(); ++i) {
        sel[i]->setTransform(rotateTransform * original_transforms_[i]);
    }
}

void SelectTool::commitTransform(const MouseEvent& e) {
    auto& sel = vm_->selection().selection();
    if (sel.empty()) return;

    switch (mode_) {
        case Mode::Move: {
            float dx = e.position.x - drag_start_.x;
            float dy = e.position.y - drag_start_.y;
            if (std::abs(dx) > 0.1f || std::abs(dy) > 0.1f) {
                // Restore original transforms, then execute command
                for (size_t i = 0; i < sel.size(); ++i) {
                    sel[i]->setTransform(original_transforms_[i]);
                }
                vm_->executeCommand(std::make_unique<MoveCommand>(sel, dx, dy));
            }
            break;
        }
        case Mode::Resize: {
            // For single shape with geometry resize, changes are already applied
            // Just mark document as modified
            if (sel.size() == 1) {
                if (auto* shapeNode = dynamic_cast<ShapeNode*>(sel[0].get())) {
                    auto geomType = shapeNode->shape()->geometry_type();
                    if (geomType == flex::GeometryType::Rect ||
                        geomType == flex::GeometryType::Circle ||
                        geomType == flex::GeometryType::Ellipse) {
                        // Geometry was resized directly, nothing more to do
                        break;
                    }
                }
            }
            // For transform-based scaling (multiple selection or other shapes)
            if (!sel.empty() && !original_transforms_.empty()) {
                float scaleX = sel[0]->scaleX() / original_transforms_[0].scaleX();
                float scaleY = sel[0]->scaleY() / original_transforms_[0].scaleY();

                if (std::abs(scaleX - 1.0f) > 0.001f || std::abs(scaleY - 1.0f) > 0.001f) {
                    // Restore and execute
                    for (size_t i = 0; i < sel.size(); ++i) {
                        sel[i]->setTransform(original_transforms_[i]);
                    }
                    vm_->executeCommand(std::make_unique<ScaleCommand>(sel, gizmo_.pivot, scaleX, scaleY));
                }
            }
            break;
        }
        case Mode::Rotate: {
            if (!sel.empty() && !original_transforms_.empty()) {
                float angle = sel[0]->rotation() - original_transforms_[0].rotationDegrees();

                if (std::abs(angle) > 0.1f) {
                    // Restore and execute
                    for (size_t i = 0; i < sel.size(); ++i) {
                        sel[i]->setTransform(original_transforms_[i]);
                    }
                    vm_->executeCommand(std::make_unique<RotateCommand>(sel, gizmo_.pivot, angle));
                }
            }
            break;
        }
        default:
            break;
    }
}

void SelectTool::nudgeSelection(float dx, float dy) {
    auto& sel = vm_->selection().selection();
    if (sel.empty()) return;
    vm_->executeCommand(std::make_unique<MoveCommand>(sel, dx, dy));
    updateGizmo();
}

Rect SelectTool::normalizeRect(const Rect& r) {
    Rect result = r;
    if (result.width < 0) {
        result.x += result.width;
        result.width = -result.width;
    }
    if (result.height < 0) {
        result.y += result.height;
        result.height = -result.height;
    }
    return result;
}

} // namespace editor
