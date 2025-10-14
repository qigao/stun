/**
 * \file export_manager.h
 * \brief Utilities for exporting whiteboard strokes to PNG or SVG formats.
 */

#pragma once

#include "types.h"

#include <nanovg.h>

#include <fstream>
#include <string>
#include <vector>

namespace whiteboard {

/**
 * \class ExportManager
 * \brief Static helpers that serialize canvas strokes into raster or vector files.
 *
 * The manager renders strokes through NanoVG for PNG capture and emits
 * simplified path data for SVG export, supporting both whole-canvas and
 * visible-region captures.
 */
class ExportManager {
public:
  static bool export_to_png(const std::string &filename, const std::vector<Stroke> &strokes,
                            NVGcontext *vg, bool visible_area_only, int width, int height);

  static bool export_to_svg(const std::string &filename, const std::vector<Stroke> &strokes,
                            bool visible_area_only, int width, int height);

private:
  static void render_strokes_to_context(NVGcontext *vg, const std::vector<Stroke> &strokes,
                                        int width, int height);
  static void draw_stroke_to_context(NVGcontext *vg, const Stroke &stroke);
  static void convert_stroke_to_svg(std::ofstream &file, const Stroke &stroke);
};

} // namespace whiteboard
