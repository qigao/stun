#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>

namespace whiteboard {
namespace ddf {

// Forward declarations
class DDFDocument;
struct Shape;

/**
 * @brief SVG importer for converting SVG documents to DDF format
 * 
 * The SVGImporter parses SVG files and converts them to DDF shapes,
 * preserving hierarchy, styles, and geometry as much as possible.
 */
class SVGImporter {
public:
    SVGImporter() = default;
    ~SVGImporter() = default;

    /**
     * @brief Import an SVG string into a DDF document
     * @param svg_string The SVG content to import
     * @param doc The DDF document to populate
     * @return true if import was successful, false otherwise
     */
    bool import(const std::string& svg_string, DDFDocument& doc);

    /**
     * @brief Get the last error message
     * @return Error message from the last import attempt
     */
    const std::string& get_last_error() const { return last_error_; }

private:
    // Forward declaration of implementation class
    class Impl;
    
    std::string last_error_;
};

} // namespace ddf
} // namespace whiteboard
