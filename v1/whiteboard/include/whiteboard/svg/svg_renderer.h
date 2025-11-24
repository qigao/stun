/**
 * \file svg_renderer.h
 * \brief SVG rendering service using LunaSVG library.
 */

#pragma once

#include <nanogui/vector.h>
#include <nanovg.h>
#include <lunasvg.h>
#include <memory>
#include <string>
#include <vector>

namespace whiteboard {

// Forward declaration
struct SVGShapeDesc;

/**
 * \class SVGRenderer
 * \brief Renders SVG shapes using LunaSVG and integrates with NanoVG.
 *
 * This class provides SVG loading and rendering capabilities.
 * It uses LunaSVG to parse and rasterize SVG content, then renders the
 * result to a NanoVG context. LunaSVG provides clean RAII semantics
 * and avoids the memory management issues of ThorVG.
 * 
 * Also supports rendering from DDF-like shape descriptions for easier
 * programmatic SVG generation.
 */
class SVGRenderer {
public:
  SVGRenderer();
  ~SVGRenderer();

  /**
   * \brief Load SVG from string data and create LunaSVG document.
   * \param svg_data SVG content as string
   * \return Unique pointer to LunaSVG document, or nullptr on error
   *
   * Creates a new LunaSVG document from the provided SVG data.
   * The document can be rendered multiple times with different parameters.
   */
  std::unique_ptr<lunasvg::Document> load_svg(const std::string& svg_data);

  /**
   * \brief Render LunaSVG document to NanoVG context.
   * \param ctx NanoVG context
   * \param document LunaSVG document to render
   * \param pos Position in canvas coordinates
   * \param scale_x Horizontal scale factor
   * \param scale_y Vertical scale factor
   * \param rotation Rotation angle in radians
   * \param tint_color Optional color tint to apply (nullptr for no tint)
   *
   * Renders the LunaSVG document by rasterizing it to a bitmap,
   * creating a NanoVG image, and drawing it with transforms applied.
   * If tint_color is provided, applies color modulation to the SVG.
   * Memory is automatically managed via RAII.
   */
  void render(NVGcontext* ctx, lunasvg::Document* document,
              const nanogui::Vector2f& pos, float scale_x, float scale_y, float rotation,
              const NVGcolor* tint_color = nullptr);

  /**
   * \brief Render from DDF-like shape description.
   * \param ctx NanoVG context
   * \param shape Shape description (DDF-like format)
   * \param pos Position in canvas coordinates
   * \param scale_x Horizontal scale factor
   * \param scale_y Vertical scale factor
   * \param rotation Rotation angle in radians
   * \param tint_color Optional color tint to apply (nullptr for no tint)
   * 
   * Generates SVG from the shape description and renders it.
   * This allows creating SVG shapes programmatically without writing XML.
   */
  void render_shape(NVGcontext* ctx, const SVGShapeDesc& shape,
                   const nanogui::Vector2f& pos, float scale_x, float scale_y, float rotation,
                   const NVGcolor* tint_color = nullptr);

  /**
   * \brief Render multiple shapes from DDF-like descriptions.
   * \param ctx NanoVG context
   * \param shapes Vector of shape descriptions
   * \param pos Position in canvas coordinates
   * \param scale_x Horizontal scale factor
   * \param scale_y Vertical scale factor
   * \param rotation Rotation angle in radians
   * \param tint_color Optional color tint to apply (nullptr for no tint)
   * 
   * Generates SVG from multiple shape descriptions and renders them as one document.
   */
  void render_shapes(NVGcontext* ctx, const std::vector<SVGShapeDesc>& shapes,
                    const nanogui::Vector2f& pos, float scale_x, float scale_y, float rotation,
                    const NVGcolor* tint_color = nullptr);

  /**
   * \brief Deprecated - no longer needed with LunaSVG.
   */
  void cache_picture(int stroke_id, std::unique_ptr<lunasvg::Document> document) {
    (void)stroke_id;
    (void)document;
  }

  /**
   * \brief Deprecated - no longer needed with LunaSVG.
   */
  lunasvg::Document* get_cached_picture(int stroke_id) {
    (void)stroke_id;
    return nullptr;
  }

  /**
   * \brief Deprecated - no longer needed with LunaSVG.
   */
  void clear_cache() {}

  /**
   * \brief Deprecated - no longer needed with LunaSVG.
   */
  void remove_from_cache(int stroke_id) {
    (void)stroke_id;
  }
};

} // namespace whiteboard
