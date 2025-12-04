#pragma once

#include <nanovg.h>
#include <string>
#include <vector>
#include <unordered_map>

namespace flexui {

class FontManager {
public:
    explicit FontManager(NVGcontext* vg);
    ~FontManager() = default;

    // Load a font family with optional fallback fonts
    bool loadFont(const std::string& name, const std::vector<std::string>& paths);
    
    // Load a single font
    bool loadFont(const std::string& name, const std::string& path);
    
    // Replace an existing font with a new one
    bool replaceFont(const std::string& name, const std::vector<std::string>& paths);
    
    // Load default fonts (Latin + CJK)
    void loadDefaultFonts();
    
    // Get font ID by name
    int getFontId(const std::string& name) const;
    
    // Check if font is loaded
    bool isFontLoaded(const std::string& name) const;

private:
    NVGcontext* vg_;
    std::unordered_map<std::string, int> font_map_;  // name -> font ID
    
    // Try to load font from multiple paths
    int tryLoadFont(const std::string& name, const std::vector<std::string>& paths);
};

} // namespace flexui
