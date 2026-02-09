#include "meta_editor/page.h"
#include "meta_editor/canvas.h"
#include "meta_editor/selection_manager.h"
#include "meta_editor/command.h"
#include "meta_editor/connector.h"
#include "meta_editor/exporter.h"
#include "meta_editor/serializer.h"
#include "meta_editor/svg_importer.h"
#include <algorithm>

namespace meta_editor {

// --- Page Implementation ---

Page::Page(const std::string& name, float width, float height)
    : name_(name), width_(width), height_(height) {
    canvas_ = std::make_unique<Canvas>(width, height);
    selection_ = std::make_unique<SelectionManager>(canvas_.get());
    command_manager_ = std::make_unique<CommandManager>();
    connector_manager_ = std::make_unique<ConnectorManager>(canvas_.get());
    exporter_ = std::make_unique<Exporter>(canvas_.get());
    serializer_ = std::make_unique<Serializer>(canvas_.get());
    svg_importer_ = std::make_unique<SvgImporter>(canvas_.get());
    
    canvas_->create_layer("Layer 1");
}

Page::~Page() = default;

void Page::set_size(float width, float height) {
    width_ = width;
    height_ = height;
    if (canvas_) {
        canvas_->set_size(width, height);
    }
}

void Page::update(float dt) {
    canvas_->update(dt);
    connector_manager_->update_all();
}

void Page::render(flex::Renderer& renderer) {
    canvas_->render(renderer);
}

// --- PageManager Implementation ---

PageManager::PageManager(float default_width, float default_height)
    : default_width_(default_width), default_height_(default_height) {}

PageManager::~PageManager() = default;

Page* PageManager::create_page(const std::string& name) {
    auto page = std::make_unique<Page>(name, default_width_, default_height_);
    Page* ptr = page.get();
    pages_.push_back(std::move(page));
    
    if (!active_page_) {
        active_page_ = ptr;
    }
    
    return ptr;
}

void PageManager::add_page(std::unique_ptr<Page> page) {
    if (!page) return;
    active_page_ = page.get();
    pages_.push_back(std::move(page));
}

bool PageManager::remove_page(int index) {
    if (index < 0 || index >= (int)pages_.size()) return false;
    
    Page* page_to_remove = pages_[index].get();
    pages_.erase(pages_.begin() + index);
    
    if (active_page_ == page_to_remove) {
        if (pages_.empty()) {
            active_page_ = nullptr;
        } else {
            int new_idx = std::max(0, index - 1);
            active_page_ = pages_[new_idx].get();
        }
    }
    
    return true;
}

bool PageManager::remove_page(Page* page) {
    auto it = std::find_if(pages_.begin(), pages_.end(), 
        [page](const auto& p) { return p.get() == page; });
    
    if (it != pages_.end()) {
        int index = (int)std::distance(pages_.begin(), it);
        return remove_page(index);
    }
    return false;
}

void PageManager::set_active_page(int index) {
    if (index >= 0 && index < (int)pages_.size()) {
        active_page_ = pages_[index].get();
    }
}

void PageManager::set_active_page(Page* page) {
    auto it = std::find_if(pages_.begin(), pages_.end(), 
        [page](const auto& p) { return p.get() == page; });
    
    if (it != pages_.end()) {
        active_page_ = page;
    }
}

int PageManager::active_page_index() const {
    for (int i = 0; i < (int)pages_.size(); ++i) {
        if (pages_[i].get() == active_page_) return i;
    }
    return -1;
}

Page* PageManager::get_page(int index) const {
    if (index >= 0 && index < (int)pages_.size()) {
        return pages_[index].get();
    }
    return nullptr;
}

void PageManager::update(float dt) {
    if (active_page_) {
        active_page_->update(dt);
    }
}

} // namespace meta_editor
