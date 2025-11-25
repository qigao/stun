#include <flexui/screen.h>
#include <flexui/tabbar.h>
#include <flexui/dropdown.h>
#include <flexui/textbox.h>
#include <flexui/checkbox.h>
#include <flexui/label.h>
#include <iostream>

int main() {
    flexui::Screen screen(600, 550, "All Widgets XML Demo");

    // CSS styling
    screen.loadCSS(R"(
        #main-panel {
            display: flex;
            flex-direction: column;
            padding: 20px;
            width: 560px;
            height: 510px;
            background: #ffffff;
            border: 2px solid #607D8B;
            border-radius: 12px;
            position: absolute;
            top: 20px;
            left: 20px;
        }
        #title {
            width: 520px;
            height: 36px;
        }
        tabbar {
            width: 520px;
            height: 45px;
        }
        hr {
            width: 520px;
            height: 1px;
        }
        .section {
            display: flex;
            flex-direction: column;
            gap: 12px;
            width: 520px;
            height: 320px;
            padding: 15px;
        }
        .row {
            display: flex;
            flex-direction: row;
            gap: 15px;
            width: 490px;
            height: 45px;
        }
        label {
            width: 120px;
            height: 40px;
        }
        select {
            width: 200px;
            height: 40px;
        }
        input {
            width: 200px;
            height: 40px;
        }
        checkbox {
            width: 24px;
            height: 24px;
        }
        img {
            width: 100px;
            height: 100px;
        }
        button {
            width: 200px;
            height: 45px;
        }
        #status {
            width: 520px;
            height: 30px;
        }
    )");

    // XML-based UI definition
    screen.loadXML(R"(
        <div id="main-panel">
            <label id="title">Widget Showcase</label>
            <tabbar id="tabs" tabs="Profile, Settings, About"/>
            <hr id="divider1"/>

            <div class="section">
                <div class="row">
                    <label>Name:</label>
                    <input id="name-input" placeholder="Enter your name"/>
                </div>

                <div class="row">
                    <label>Country:</label>
                    <select id="country-dropdown" items="USA, Canada, UK, Germany, Japan, China"/>
                </div>

                <div class="row">
                    <label>Theme:</label>
                    <select id="theme-dropdown" items="Light, Dark, System"/>
                </div>

                <div class="row">
                    <checkbox id="notifications-check"/>
                    <label>Enable Notifications</label>
                </div>

                <hr id="divider2"/>

                <div class="row">
                    <button id="save-btn" onclick="handleSave">Save Settings</button>
                    <button id="reset-btn" onclick="handleReset">Reset</button>
                </div>
            </div>

            <label id="status">Ready</label>
        </div>
    )");

    // Register event handlers
    screen.registerHandler("handleSave", [&](flexui::Widget* w) {
        auto* nameInput = dynamic_cast<flexui::TextBox*>(screen.findWidget("name-input"));
        auto* countryDropdown = dynamic_cast<flexui::Dropdown*>(screen.findWidget("country-dropdown"));
        auto* themeDropdown = dynamic_cast<flexui::Dropdown*>(screen.findWidget("theme-dropdown"));
        auto* notificationsCheck = dynamic_cast<flexui::Checkbox*>(screen.findWidget("notifications-check"));

        std::cout << "=== Settings Saved ===" << std::endl;
        if (nameInput)
            std::cout << "Name: " << nameInput->getInputText() << std::endl;
        if (countryDropdown)
            std::cout << "Country: " << countryDropdown->getSelectedItem() << std::endl;
        if (themeDropdown)
            std::cout << "Theme: " << themeDropdown->getSelectedItem() << std::endl;
        if (notificationsCheck)
            std::cout << "Notifications: " << (notificationsCheck->isChecked() ? "Enabled" : "Disabled") << std::endl;

        // Update status
        auto* status = dynamic_cast<flexui::Label*>(screen.findWidget("status"));
        if (status) status->setText("Settings saved!");

        return true;
    });

    screen.registerHandler("handleReset", [&](flexui::Widget* w) {
        auto* nameInput = dynamic_cast<flexui::TextBox*>(screen.findWidget("name-input"));
        auto* countryDropdown = dynamic_cast<flexui::Dropdown*>(screen.findWidget("country-dropdown"));
        auto* themeDropdown = dynamic_cast<flexui::Dropdown*>(screen.findWidget("theme-dropdown"));
        auto* notificationsCheck = dynamic_cast<flexui::Checkbox*>(screen.findWidget("notifications-check"));

        if (nameInput) nameInput->setText("");
        if (countryDropdown) countryDropdown->setSelectedIndex(0);
        if (themeDropdown) themeDropdown->setSelectedIndex(0);
        if (notificationsCheck) notificationsCheck->setChecked(false);

        auto* status = dynamic_cast<flexui::Label*>(screen.findWidget("status"));
        if (status) status->setText("Settings reset!");

        std::cout << "=== Settings Reset ===" << std::endl;
        return true;
    });

    // TabBar change callback
    auto* tabs = dynamic_cast<flexui::TabBar*>(screen.findWidget("tabs"));
    if (tabs) {
        tabs->setTabChangeCallback([&](int index, const std::string& name) {
            auto* status = dynamic_cast<flexui::Label*>(screen.findWidget("status"));
            if (status) {
                status->setText("Tab: " + name);
            }
            std::cout << "Tab changed to: " << name << " (index " << index << ")" << std::endl;
        });
    }

    // Main loop
    while (screen.pollEvents()) {
        screen.draw();
    }

    return 0;
}
