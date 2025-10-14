#include "whiteboard/export_manager.h"

#include <nanogui.h>
#include <nanogui/opengl.h>
#include <vector>
#include <cstring>
#include <memory>
#include "whiteboard/common.h"
#include "../../vendor/nanovg/example/stb_image_write.h"

namespace whiteboard {

bool ExportManager::export_to_png(const std::string &filename, const std::vector<Stroke> &strokes,
                                  NVGcontext *vg, bool /*visible_area_only*/, int width,
                                  int height) {
  try {
    std::unique_ptr<unsigned char[]> pixels(new unsigned char[static_cast<size_t>(width) *
                                                              static_cast<size_t>(height) * 4]);

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    GLuint fbo = 0;
    GLuint rbo = 0;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glDeleteRenderbuffers(1, &rbo);
      glDeleteFramebuffers(1, &fbo);
      return false;
    }

    glViewport(0, 0, width, height);
    glClearColor(0.96f, 0.96f, 0.96f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    nvgBeginFrame(vg, static_cast<float>(width), static_cast<float>(height), 1.0f);
    render_strokes_to_context(vg, strokes, width, height);
    nvgEndFrame(vg);

    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());

    std::unique_ptr<unsigned char[]> flipped(
        new unsigned char[static_cast<size_t>(width) * static_cast<size_t>(height) * 4]);
    for (int y = 0; y < height; ++y) {
      std::memcpy(flipped.get() + static_cast<size_t>(y) * width * 4,
                  pixels.get() + static_cast<size_t>(height - 1 - y) * width * 4,
                  static_cast<size_t>(width) * 4);
    }

    int result = stbi_write_png(filename.c_str(), width, height, 4, flipped.get(), width * 4);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteRenderbuffers(1, &rbo);
    glDeleteFramebuffers(1, &fbo);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

    return result != 0;
  } catch (...) {
    return false;
  }
}

bool ExportManager::export_to_svg(const std::string &filename, const std::vector<Stroke> &strokes,
                                  bool /*visible_area_only*/, int width, int height) {
  try {
    std::ofstream file(filename);
    if (!file.is_open())
      return false;

    file << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?>\n";
    file << "<svg xmlns=\"http://www.w3.org/2000/svg\" "
         << "width=\"" << width << "\" height=\"" << height << "\" "
         << "viewBox=\"0 0 " << width << " " << height << "\">\n";
    file << "  <rect width=\"" << width << "\" height=\"" << height << "\" fill=\"#F5F5F5\"/>\n";

    for (const auto &stroke : strokes) {
      if (!stroke.visible)
        continue;
      convert_stroke_to_svg(file, stroke);
    }

    file << "</svg>\n";
    return true;
  } catch (...) {
    return false;
  }
}

void ExportManager::render_strokes_to_context(NVGcontext *vg, const std::vector<Stroke> &strokes,
                                              int width, int height) {
  nvgBeginPath(vg);
  nvgRect(vg, 0, 0, static_cast<float>(width), static_cast<float>(height));
  nvgFillColor(vg, nvgRGBA(245, 245, 245, 255));
  nvgFill(vg);

  for (const auto &stroke : strokes) {
    if (!stroke.visible)
      continue;
    draw_stroke_to_context(vg, stroke);
  }
}

void ExportManager::draw_stroke_to_context(NVGcontext *vg, const Stroke &stroke) {
  if (stroke.points.empty())
    return;

  nvgSave(vg);

  if (std::abs(stroke.rotation) > 0.001f) {
    float min_x = 0.f, min_y = 0.f, max_x = 0.f, max_y = 0.f;
    stroke.get_bounds(min_x, min_y, max_x, max_y);
    float center_x = (min_x + max_x) / 2.0f;
    float center_y = (min_y + max_y) / 2.0f;
    nvgTranslate(vg, center_x, center_y);
    nvgRotate(vg, stroke.rotation);
    nvgTranslate(vg, -center_x, -center_y);
  }

  nvgStrokeColor(vg, nvgRGBAf(stroke.color.r(), stroke.color.g(), stroke.color.b(),
                              stroke.color.w()));
  nvgStrokeWidth(vg, stroke.width);

  switch (stroke.tool) {
  case Tool::Pen: {
    nvgBeginPath(vg);
    nvgMoveTo(vg, stroke.points[0].x, stroke.points[0].y);
    for (size_t i = 1; i < stroke.points.size(); ++i) {
      nvgLineTo(vg, stroke.points[i].x, stroke.points[i].y);
    }
    nvgStroke(vg);
    break;
  }
  case Tool::Rectangle: {
    if (stroke.points.size() < 2)
      break;
    float x = std::min(stroke.points[0].x, stroke.points[1].x);
    float y = std::min(stroke.points[0].y, stroke.points[1].y);
    float w = std::abs(stroke.points[1].x - stroke.points[0].x);
    float h = std::abs(stroke.points[1].y - stroke.points[0].y);

    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    if (stroke.fill_style == FillStyle::Solid) {
      nvgFillColor(vg, nvgRGBAf(stroke.fill_color.r(), stroke.fill_color.g(),
                                stroke.fill_color.b(), stroke.fill_color.w()));
      nvgFill(vg);
    }
    nvgStroke(vg);
    break;
  }
  case Tool::Circle: {
    if (stroke.points.size() < 2)
      break;
    float cx = (stroke.points[0].x + stroke.points[1].x) / 2.0f;
    float cy = (stroke.points[0].y + stroke.points[1].y) / 2.0f;
    float rx = std::abs(stroke.points[1].x - stroke.points[0].x) / 2.0f;
    float ry = std::abs(stroke.points[1].y - stroke.points[0].y) / 2.0f;
    float r = std::max(rx, ry);

    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, r);
    if (stroke.fill_style == FillStyle::Solid) {
      nvgFillColor(vg, nvgRGBAf(stroke.fill_color.r(), stroke.fill_color.g(),
                                stroke.fill_color.b(), stroke.fill_color.w()));
      nvgFill(vg);
    }
    nvgStroke(vg);
    break;
  }
  case Tool::Line:
  case Tool::Arrow: {
    if (stroke.points.size() < 2)
      break;
    nvgBeginPath(vg);
    nvgMoveTo(vg, stroke.points[0].x, stroke.points[0].y);
    nvgLineTo(vg, stroke.points[1].x, stroke.points[1].y);
    nvgStroke(vg);

    if (stroke.tool == Tool::Arrow) {
      float dx = stroke.points[1].x - stroke.points[0].x;
      float dy = stroke.points[1].y - stroke.points[0].y;
      float angle = std::atan2(dy, dx);
      float arrow_size = 15.0f;
      float x1 = stroke.points[1].x - arrow_size * std::cos(angle - static_cast<float>(M_PI) / 6);
      float y1 = stroke.points[1].y - arrow_size * std::sin(angle - static_cast<float>(M_PI) / 6);
      float x2 = stroke.points[1].x - arrow_size * std::cos(angle + static_cast<float>(M_PI) / 6);
      float y2 = stroke.points[1].y - arrow_size * std::sin(angle + static_cast<float>(M_PI) / 6);

      nvgBeginPath(vg);
      nvgMoveTo(vg, x1, y1);
      nvgLineTo(vg, stroke.points[1].x, stroke.points[1].y);
      nvgLineTo(vg, x2, y2);
      nvgStroke(vg);
    }
    break;
  }
  case Tool::Text: {
    if (stroke.points.empty() || stroke.text.empty())
      break;
    nvgBeginPath(vg);
    nvgFontFace(vg, stroke.font_face.c_str());
    nvgFontSize(vg, stroke.font_size);
    nvgFillColor(vg, nvgRGBAf(stroke.color.r(), stroke.color.g(), stroke.color.b(),
                              stroke.color.w()));
    nvgTextAlign(vg, stroke.text_align);
    nvgText(vg, stroke.points[0].x, stroke.points[0].y, stroke.text.c_str(), nullptr);
    break;
  }
  case Tool::Sticky: {
    if (stroke.points.size() < 2)
      break;
    float x = std::min(stroke.points[0].x, stroke.points[1].x);
    float y = std::min(stroke.points[0].y, stroke.points[1].y);
    float w = std::abs(stroke.points[1].x - stroke.points[0].x);
    float h = std::abs(stroke.points[1].y - stroke.points[0].y);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, 5.0f);
    nvgFillColor(vg, nvgRGBAf(stroke.fill_color.r(), stroke.fill_color.g(), stroke.fill_color.b(),
                              stroke.fill_color.w()));
    nvgFill(vg);
    nvgStroke(vg);

    if (!stroke.text.empty()) {
      nvgFontFace(vg, "sans");
      nvgFontSize(vg, 14.0f);
      nvgFillColor(vg, nvgRGBA(50, 50, 50, 255));
      nvgText(vg, x + 10.0f, y + 24.0f, stroke.text.c_str(), nullptr);
    }
    break;
  }
  case Tool::Image: {
    if (stroke.points.size() < 2 || stroke.nvg_image_handle < 0)
      break;
    float x = std::min(stroke.points[0].x, stroke.points[1].x);
    float y = std::min(stroke.points[0].y, stroke.points[1].y);
    float w = std::abs(stroke.points[1].x - stroke.points[0].x);
    float h = std::abs(stroke.points[1].y - stroke.points[0].y);

    NVGpaint img = nvgImagePattern(vg, x, y, w, h, 0.0f, stroke.nvg_image_handle, 1.0f);
    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    nvgFillPaint(vg, img);
    nvgFill(vg);
    break;
  }
  default:
    break;
  }

  nvgRestore(vg);
}

void ExportManager::convert_stroke_to_svg(std::ofstream &file, const Stroke &stroke) {
  if (stroke.points.empty())
    return;

  auto color_to_hex = [](const nanogui::Color &c) {
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "#%02X%02X%02X", static_cast<int>(c.r() * 255.0f),
                  static_cast<int>(c.g() * 255.0f), static_cast<int>(c.b() * 255.0f));
    return std::string(buffer);
  };

  auto color_with_alpha = [](const nanogui::Color &c) {
    char buffer[16];
    std::snprintf(buffer, sizeof(buffer), "rgba(%d,%d,%d,%f)", static_cast<int>(c.r() * 255.0f),
                  static_cast<int>(c.g() * 255.0f), static_cast<int>(c.b() * 255.0f), c.w());
    return std::string(buffer);
  };

  const std::string stroke_color = color_to_hex(stroke.color);
  const std::string fill_color = color_with_alpha(stroke.fill_color);
  const std::string opacity = std::to_string(stroke.color.w());

  auto rotation_transform = [&]() -> std::string {
    if (std::abs(stroke.rotation) < 0.001f)
      return {};
    float min_x = 0.f, min_y = 0.f, max_x = 0.f, max_y = 0.f;
    stroke.get_bounds(min_x, min_y, max_x, max_y);
    float cx = (min_x + max_x) / 2.0f;
    float cy = (min_y + max_y) / 2.0f;
    return " transform=\"rotate(" + std::to_string(stroke.rotation * 180.0f / static_cast<float>(M_PI)) +
           " " + std::to_string(cx) + " " + std::to_string(cy) + ")\"";
  };

  switch (stroke.tool) {
  case Tool::Pen: {
    file << "  <polyline points=\"";
    for (const auto &pt : stroke.points) {
      file << pt.x << "," << pt.y << " ";
    }
    file << "\" stroke=\"" << stroke_color << "\" stroke-width=\"" << stroke.width
         << "\" fill=\"none\" opacity=\"" << opacity << "\"" << rotation_transform() << "/>\n";
    break;
  }
  case Tool::Rectangle: {
    if (stroke.points.size() < 2)
      break;
    float x = std::min(stroke.points[0].x, stroke.points[1].x);
    float y = std::min(stroke.points[0].y, stroke.points[1].y);
    float w = std::abs(stroke.points[1].x - stroke.points[0].x);
    float h = std::abs(stroke.points[1].y - stroke.points[0].y);
    file << "  <rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << h
         << "\" stroke=\"" << stroke_color << "\" stroke-width=\"" << stroke.width << "\" ";
    if (stroke.fill_style == FillStyle::Solid) {
      file << "fill=\"" << fill_color << "\" ";
    } else {
      file << "fill=\"none\" ";
    }
    file << "opacity=\"" << opacity << "\"" << rotation_transform() << "/>\n";
    break;
  }
  case Tool::Circle: {
    if (stroke.points.size() < 2)
      break;
    float cx = (stroke.points[0].x + stroke.points[1].x) / 2.0f;
    float cy = (stroke.points[0].y + stroke.points[1].y) / 2.0f;
    float rx = std::abs(stroke.points[1].x - stroke.points[0].x) / 2.0f;
    float ry = std::abs(stroke.points[1].y - stroke.points[0].y) / 2.0f;
    float r = std::max(rx, ry);
    file << "  <circle cx=\"" << cx << "\" cy=\"" << cy << "\" r=\"" << r << "\" stroke=\""
         << stroke_color << "\" stroke-width=\"" << stroke.width << "\" ";
    if (stroke.fill_style == FillStyle::Solid) {
      file << "fill=\"" << fill_color << "\" ";
    } else {
      file << "fill=\"none\" ";
    }
    file << "opacity=\"" << opacity << "\"" << rotation_transform() << "/>\n";
    break;
  }
  case Tool::Line:
  case Tool::Arrow: {
    if (stroke.points.size() < 2)
      break;
    file << "  <line x1=\"" << stroke.points[0].x << "\" y1=\"" << stroke.points[0].y << "\" "
         << "x2=\"" << stroke.points[1].x << "\" y2=\"" << stroke.points[1].y << "\" stroke=\""
         << stroke_color << "\" stroke-width=\"" << stroke.width << "\" opacity=\"" << opacity
         << "\"" << rotation_transform() << "/>\n";
    if (stroke.tool == Tool::Arrow) {
      float dx = stroke.points[1].x - stroke.points[0].x;
      float dy = stroke.points[1].y - stroke.points[0].y;
      float angle = std::atan2(dy, dx);
      float arrow_size = 15.0f;
      float x1 = stroke.points[1].x - arrow_size * std::cos(angle - static_cast<float>(M_PI) / 6);
      float y1 = stroke.points[1].y - arrow_size * std::sin(angle - static_cast<float>(M_PI) / 6);
      float x2 = stroke.points[1].x - arrow_size * std::cos(angle + static_cast<float>(M_PI) / 6);
      float y2 = stroke.points[1].y - arrow_size * std::sin(angle + static_cast<float>(M_PI) / 6);
      file << "  <path d=\"M " << x1 << " " << y1 << " L " << stroke.points[1].x << " "
           << stroke.points[1].y << " L " << x2 << " " << y2 << "\" stroke=\"" << stroke_color
           << "\" stroke-width=\"" << stroke.width << "\" fill=\"none\" opacity=\"" << opacity
           << "\"" << rotation_transform() << "/>\n";
    }
    break;
  }
  case Tool::Text: {
    if (stroke.points.empty() || stroke.text.empty())
      break;
    file << "  <text x=\"" << stroke.points[0].x << "\" y=\"" << stroke.points[0].y
         << "\" font-family=\"" << stroke.font_face << "\" font-size=\"" << stroke.font_size
         << "\" fill=\"" << stroke_color << "\" opacity=\"" << opacity << "\""
         << rotation_transform() << ">" << stroke.text << "</text>\n";
    break;
  }
  case Tool::Sticky: {
    if (stroke.points.size() < 2)
      break;
    float x = std::min(stroke.points[0].x, stroke.points[1].x);
    float y = std::min(stroke.points[0].y, stroke.points[1].y);
    float w = std::abs(stroke.points[1].x - stroke.points[0].x);
    float h = std::abs(stroke.points[1].y - stroke.points[0].y);
    file << "  <rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << h
         << "\" rx=\"5\" ry=\"5\" stroke=\"" << stroke_color << "\" stroke-width=\"" << stroke.width
         << "\" fill=\"" << fill_color << "\" opacity=\"" << opacity << "\"" << rotation_transform()
         << "/>\n";
    if (!stroke.text.empty()) {
      file << "  <text x=\"" << (x + 10) << "\" y=\"" << (y + 24)
           << "\" font-family=\"sans\" font-size=\"14\" fill=\"#323232\">" << stroke.text
           << "</text>\n";
    }
    break;
  }
  default:
    break;
  }
}

} // namespace whiteboard
