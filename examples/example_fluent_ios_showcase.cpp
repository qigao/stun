#include <memory>
#include <utility>
#include <vector>

#include <nanogui.h>
#include <nanogui/fluent_ios.h>

using namespace nanogui;

class FluentIOSDemo : public Screen {
public:
    FluentIOSDemo() : Screen(Vector2i(420, 720), "Fluent iOS Showcase") {
        initialize_ui();
        perform_layout();
    }

private:
    void initialize_ui() {
        m_iosTheme = std::make_unique<FluentIOSTheme>(nvg_context());
        set_theme(m_iosTheme.get());

        auto *nav = new FluentIOSNavigationBar(this, "Home");
        nav->set_subtitle("Fluent iOS");
        nav->set_position(Vector2i(0, 0));
        nav->set_size(Vector2i(m_size.x(), nav->preferred_size(nvg_context()).y()));

        m_segmented = new FluentIOSSegmentedControl(this, {"All", "Favorites", "Recents"});
        m_segmented->set_position(Vector2i(20, nav->size().y() + 12));
        Vector2i seg_pref = m_segmented->preferred_size(nvg_context());
        m_segmented->set_size(Vector2i(m_size.x() - 40, seg_pref.y()));

        auto *scroll = new VScrollPanel(this);
        scroll->set_position(Vector2i(0, m_segmented->position().y() + m_segmented->size().y() + 12));
        scroll->set_size(Vector2i(m_size.x(), m_size.y() - scroll->position().y()));

        Widget *list_root = new Widget(scroll);
        list_root->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 0, 1));

        Widget *input_section = new Widget(list_root);
        input_section->set_layout(new BoxLayout(Orientation::Vertical, Alignment::Fill, 16, 12));

        auto *section_title = new Label(input_section, "Quick Actions", "sans-bold", 16);
        section_title->set_color(m_iosTheme->m_text_color);

        m_feedbackField = new FluentIOSTextField(input_section);
        m_feedbackField->set_placeholder("Share quick notes...");
        m_feedbackField->set_fixed_height(m_feedbackField->preferred_size(nvg_context()).y());

        Widget *switch_row = new Widget(input_section);
        auto *switch_layout = new GridLayout(Orientation::Horizontal, 2, Alignment::Middle, 0, 12);
        switch_row->set_layout(switch_layout);

        auto *toggle_label = new Label(switch_row, "Alerts", "sans", 15);
        toggle_label->set_color(m_iosTheme->m_text_color);

        m_alertSwitch = new FluentIOSSwitch(switch_row, true);
        m_alertSwitch->set_fixed_size(m_alertSwitch->preferred_size(nvg_context()));

        m_alertStatus = new Label(input_section, std::string(), "sans", 13);

        auto update_alert_label = [this](bool enabled) {
            if (!m_alertStatus)
                return;
            Color base = m_iosTheme->m_text_color;
            Color accent = m_iosTheme->m_button_gradient_top_focused;
            Color status_color = enabled ? accent : Color(base.r(), base.g(), base.b(), 0.6f);
            m_alertStatus->set_color(status_color);
            m_alertStatus->set_caption(enabled ? "Alerts enabled" : "Alerts disabled");
        };

        m_alertSwitch->set_callback([this, update_alert_label](bool enabled) {
            update_alert_label(enabled);
        });
        update_alert_label(m_alertSwitch->state());

        m_feedbackField->set_callback([this, update_alert_label](const std::string &value) {
            if (!m_alertStatus)
                return true;
            if (value.empty()) {
                bool enabled = m_alertSwitch ? m_alertSwitch->state() : false;
                update_alert_label(enabled);
            } else {
                m_alertStatus->set_caption("Thanks for the note!");
                Color accent = m_iosTheme->m_button_gradient_top_unfocused;
                m_alertStatus->set_color(accent);
            }
            return true;
        });

        std::vector<std::pair<std::string, std::string>> items = {
            {"Design Guidelines", "Updated two days ago"},
            {"Navigation Patterns", "Last opened yesterday"},
            {"Colors", "12 swatches"},
            {"Typography", "SF Pro + Fluent tokens"},
            {"Motion", "Spring response, easing"}
        };

        m_cells.reserve(items.size());
        for (const auto &entry : items) {
            auto *cell = new FluentIOSListCell(list_root, entry.first, entry.second,
                                               FluentIOSListCell::Accessory::Chevron);
            cell->set_fixed_height(72);
            cell->set_callback([this, cell]() {
                for (auto *other : m_cells)
                    other->set_selected(false);
                cell->set_selected(true);
            });
            m_cells.push_back(cell);
        }

        if (!m_cells.empty())
            m_cells.front()->set_selected(true);

        m_segmented->set_callback([this](int index) {
            if (m_cells.empty())
                return;
            index = index % static_cast<int>(m_cells.size());
            for (size_t i = 0; i < m_cells.size(); ++i)
                m_cells[i]->set_selected(static_cast<int>(i) == index);

            if (m_feedbackField) {
                const char *placeholders[] = {
                    "Share quick notes...",
                    "Add a favorite summary...",
                    "Capture your latest activity..."
                };
                size_t count = sizeof(placeholders) / sizeof(placeholders[0]);
                m_feedbackField->set_placeholder(placeholders[index % count]);
            }
        });
    }

    std::unique_ptr<FluentIOSTheme> m_iosTheme;
    FluentIOSSegmentedControl *m_segmented = nullptr;
    FluentIOSTextField *m_feedbackField = nullptr;
    FluentIOSSwitch *m_alertSwitch = nullptr;
    Label *m_alertStatus = nullptr;
    std::vector<FluentIOSListCell *> m_cells;
};

int main() {
    nanogui::init();
    {
        nanogui::ref<FluentIOSDemo> app = new FluentIOSDemo();
        app->set_visible(true);
        app->perform_layout();
        nanogui::run();
    }
    nanogui::shutdown();
    return 0;
}
