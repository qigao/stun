/*
 * NanoVG CSS - SVG XML Parser
 *
 * Parses SVG XML documents and creates nanovg_css elements.
 * Uses pugixml for XML parsing.
 */

#ifndef NANOVG_CSS_SVG_XML_H
#define NANOVG_CSS_SVG_XML_H

#include "nanovg_css.h"
#include <string>
#include <functional>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Load SVG from XML string
 *
 * Parses SVG XML and creates corresponding nanovg_css elements.
 * All elements are created as children of the specified parent.
 *
 * @param renderer CSS renderer
 * @param svg_xml SVG XML string
 * @param parent Parent element (NULL for root)
 * @return Root SVG element, or NULL on parse error
 *
 * @example
 *   const char* svg = R"(
 *     <svg width="100" height="100">
 *       <circle cx="50" cy="50" r="40" fill="red"/>
 *     </svg>
 *   )";
 *   NVGCSSElement* root = nvgcssLoadSVG(renderer, svg, NULL);
 */
NVGCSSElement* nvgcssLoadSVG(NVGCSSRenderer* renderer,
                              const char* svg_xml,
                              NVGCSSElement* parent);

/**
 * @brief Load SVG from file
 *
 * @param renderer CSS renderer
 * @param filepath Path to SVG file
 * @param parent Parent element (NULL for root)
 * @return Root SVG element, or NULL on error
 */
NVGCSSElement* nvgcssLoadSVGFile(NVGCSSRenderer* renderer,
                                  const char* filepath,
                                  NVGCSSElement* parent);

/**
 * @brief Parse SVG and return CSS stylesheet
 *
 * Extracts <style> elements from SVG and returns combined CSS.
 * Useful when you want to apply SVG's embedded styles.
 *
 * @param svg_xml SVG XML string
 * @return CSS string (caller must free)
 */
char* nvgcssExtractSVGStyles(const char* svg_xml);

#ifdef __cplusplus
}

namespace nvgcss {

/**
 * @brief SVG XML Parser (C++ interface)
 *
 * More flexible C++ API for SVG parsing with callbacks.
 */
class SVGXMLParser {
public:
    struct Options {
        bool preserve_ids = true;         // Keep SVG element IDs
        bool parse_styles = true;         // Parse <style> elements
        bool parse_defs = true;           // Parse <defs> (gradients, patterns)
        std::string id_prefix;            // Prefix for generated IDs
        float scale = 1.0f;               // Scale factor
    };

    /**
     * @brief Parse SVG XML string
     *
     * @param renderer CSS renderer
     * @param svg_xml SVG XML string
     * @param options Parse options
     * @return Root element or nullptr
     */
    static NVGCSSElement* parse(NVGCSSRenderer* renderer,
                                 const std::string& svg_xml,
                                 const Options& options = {});

    /**
     * @brief Parse SVG file
     */
    static NVGCSSElement* parseFile(NVGCSSRenderer* renderer,
                                     const std::string& filepath,
                                     const Options& options = {});

    /**
     * @brief Resolve gradient references and build cache (performance optimization)
     * 
     * Called once after SVG parsing to cache gradient pointers.
     * Eliminates repeated map lookups during rendering.
     * 
     * @param renderer CSS renderer with gradient registry
     * @param element Root element to process (recursively processes children)
     */
    static void resolve_gradient_references(NVGCSSRenderer* renderer, NVGCSSElement* element);
};

} // namespace nvgcss

#endif // __cplusplus

#endif // NANOVG_CSS_SVG_XML_H
