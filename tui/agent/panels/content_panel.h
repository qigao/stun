#pragma once
#include <flexUI/box.h>
#include <flexUI/widgets/virtualized_list_widget.h>
#include <flexUI/widgets/markdown_widget.h>
#include <flexUI/widgets/label_widget.h>
#include "../agent_model.h"

using namespace flexUI;

class ContentPanel {
public:
    static VirtualizedListWidget* create(Box* box, Element* parent, const AgentModel* model) {
        auto* content = box->create("div", "content");
        content->add_class("content");
        parent->append(content);
        
        auto* chat_list = new VirtualizedListWidget();
        auto* list_elem = box->create_with_widget("div", chat_list, "chat-list");
        list_elem->add_class("chat-list");
        content->append(list_elem);

        chat_list->set_row_height(400); 

        // ROW CREATION
        chat_list->set_create_row_fn([box]() {
            auto* row = box->create("div", "");
            row->add_class("message-row");
            
            auto* header = box->create("div", "");
            header->add_class("msg-header");
            row->append(header);
            
            auto* badge = box->create_widget<LabelWidget>("label", "", "ROLE");
            badge->add_class("role-badge"); 
            header->append(badge);
            
            auto* content = box->create_widget<MarkdownWidget>("div", "", "Content");
            content->add_class("msg-content");
            row->append(content);
            
            return row;
        });

        // ROW BINDING
        // We capture model by value pointer (const AgentModel*)
        // Note: In real app, consider lifetime issues. Here View owns everything usually.
        chat_list->set_bind_row_fn([model](Element* row, int index) {
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
            
            // 1. Badge
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
            
            // 2. Content
            auto* content_elem = row->child_at(1);
            auto* md_w = content_elem->widget_as<MarkdownWidget>();
            if (md_w) {
                md_w->set_markdown(msg.content);
            }
        });

        return chat_list;
    }
};
