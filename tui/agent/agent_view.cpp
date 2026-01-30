#include "agent_view.h"
#include "styles.h"
#include "panels/top_panel.h"
#include "panels/content_panel.h"
#include "panels/bottom_panel.h"

#include <flexUI/box.h>
#include <flexUI/widgets/input_widget.h>
#include <flexUI/widgets/virtualized_list_widget.h>
#include <flexUI/widgets/draggable_widget.h>

using namespace flexUI;

AgentView::AgentView(tui::Terminal& term, flex::Renderer* renderer) 
    : term_(term), renderer_(renderer) {}

AgentView::~AgentView() {
    // box is unique_ptr
}

bool AgentView::init() {
    box_ = std::make_unique<Box>(renderer_);
    if (!box_) return false;
    
    box_->load_css(LayoutStyles::CSS);
    update_viewport();
    
    // We can't build UI fully until model is set? 
    // Actually we can, but bind functions need model reference.
    // Ideally set_model_reference is called before init or we delay binding.
    // For now, assume model is set later, but list binding needs it.
    // Let's postpone binding or check for null in bind (which we did).
    build_ui();
    return true;
}

void AgentView::set_model_reference(AgentModel* model) {
    model_ = model;
    
    if (chat_list_) {
        // Re-set bind fn with valid model
        chat_list_->set_bind_row_fn([model](Element* row, int index) {
            if (!model || index < 0 || index >= (int)model->messages().size()) return;
            const auto& msg = model->messages()[index];
            
            // Set role-based classes
            row->remove_class("user-msg");
            row->remove_class("ai-msg");
            row->remove_class("sys-msg");
            if (msg.role == "User") row->add_class("user-msg");
            else if (msg.role == "AI") row->add_class("ai-msg");
            else row->add_class("sys-msg");

            if (row->child_count() < 2) return;
            
            auto* header_elem = row->child_at(0);
            if (header_elem && header_elem->child_count() > 0) {
                auto* badge_elem = header_elem->child_at(0);
                auto* badge_w = badge_elem->widget_as<LabelWidget>();
                if (badge_w) {
                    badge_w->set_text(msg.role == "User" ? "USER >" : (msg.role == "AI" ? "AI >" : "SYS >"));
                    if (msg.role == "User") badge_elem->computed_style->text_color = {0.3f, 0.7f, 1.0f, 1.0f};
                    else if (msg.role == "System") badge_elem->computed_style->text_color = {1.0f, 0.3f, 0.3f, 1.0f};
                    else badge_elem->computed_style->text_color = {1.0f, 0.6f, 0.2f, 1.0f};
                }
            }
            
            auto* content_elem = row->child_at(1);
            auto* md_w = content_elem->widget_as<MarkdownWidget>();
            if (md_w) {
                md_w->set_markdown(msg.content);
            }
        });
    }

    // Update initial model name
    auto* model_val = box_->get_by_id("status-model");
    if (model_val) {
        auto* label = model_val->widget_as<LabelWidget>();
        if (label) label->set_text(model_->model_name());
    }
}

void AgentView::update(float dt) {
    box_->update_time(dt);
    box_->update();
}

void AgentView::dispatch(flexUI::Event& evt) {
    box_->dispatch_event(evt);
}

void AgentView::update_viewport() {
    float vp_w = static_cast<float>(term_.width() * flex::tui_backend::CHAR_WIDTH);
    float vp_h = static_cast<float>(term_.height() * flex::tui_backend::CHAR_HEIGHT);
    box_->set_viewport(vp_w, vp_h);
}

bool AgentView::handle_key(tui::Key key) {
    if (is_selector_open()) {
        if (key == tui::Key::Up) {
            model_selected_index_ = (model_selected_index_ > 0) ? model_selected_index_ - 1 : (int)model_options_.size() - 1;
            show_model_selector(); // This will refresh active classes
            return true;
        }
        if (key == tui::Key::Down) {
            model_selected_index_ = (model_selected_index_ + 1) % (int)model_options_.size();
            show_model_selector();
            return true;
        }
        if (key == tui::Key::Enter) {
            if (model_selected_index_ >= 0 && model_selected_index_ < (int)model_options_.size()) {
                if (model_options_[model_selected_index_]->click_callback())
                    model_options_[model_selected_index_]->click_callback()();
            }
            return true;
        }
    }
    
    if (is_cmd_popup_open()) {
        if (key == tui::Key::Up) {
            cmd_selected_index_ = (cmd_selected_index_ > 0) ? cmd_selected_index_ - 1 : (int)cmd_options_.size() - 1;
            // Refresh cmd styles
            for (int i=0; i<(int)cmd_options_.size(); ++i) {
                cmd_options_[i]->remove_class("active");
                if (i == cmd_selected_index_) cmd_options_[i]->add_class("active");
                cmd_options_[i]->mark_style_dirty();
            }
            return true;
        }
        if (key == tui::Key::Down) {
            cmd_selected_index_ = (cmd_selected_index_ + 1) % (int)cmd_options_.size();
            for (int i=0; i<(int)cmd_options_.size(); ++i) {
                cmd_options_[i]->remove_class("active");
                if (i == cmd_selected_index_) cmd_options_[i]->add_class("active");
                cmd_options_[i]->mark_style_dirty();
            }
            return true;
        }
        if (key == tui::Key::Enter) {
            if (cmd_selected_index_ >= 0 && cmd_selected_index_ < (int)cmd_options_.size()) {
                if (cmd_options_[cmd_selected_index_]->click_callback())
                    cmd_options_[cmd_selected_index_]->click_callback()();
            }
            return true;
        }
    }
    
    return false;
}

std::string AgentView::consume_input() {
    if (!main_input_) return "";
    std::string text = main_input_->text();
    main_input_->set_text("");
    return text;
}

void AgentView::sync_messages() {
    if (!chat_list_ || !model_) return;
    int count = (int)model_->messages().size();
    chat_list_->set_item_count(count);
    chat_list_->refresh();
    chat_list_->scroll_to(count - 1);
    
    // Update status bar count
    auto* msg_val = box_->get_by_id("status-count");
    if (msg_val) {
        auto* label = msg_val->widget_as<LabelWidget>();
        if (label) label->set_text(std::to_string(count));
    }

    // Update status bar state
    auto* status_val = box_->get_by_id("status-state");
    if (status_val) {
        auto* label = status_val->widget_as<LabelWidget>();
        if (label) {
            label->set_text(count > 0 ? "ACTIVE" : "READY");
            status_val->remove_class("active");
            if (count > 0) status_val->add_class("active");
        }
    }

    auto* model_val = box_->get_by_id("status-model");
    if (model_val) {
        auto* label = model_val->widget_as<LabelWidget>();
        if (label) label->set_text(model_->model_name());
    }

    // Ensure the list element is marked dirty for next redraw
    auto* list_elem = box_->get_by_id("chat-list");
    if (list_elem) list_elem->mark_paint_dirty();
}

void AgentView::focus_input() {
    if (input_wrapper_) box_->set_focus(input_wrapper_);
}

void AgentView::build_ui() {
    auto* root = box_->create("div", "root");
    root->add_class("root");
    box_->set_root(root);
    
    // Top Panel
    TopPanel::create(box_.get(), root);
    
    // Content Panel
    // Note: model_ might be null here, so the initial binding might be no-op. 
    // set_model_reference will fix it.
    chat_list_ = ContentPanel::create(box_.get(), root, model_);
    
    // Bottom Panel
    BottomPanel::create(box_.get(), root, &main_input_, &input_wrapper_);

    create_command_popup(root);
    create_model_selector(root);

    if (main_input_) {
        main_input_->set_change_callback([this](const std::string& text) {
            handle_input_change(text);
        });
    }

    // Click model badge to open selector
    auto* model_badge = box_->get_by_id("status-model-item");
    if (model_badge) {
        auto cb = [this]() { show_model_selector(); };
        model_badge->on_click(cb);
        // Also bind to children because non-bubbling click
        model_badge->for_each_child([cb](Element* child) {
            child->on_click(cb);
        });
    }
}

void AgentView::create_model_selector(Element* parent) {
    selector_overlay_ = box_->create("div", "model-selector-overlay");
    selector_overlay_->add_class("selector-overlay");
    selector_overlay_->add_class("hidden");
    parent->append(selector_overlay_);
    
    // Close on overlay click
    selector_overlay_->on_click([this]() { hide_model_selector(); });

    auto* panel = box_->create("div", "model-selector-panel");
    panel->add_class("selector-panel");
    // Prevent closing when clicking panel
    panel->on_click([](){}); 
    selector_overlay_->append(panel);

    auto* header = box_->create("div", "");
    header->add_class("selector-header");
    header->append(box_->create_widget<LabelWidget>("label", "", "SELECT AI MODEL"))->add_class("selector-title");
    panel->append(header);

    auto add_option = [&](const std::string& name, const std::string& desc, const std::string& model_id) {
        auto* opt = box_->create("div", "model-opt-" + model_id);
        opt->add_class("model-option");
        
        auto* name_lbl = box_->create_widget<LabelWidget>("label", "", name);
        name_lbl->add_class("opt-name");
        opt->append(name_lbl);
        
        auto* desc_lbl = box_->create_widget<LabelWidget>("label", "", desc);
        desc_lbl->add_class("opt-desc");
        opt->append(desc_lbl);
        
        auto cb = [this, model_id]() {
            if (model_) {
                model_->set_model_name(model_id);
                sync_messages(); // Refresh UI/status
            }
            hide_model_selector();
        };

        opt->on_click(cb);
        name_lbl->on_click(cb);
        desc_lbl->on_click(cb);

        panel->append(opt);
        model_options_.push_back(opt);
    };

    model_options_.clear();
    add_option("GPT-4o", "OpenAI - Most capable model", "gpt-4o");
    add_option("GPT-4 Turbo", "OpenAI - Fast and accurate", "gpt-4-turbo");
    add_option("GPT-5 Preview", "OpenAI - Next gen preview", "gpt-5-preview");
    add_option("Claude 3 Opus", "Anthropic - High reasoning", "claude-3-opus");

    panel->append(box_->create_widget<LabelWidget>("label", "", "Use Up/Down to select | Enter to confirm"))->add_class("selector-hint");
}

void AgentView::show_model_selector() {
    if (!selector_overlay_ || !model_) return;
    
    std::string current = model_->model_name();
    std::vector<std::string> models = {"gpt-4o", "gpt-4-turbo", "gpt-5-preview", "claude-3-opus"};
    
    // Update active classes
    // Use custom selected index if changed by keys, otherwise follow model
    if (model_selected_index_ < 0) {
        for (int i = 0; i < (int)models.size(); ++i) {
            if (models[i] == current) model_selected_index_ = i;
        }
    }

    for (int i = 0; i < (int)models.size(); ++i) {
        auto* opt = box_->get_by_id("model-opt-" + models[i]);
        if (opt) {
            opt->remove_class("active");
            if (i == model_selected_index_) opt->add_class("active");
            opt->mark_style_dirty();
        }
    }

    selector_overlay_->remove_class("hidden");
    selector_overlay_->add_class("visible");
    selector_overlay_->mark_style_dirty();
}

void AgentView::hide_model_selector() {
    if (selector_overlay_) {
        selector_overlay_->remove_class("visible");
        selector_overlay_->add_class("hidden");
        selector_overlay_->mark_style_dirty();
        model_selected_index_ = -1; // Reset selection
    }
}

bool AgentView::is_selector_open() const {
    return selector_overlay_ && selector_overlay_->has_class("visible");
}

void AgentView::create_command_popup(Element* parent) {
    cmd_popup_ = box_->create_with_widget("div", new DraggableWidget(), "cmd-popup");
    cmd_popup_->add_class("cmd-popup");
    cmd_popup_->add_class("hidden");
    parent->append(cmd_popup_);

    // Header
    auto* header = box_->create("div", "");
    header->add_class("cmd-header");
    cmd_popup_->append(header);

    auto* title = box_->create_widget<LabelWidget>("label", "", "AVAILABLE COMMANDS");
    title->add_class("cmd-title");
    header->append(title);

    auto* close_btn = box_->create_widget<LabelWidget>("label", "", " [X] ");
    close_btn->add_class("cmd-close");
    close_btn->on_click([this]() { hide_command_popup(); });
    header->append(close_btn);

    // Container for dynamic items
    cmd_container_ = box_->create("div", "");
    cmd_container_->add_class("cmd-container");
    cmd_popup_->append(cmd_container_);
}

void AgentView::hide_command_popup() {
    if (cmd_popup_) {
        cmd_popup_->remove_class("visible");
        cmd_popup_->add_class("hidden");
        cmd_popup_->mark_style_dirty();
        cmd_popup_->mark_layout_dirty();
    }
}

void AgentView::handle_input_change(const std::string& text) {
    if (!cmd_popup_) return;
    
    // Only show if starts with / and not empty
    if (text.empty() || text[0] != '/') {
        cmd_popup_->computed_style->visibility = Visibility::Hidden;
        cmd_popup_->mark_style_dirty();
        return;
    }

    // Filter commands
    std::vector<std::pair<std::string, std::string>> matches;
    if (text == "/") {
        matches = commands_;
    } else {
        for (const auto& cmd : commands_) {
            if (cmd.first.find(text) == 0) {
                matches.push_back(cmd);
            }
        }
    }

    if (matches.empty()) {
        cmd_popup_->computed_style->visibility = Visibility::Hidden;
        cmd_popup_->mark_style_dirty();
        return;
    }

    // Rebuild popup content
    // Clear children first
    while (cmd_container_->child_count() > 0) {
        cmd_container_->remove(cmd_container_->child_at(0));
    }
    cmd_options_.clear();
    cmd_selected_index_ = 0;

    for (int i=0; i < (int)matches.size(); ++i) {
        const auto& match = matches[i];
        auto* item = box_->create("div", "");
        item->add_class("cmd-item");
        if (i == cmd_selected_index_) item->add_class("active");
        cmd_options_.push_back(item);
        
        auto* name = box_->create_widget<LabelWidget>("label", "", match.first);
        name->add_class("cmd-name");
        item->append(name);
        
        auto* desc = box_->create_widget<LabelWidget>("label", "", match.second);
        desc->add_class("cmd-desc");
        item->append(desc);
        
        // Click to autocomplete
        std::string cmd_str = match.first;
        auto cb = [this, cmd_str]() {
            if (main_input_) {
                main_input_->set_text(cmd_str);
                main_input_->set_cursor_pos((int)cmd_str.length());
                // Hide popup
                hide_command_popup();
                // Focus input
                focus_input();
            }
        };

        item->on_click(cb);
        name->on_click(cb);
        desc->on_click(cb);

        cmd_container_->append(item);
    }

    cmd_popup_->remove_class("hidden");
    cmd_popup_->add_class("visible");
    cmd_popup_->mark_style_dirty();
    cmd_popup_->mark_layout_dirty();
}

bool AgentView::is_cmd_popup_open() const {
    return cmd_popup_ && cmd_popup_->has_class("visible");
}
