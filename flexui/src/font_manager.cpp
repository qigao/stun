#include <flexui/font_manager.h>
#include <fmtlog.h>

namespace flexui {

FontManager::FontManager(NVGcontext* vg) : vg_(vg) {}

bool FontManager::loadFont(const std::string& name, const std::string& path) {
    return loadFont(name, std::vector<std::string>{path});
}

bool FontManager::loadFont(const std::string& name, const std::vector<std::string>& paths) {
    if (isFontLoaded(name)) {
        return true;
    }
    
    int font_id = tryLoadFont(name, paths);
    if (font_id == -1) {
        return false;
    }
    
    font_map_[name] = font_id;
    return true;
}

bool FontManager::replaceFont(const std::string& name, const std::vector<std::string>& paths) {
    // Remove old font from map (NanoVG will keep the ID valid)
    font_map_.erase(name);
    
    // Load new font with same name
    int font_id = tryLoadFont(name, paths);
    if (font_id == -1) {
        return false;
    }
    
    font_map_[name] = font_id;
    return true;
}

void FontManager::loadDefaultFonts() {
    // Load Latin font
    loadFont("sans-serif", "resources/Roboto-Regular.ttf");
    
    // Load CJK font with fallbacks
    std::vector<std::string> cjk_paths = {
        "C:/Windows/Fonts/msyh.ttc",      // Microsoft YaHei
        "C:/Windows/Fonts/simhei.ttf",    // SimHei
        "C:/Windows/Fonts/simsun.ttc",    // SimSun
        "/System/Library/Fonts/PingFang.ttc",  // macOS
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",  // Linux
    };
    
    // Load Emoji font
    std::vector<std::string> emoji_paths = {
        "C:/Windows/Fonts/seguiemj.ttf",  // Segoe UI Emoji (Windows)
        "C:/Windows/Fonts/seguisym.ttf",  // Segoe UI Symbol (Windows fallback)
        "/System/Library/Fonts/Apple Color Emoji.ttc",  // macOS
        "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf",  // Linux
    };
    
    int main_font = getFontId("sans-serif");
    
    // Add CJK as fallback
    if (loadFont("sans-serif-cjk", cjk_paths)) {
        int cjk_font = getFontId("sans-serif-cjk");
        if (main_font != -1 && cjk_font != -1) {
            nvgAddFallbackFontId(vg_, main_font, cjk_font);
        }
    }
    
    // Add Emoji as fallback
    if (loadFont("emoji", emoji_paths)) {
        int emoji_font = getFontId("emoji");
        if (main_font != -1 && emoji_font != -1) {
            nvgAddFallbackFontId(vg_, main_font, emoji_font);
        }
    }
}

int FontManager::getFontId(const std::string& name) const {
    auto it = font_map_.find(name);
    return (it != font_map_.end()) ? it->second : -1;
}

bool FontManager::isFontLoaded(const std::string& name) const {
    return font_map_.find(name) != font_map_.end();
}

int FontManager::tryLoadFont(const std::string& name, const std::vector<std::string>& paths) {
    for (const auto& path : paths) {
        int font_id = nvgCreateFont(vg_, name.c_str(), path.c_str());
        if (font_id != -1) {
            logi("Loaded font '{}' from: {}", name, path);
            return font_id;
        }
    }
    
    loge("Failed to load font '{}' from {} paths", name, paths.size());
    return -1;
}

} // namespace flexui
