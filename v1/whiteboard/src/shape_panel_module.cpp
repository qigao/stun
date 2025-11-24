#include "whiteboard/shape_panel_module.h"
#include "whiteboard/svg/svg_shape_library.h"
#include "whiteboard/svg/svg_renderer.h"
#include <fmtlog.h>
#include <cstdio>
#include <fstream>
#include <map>

ShapePanelModule::ShapePanelModule(Widget *parent, whiteboard::SVGShapeLibrary *library)
    : Widget(parent), m_library(library), m_dragging(false),
      m_scroll_offset(0.f), m_max_scroll(0.f),
      m_svg_renderer(std::make_unique<whiteboard::SVGRenderer>()) {

  logi("ShapePanelModule: Initializing shape panel");
  if (!library) {
    loge("ShapePanelModule: Library is null!");
  } else if (!library->is_loaded()) {
    logw("ShapePanelModule: Library is not loaded yet");
  }
  
  rebuild_accordion();
  logi("ShapePanelModule: Initialized with {} categories, {} total shapes", 
       m_categories.size(), m_all_shapes.size());
}

Vector2i ShapePanelModule::preferred_size_impl(NVGcontext *) const {
  return {400, 600}; // Wider panel for shape grid
}

void ShapePanelModule::rebuild_accordion() {
  logd("ShapePanelModule: Rebuilding accordion");
  
  // Save current expansion states
  std::map<std::string, bool> expansionStates;
  for (const auto &section : m_categories) {
    expansionStates[section.name] = section.expanded;
  }

  m_categories.clear();
  m_all_shapes.clear();

  if (!m_library || !m_library->is_loaded()) {
    logw("ShapePanelModule: Cannot rebuild - library not loaded");
    return;
  }

  const float headerHeight = 40.f;
  const float buttonSize = 70.f;
  const float spacing = 10.f;
  const float padding = 15.f;
  const int columns = 4;

  float currentY = 60.f; // Start below title

  // Get all categories
  auto categories = m_library->get_categories();
  logi("ShapePanelModule: Found {} categories", categories.size());
  
  if (categories.empty()) {
    loge("ShapePanelModule: No categories found in library!");
    return;
  }

  // Build accordion sections
  for (const auto &category : categories) {
    CategorySection section;
    section.name = category;
    section.y = currentY;
    
    // Restore previous expansion state, or default to "Basic" expanded
    if (expansionStates.find(category) != expansionStates.end()) {
      section.expanded = expansionStates[category];
    } else {
      section.expanded = (category == "Basic");
    }

    // Get shapes for this category
    auto shapes = m_library->get_shapes(category);
    logd("ShapePanelModule: Category '{}' has {} shapes, expanded={}", 
         category, shapes.size(), section.expanded);
    
    // Add shapes to global list and track indices
    for (const auto &shape : shapes) {
      section.shape_indices.push_back(static_cast<int>(m_all_shapes.size()));
      m_all_shapes.push_back({0.f, 0.f, buttonSize, shape.id, shape.name, category, false});
    }

    // Calculate section height
    if (section.expanded && !shapes.empty()) {
      int rows = (static_cast<int>(shapes.size()) + columns - 1) / columns;
      section.height = headerHeight + rows * (buttonSize + spacing) + padding;
    } else {
      section.height = headerHeight;
    }

    m_categories.push_back(section);
    currentY += section.height;
  }

  // Calculate max scroll
  m_max_scroll = std::max(0.f, currentY - 600.f + 20.f);
}

bool ShapePanelModule::mouse_button_event(const Vector2i &p, int button, bool down,
                                           int modifiers) {
  if (button == GLFW_MOUSE_BUTTON_1 && down) {
    logd("ShapePanelModule: Mouse click at ({}, {})", p.x(), p.y());
    
    Vector2f localPos = Vector2f(static_cast<float>(p.x() - m_pos.x()), 
                                  static_cast<float>(p.y() - m_pos.y()));
    
    logd("ShapePanelModule: Local pos ({}, {}), panel has {} categories", 
         localPos.x(), localPos.y(), m_categories.size());

    const float padding = 15.f;
    const float buttonSize = 70.f;
    const float spacing = 10.f;
    const int columns = 4;
    const float headerHeight = 40.f;
    const float titleAreaHeight = 60.f;
    const float panelWidth = static_cast<float>(m_size.x());

    // Skip if click is in title area
    if (localPos.y() < titleAreaHeight) {
      m_dragging = true;
      m_dragStart = p;
      return true;
    }

    // Check category headers and shapes (with scroll offset)
    for (size_t sectionIdx = 0; sectionIdx < m_categories.size(); ++sectionIdx) {
      const auto &section = m_categories[sectionIdx];
      float adjustedY = section.y - m_scroll_offset;
      float headerTop = adjustedY;
      float headerBottom = adjustedY + headerHeight;
      
      // Check if click is within header bounds
      if (localPos.x() >= 10.f && localPos.x() <= panelWidth - 10.f &&
          localPos.y() >= headerTop && localPos.y() <= headerBottom) {
        // Toggle expansion - use index to avoid reference invalidation
        bool newState = !m_categories[sectionIdx].expanded;
        logi("ShapePanelModule: Toggling category '{}' to {}", 
             section.name, newState ? "expanded" : "collapsed");
        m_categories[sectionIdx].expanded = newState;
        rebuild_accordion();
        screen()->redraw();
        return true;
      }

      // Check shapes in expanded sections
      if (section.expanded) {
        float shapeStartY = adjustedY + headerHeight;
        for (size_t i = 0; i < section.shape_indices.size(); ++i) {
          int shapeIdx = section.shape_indices[i];
          
          // Bounds check to prevent heap corruption
          if (shapeIdx < 0 || shapeIdx >= static_cast<int>(m_all_shapes.size())) {
            continue;
          }
          
          int row = static_cast<int>(i) / columns;
          int col = static_cast<int>(i) % columns;
          float x = padding + col * (buttonSize + spacing);
          float y = shapeStartY + row * (buttonSize + spacing);

          if (localPos.x() >= x && localPos.x() <= x + buttonSize &&
              localPos.y() >= y && localPos.y() <= y + buttonSize) {
            // Shape selected - copy the ID before any potential reallocation
            std::string selectedShapeId = m_all_shapes[shapeIdx].id;
            std::string selectedShapeName = m_all_shapes[shapeIdx].name;
            logi("ShapePanelModule: Shape selected - ID: '{}', Name: '{}'", 
                 selectedShapeId, selectedShapeName);
            
            if (m_shape_callback) {
              logi("ShapePanelModule: Calling shape callback");
              
              try {
                m_shape_callback(selectedShapeId);
                logi("ShapePanelModule: Callback completed successfully");
              } catch (const std::exception &e) {
                loge("ShapePanelModule: Exception in callback: {}", e.what());
              } catch (...) {
                loge("ShapePanelModule: Unknown exception in callback!");
              }
            } else {
              logw("ShapePanelModule: No shape callback set!");
            }
            screen()->redraw();
            return true;
          }
        }
      }
    }

    // Start dragging panel
    m_dragging = true;
    m_dragStart = p;
    return true;
  } else if (button == GLFW_MOUSE_BUTTON_1 && !down) {
    m_dragging = false;
  }
  return Widget::mouse_button_event(p, button, down, modifiers);
}

bool ShapePanelModule::mouse_drag_event(const Vector2i &p, const Vector2i &rel,
                                         int button, int modifiers) {
  if (m_dragging) {
    Vector2i newPos = m_pos + rel;
    set_position(newPos);
    return true;
  }
  return Widget::mouse_drag_event(p, rel, button, modifiers);
}

bool ShapePanelModule::scroll_event(const Vector2i &p, const Vector2f &rel) {
  // Check if mouse is over the panel
  Vector2f localPos = Vector2f(static_cast<float>(p.x() - m_pos.x()), 
                                static_cast<float>(p.y() - m_pos.y()));
  
  if (localPos.x() >= 0 && localPos.x() <= m_size.x() &&
      localPos.y() >= 0 && localPos.y() <= m_size.y()) {
    
    // Scroll the panel
    float scroll_amount = rel.y() * 20.f; // Adjust scroll speed
    m_scroll_offset -= scroll_amount;
    m_scroll_offset = std::max(0.f, std::min(m_scroll_offset, m_max_scroll));
    
    logd("ShapePanelModule: Scrolled to offset {}", m_scroll_offset);
    screen()->redraw();
    return true;
  }
  
  return Widget::scroll_event(p, rel);
}

void ShapePanelModule::draw(NVGcontext *ctx) {
  static bool first_draw = true;
  if (first_draw) {
    logi("ShapePanelModule: First draw - Panel visible, {} categories, {} shapes", 
         m_categories.size(), m_all_shapes.size());
    first_draw = false;
  }
  
  Widget::draw(ctx);

  const float px = static_cast<float>(m_pos.x());
  const float py = static_cast<float>(m_pos.y());
  const float pw = static_cast<float>(m_size.x());
  const float ph = static_cast<float>(m_size.y());

  // Draw rounded panel background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, px, py, pw, ph, 8.f);
  nvgFillColor(ctx, nvgRGBA(248, 248, 252, 255));
  nvgFill(ctx);
  
  // Draw border
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, px, py, pw, ph, 8.f);
  nvgStrokeColor(ctx, nvgRGBA(200, 200, 210, 255));
  nvgStrokeWidth(ctx, 1.5f);
  nvgStroke(ctx);

  // Title
  drawLabel(ctx, px + 20.f, py + 20.f, "形状库");

  // Clip region for scrollable content
  nvgSave(ctx);
  nvgScissor(ctx, px, py + 60.f, pw, ph - 60.f);

  const float padding = 15.f;
  const float buttonSize = 70.f;
  const float spacing = 10.f;
  const int columns = 4;

  // Draw accordion sections
  for (const auto &section : m_categories) {
    drawCategoryHeader(ctx, section, px, py);

    // Draw shapes if expanded
    if (section.expanded) {
      float shapeStartY = section.y - m_scroll_offset + 40.f;
      for (size_t i = 0; i < section.shape_indices.size(); ++i) {
        int shapeIdx = section.shape_indices[i];
        int row = static_cast<int>(i) / columns;
        int col = static_cast<int>(i) % columns;
        
        ShapeButton btn = m_all_shapes[shapeIdx];
        btn.x = padding + col * (buttonSize + spacing);
        btn.y = shapeStartY + row * (buttonSize + spacing);
        
        drawShapeButton(ctx, btn, px, py);
      }
    }
  }

  nvgRestore(ctx);

  // Draw scroll indicator if needed
  if (m_max_scroll > 0.f) {
    float scrollBarHeight = 100.f;
    float scrollBarY = py + 70.f + (m_scroll_offset / m_max_scroll) * (ph - 80.f - scrollBarHeight);
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, px + pw - 10.f, scrollBarY, 6.f, scrollBarHeight, 3.f);
    nvgFillColor(ctx, nvgRGBA(180, 180, 200, 150));
    nvgFill(ctx);
  }
}

void ShapePanelModule::drawLabel(NVGcontext *ctx, float x, float y, const char *text) {
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 14.f);
  nvgFillColor(ctx, nvgRGBA(80, 80, 90, 255));
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
  nvgText(ctx, x, y, text, nullptr);
}

void ShapePanelModule::drawCategoryHeader(NVGcontext *ctx, const CategorySection &section, float px, float py) {
  const float headerHeight = 40.f;
  float adjustedY = py + section.y - m_scroll_offset;

  // Header background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, px + 10.f, adjustedY, static_cast<float>(m_size.x()) - 20.f, headerHeight, 8.f);
  nvgFillColor(ctx, nvgRGBA(240, 240, 245, 255));
  nvgFill(ctx);

  // Draw expand/collapse icon
  drawExpandIcon(ctx, px + 20.f, adjustedY + headerHeight * 0.5f, 12.f, section.expanded);

  // Category name
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 14.f);
  nvgFillColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgTextAlign(ctx, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  nvgText(ctx, px + 40.f, adjustedY + headerHeight * 0.5f, section.name.c_str(), nullptr);

  // Shape count
  char countText[32];
  snprintf(countText, sizeof(countText), "(%zu)", section.shape_indices.size());
  nvgFontSize(ctx, 12.f);
  nvgFillColor(ctx, nvgRGBA(120, 120, 140, 255));
  nvgTextAlign(ctx, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
  nvgText(ctx, px + static_cast<float>(m_size.x()) - 20.f, adjustedY + headerHeight * 0.5f, countText, nullptr);
}

void ShapePanelModule::drawExpandIcon(NVGcontext *ctx, float x, float y, float size, bool expanded) {
  nvgStrokeColor(ctx, nvgRGBA(60, 60, 80, 255));
  nvgStrokeWidth(ctx, 2.f);
  nvgLineCap(ctx, NVG_ROUND);

  if (expanded) {
    // Down arrow (expanded)
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, x - size * 0.4f, y - size * 0.2f);
    nvgLineTo(ctx, x, y + size * 0.3f);
    nvgLineTo(ctx, x + size * 0.4f, y - size * 0.2f);
    nvgStroke(ctx);
  } else {
    // Right arrow (collapsed)
    nvgBeginPath(ctx);
    nvgMoveTo(ctx, x - size * 0.2f, y - size * 0.4f);
    nvgLineTo(ctx, x + size * 0.3f, y);
    nvgLineTo(ctx, x - size * 0.2f, y + size * 0.4f);
    nvgStroke(ctx);
  }
}

void ShapePanelModule::drawShapeButton(NVGcontext *ctx, const ShapeButton &btn, float px, float py) {

  // Button background
  nvgBeginPath(ctx);
  nvgRoundedRect(ctx, px + btn.x, py + btn.y, btn.size, btn.size, 8.f);

  if (btn.hovered) {
    nvgFillColor(ctx, nvgRGBA(220, 220, 245, 255));
  } else {
    nvgFillColor(ctx, nvgRGBA(240, 240, 245, 255));
  }
  nvgFill(ctx);

  // Draw border for hovered
  if (btn.hovered) {
    nvgBeginPath(ctx);
    nvgRoundedRect(ctx, px + btn.x, py + btn.y, btn.size, btn.size, 8.f);
    nvgStrokeColor(ctx, nvgRGBA(180, 180, 220, 255));
    nvgStrokeWidth(ctx, 2.f);
    nvgStroke(ctx);
  }

  // Draw SVG icon in the center
  if (m_library && m_svg_renderer) {
    auto shape_info = m_library->get_shape(btn.id);
    if (shape_info && !shape_info->svg_file.empty()) {
      // Prepend shapes/ directory to the path
      std::string svg_path = "shapes/" + shape_info->svg_file;
      
      // Read the SVG file
      std::ifstream file(svg_path);
      if (file.is_open()) {
        std::string svg_data((std::istreambuf_iterator<char>(file)),
                             std::istreambuf_iterator<char>());
        file.close();
        
        // Load and render the SVG
        auto svg_doc = m_svg_renderer->load_svg(svg_data);
        if (svg_doc) {
          // Calculate icon size and position (centered in button with padding)
          float iconSize = btn.size * 0.6f;  // 60% of button size
          float centerX = px + btn.x + (btn.size - iconSize) * 0.5f;
          float centerY = py + btn.y + (btn.size - iconSize) * 0.5f;
          
          // Render the SVG icon
          m_svg_renderer->render(ctx, svg_doc.get(), 
                                nanogui::Vector2f(centerX, centerY),
                                iconSize / svg_doc->width(),  // scale to fit
                                iconSize / svg_doc->height(),
                                0.0f);  // no rotation
        }
      }
    }
  }

  // Draw label below icon
  float centerX = px + btn.x + btn.size * 0.5f;
  nvgFontFace(ctx, "sans");
  nvgFontSize(ctx, 9.f);
  nvgFillColor(ctx, nvgRGBA(80, 80, 100, 255));
  nvgTextAlign(ctx, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
  nvgText(ctx, centerX, py + btn.y + btn.size - 6.f, btn.name.c_str(), nullptr);
}




