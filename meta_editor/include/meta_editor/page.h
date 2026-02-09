#pragma once

#include <vector>
#include <memory>
#include <string>

namespace flex {
    class Renderer;
}

namespace meta_editor {

class Canvas;
class SelectionManager;
class CommandManager;
class ConnectorManager;
class Exporter;
class Serializer;
class SvgImporter;

/**
 * Page - Encapsulates all state for a single drawing/document.
 */
class Page {
public:
    Page(const std::string& name, float width, float height);
    ~Page();

    // Core systems
    Canvas* canvas() { return canvas_.get(); }
    SelectionManager* selection() { return selection_.get(); }
    CommandManager* command_manager() { return command_manager_.get(); }
    ConnectorManager* connector_manager() { return connector_manager_.get(); }
    Exporter* exporter() { return exporter_.get(); }
    Serializer* serializer() { return serializer_.get(); }
    SvgImporter* svg_importer() { return svg_importer_.get(); }

    const std::string& name() const { return name_; }
    void set_name(const std::string& name) { name_ = name; }

    float width() const { return width_; }
    float height() const { return height_; }
    void set_size(float width, float height);

    void update(float dt);
    void render(flex::Renderer& renderer);

private:
    std::string name_;
    float width_;
    float height_;

    std::unique_ptr<Canvas> canvas_;
    std::unique_ptr<SelectionManager> selection_;
    std::unique_ptr<CommandManager> command_manager_;
    std::unique_ptr<ConnectorManager> connector_manager_;
    std::unique_ptr<Exporter> exporter_;
    std::unique_ptr<Serializer> serializer_;
    std::unique_ptr<SvgImporter> svg_importer_;
};

/**
 * PageManager - Manages multiple independent pages.
 */
class PageManager {
public:
    PageManager(float default_width, float default_height);
    ~PageManager();

    // Page management
    Page* create_page(const std::string& name);
    void add_page(std::unique_ptr<Page> page);
    bool remove_page(int index);
    bool remove_page(Page* page);

    // Active page
    Page* active_page() const { return active_page_; }
    void set_active_page(int index);
    void set_active_page(Page* page);
    int active_page_index() const;

    // Access
    int page_count() const { return (int)pages_.size(); }
    Page* get_page(int index) const;
    const std::vector<std::unique_ptr<Page>>& pages() const { return pages_; }

    void update(float dt);

private:
    std::vector<std::unique_ptr<Page>> pages_;
    Page* active_page_ = nullptr;
    float default_width_, default_height_;
};

} // namespace meta_editor
