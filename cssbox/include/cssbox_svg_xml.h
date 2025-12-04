/*
 * NanoVG CSS - SVG XML Parser
 *
 * Parses SVG XML documents and creates cssbox elements.
 * Uses pugixml for XML parsing.
 */

#ifndef cssbox_SVG_XML_H
#define cssbox_SVG_XML_H

#include "cssbox.h"
#include <string>
#include <functional>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Load SVG from XML string
 *
 * Parses SVG XML and creates corresponding cssbox elements.
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
 *   cssboxElement* root = cssboxLoadSVG(renderer, svg, NULL);
 */
cssboxElement* cssboxLoadSVG(cssboxRenderer* renderer,
                              const char* svg_xml,
                              cssboxElement* parent);

/**
 * @brief Load SVG from file
 *
 * @param renderer CSS renderer
 * @param filepath Path to SVG file
 * @param parent Parent element (NULL for root)
 * @return Root SVG element, or NULL on error
 */
cssboxElement* cssboxLoadSVGFile(cssboxRenderer* renderer,
                                  const char* filepath,
                                  cssboxElement* parent);

/**
 * @brief Parse SVG and return CSS stylesheet
 *
 * Extracts <style> elements from SVG and returns combined CSS.
 * Useful when you want to apply SVG's embedded styles.
 *
 * @param svg_xml SVG XML string
 * @return CSS string (caller must free)
 */
char* cssboxExtractSVGStyles(const char* svg_xml);

#ifdef __cplusplus
}

namespace cssbox {

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
    static cssboxElement* parse(cssboxRenderer* renderer,
                                 const std::string& svg_xml,
                                 const Options& options = {});

    /**
     * @brief Parse SVG file
     */
    static cssboxElement* parseFile(cssboxRenderer* renderer,
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
    static void resolve_gradient_references(cssboxRenderer* renderer, cssboxElement* element);
};

} // namespace cssbox

#endif // __cplusplus

#endif // cssbox_SVG_XML_H
