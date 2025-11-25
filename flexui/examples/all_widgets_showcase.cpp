#include <flexui/screen.h>
#include <flexui/button.h>
#include <flexui/textbox.h>
#include <flexui/checkbox.h>
#include <flexui/label.h>
#include <flexui/radiobutton.h>
#include <flexui/switch.h>
#include <flexui/slider.h>
#include <flexui/progressbar.h>
#include <flexui/rating.h>
#include <flexui/searchbox.h>
#include <flexui/spinner.h>
#include <flexui/badge.h>
#include <flexui/avatar.h>
#include <flexui/chip.h>
#include <flexui/alert.h>
#include <flexui/card.h>
#include <flexui/tabbar.h>
#include <flexui/dropdown.h>
#include <flexui/breadcrumb.h>
#include <flexui/pagination.h>
#include <flexui/divider.h>
#include <flexui/imageview.h>
#include <flexui/calendar.h>
#include <flexui/colorpicker.h>
#include <flexui/modal.h>
#include <flexui/menu.h>
#include <flexui/iconbutton.h>
#include <flexui/tablist.h>
#include <flexui/toast.h>
#include <flexui/snackbar.h>
#include <flexui/table.h>
#include <iostream>

int main() {
    flexui::Screen screen(1200, 800, "FlexUI - All Widgets Showcase");

    // CSS styling for organized layout
    screen.loadCSS(R"(
        #main-container {
            display: flex;
            flex-direction: column;
            padding: 30px;
            gap: 15px;
            background: #f0f2f5;
            width: 1140px;
            height: 740px;
            position: absolute;
            top: 30px;
            left: 30px;
        }

        .section {
            display: flex;
            flex-direction: column;
            gap: 15px;
            padding: 20px;
            background: white;
            border-radius: 8px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.08);
            width: 527.5px;
            height: 200px;
            flex-basis: calc(50% - 12.5px);
            flex-shrink: 0;
        }

        .section-wide {
            display: flex;
            flex-direction: column;
            gap: 15px;
            padding: 20px;
            background: white;
            border-radius: 8px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.08);
            width: 1080px;
            height: 80px;
            flex-basis: 100%;
            flex-shrink: 0;
        }

        .section-title {
            font-size: 16px;
            font-weight: bold;
            color: #1a73e8;
            width: 100%;
            height: 24px;
            margin-bottom: 5px;
        }

        .widget-row {
            display: flex;
            flex-direction: row;
            gap: 15px;
            flex-wrap: wrap;
            align-items: center;
            min-height: 40px;
        }

        .widget-row > * {
            display: inline-block;
        }

        /* Basic Widgets */
        button {
            width: 110px;
            height: 36px;
        }

        input {
            width: 180px;
            height: 36px;
        }

        checkbox {
            width: 20px;
            height: 20px;
            display: inline-block;
        }

        label {
            height: 24px;
            color: #5f6368;
            font-size: 14px;
        }

        /* Interactive Widgets */
        switch {
            width: 48px;
            height: 26px;
        }

        slider {
            width: 180px;
            height: 32px;
        }

        progressbar {
            width: 180px;
            height: 18px;
        }

        rating {
            width: 110px;
            height: 28px;
        }

        searchbox {
            width: 220px;
            height: 36px;
        }

        /* Display Widgets */
        spinner {
            width: 36px;
            height: 36px;
        }

        badge {
            width: 50px;
            height: 22px;
        }

        avatar {
            width: 36px;
            height: 36px;
        }

        chip {
            width: 80px;
            height: 28px;
        }

        alert {
            width: 500px;
            height: 48px;
        }

        card {
            width: 160px;
            height: 120px;
        }

        /* Navigation Widgets */
        tabbar {
            width: 1105px;
            height: 45px;
        }

        hr {
            width: 1105px;
            height: 1px;
            background: #e0e0e0;
        }

        dropdown {
            width: 180px;
            height: 36px;
        }

        breadcrumb {
            width: 350px;
            height: 32px;
        }

        pagination {
            width: 280px;
            height: 40px;
        }

        radio {
            width: 18px;
            height: 18px;
        }

        /* Complex Widgets */
        calendar {
            width: 260px;
            height: 280px;
        }

        table {
            width: 380px;
            height: 240px;
        }

        iconbutton {
            width: 44px;
            height: 44px;
        }

        colorpicker {
            width: 200px;
            height: 260px;
        }

        tablist {
            width: 180px;
            height: 180px;
        }

        toast {
            width: 280px;
            height: 56px;
        }

        snackbar {
            width: 380px;
            height: 56px;
        }

        modal {
            width: 380px;
            height: 280px;
        }

        menu {
            width: 180px;
            height: 140px;
        }

        hr {
            width: 100%;
            height: 1px;
            background: #e0e0e0;
        }

        img {
            width: 140px;
            height: 90px;
        }

        .tab-content {
            display: none;
            flex-direction: row;
            flex-wrap: wrap;
            gap: 25px;
            width: 1080px;
            min-height: 500px;
        }

        .tab-content.active {
            display: flex;
        }

        #status {
            width: 100%;
            height: 32px;
            color: #1a73e8;
            font-size: 14px;
            font-weight: 500;
        }
    )");

    // XML-based UI with all widgets organized by category
    screen.loadXML(R"(
        <div id="main-container">
            <!-- Top TabBar -->
            <tabbar id="main-tabs" tabs="Basic Controls, Feedback, Navigation"/>

            <hr id="divider-main"/>

            <!-- Tab Content 1: Basic Controls -->
            <div id="tab-content-0" class="tab-content active">
                <div class="section">
                    <label class="section-title">⌨️ Basic Input</label>
                    <div class="widget-row">
                        <button id="btn1">Button</button>
                        <input id="text1" placeholder="Text Input"/>
                    </div>
                    <div class="widget-row">
                        <checkbox id="check1"/>
                        <label>Checkbox</label>
                        <radio id="radio1" name="group1" value="option1"/>
                        <label>Option 1</label>
                        <radio id="radio2" name="group1" value="option2"/>
                        <label>Option 2</label>
                    </div>
                </div>

                <div class="section">
                    <label class="section-title">🎛️ Interactive Controls</label>
                    <div class="widget-row">
                        <switch id="switch1" on="false"/>
                        <label>Toggle</label>
                    </div>
                    <div class="widget-row">
                        <slider id="slider1" value="0.5" min="0" max="1"/>
                    </div>
                    <div class="widget-row">
                        <progressbar id="progress1" value="0.65"/>
                        <label>65%</label>
                    </div>
                    <div class="widget-row">
                        <rating id="rating1" value="3"/>
                        <label>Rate it</label>
                    </div>
                    <div class="widget-row">
                        <searchbox id="search1" placeholder="Search..."/>
                    </div>
                </div>

                <!-- Status Bar moved here -->
                <div class="section-wide">
                    <label id="status">✅ All 28+ widgets loaded - Click tabs to explore!</label>
                </div>
            </div>

            <!-- Tab Content 2: Feedback -->
            <div id="tab-content-1" class="tab-content">
                <div class="section">
                    <label class="section-title">💬 Feedback & Status</label>
                    <div class="widget-row">
                        <spinner id="spinner1"/>
                        <badge id="badge1">NEW</badge>
                        <badge id="badge2">5</badge>
                        <avatar id="avatar1">JD</avatar>
                        <chip id="chip1">JS</chip>
                        <chip id="chip2">C++</chip>
                    </div>
                    <alert id="alert1" type="info">Info message</alert>
                    <alert id="alert2" type="success">Success!</alert>
                </div>

                <div class="section-wide">
                    <label id="status-tab1">💬 Feedback widgets - Spinners, badges, avatars, chips, and alerts</label>
                </div>
            </div>

            <!-- Tab Content 3: Navigation -->
            <div id="tab-content-2" class="tab-content">
                <div class="section">
                    <label class="section-title">🧭 Navigation & Selection</label>
                    <div class="widget-row">
                        <dropdown id="dropdown1" items="Option 1, Option 2, Option 3"/>
                        <label>Dropdown</label>
                    </div>
                    <div class="widget-row">
                        <breadcrumb id="breadcrumb1" items="Home, Docs, Guide"/>
                    </div>
                    <div class="widget-row">
                        <pagination id="pagination1" total="10" current="3"/>
                    </div>
                </div>

                <div class="section-wide">
                    <label id="status-tab2">🧭 Navigation widgets - Dropdowns, breadcrumbs, and pagination</label>
                </div>
            </div>
        </div>

        <!-- Hidden complex widgets (shown on demand) -->
        <calendar id="calendar1" year="2025" month="1" style="display:none"/>
        <colorpicker id="colorpicker1" style="display:none"/>
        <modal id="modal1" title="Sample Modal" style="display:none">This is a modal dialog with some content.</modal>
        <menu id="menu1" style="display:none"/>
        <tablist id="tablist1" tabs="Tab A, Tab B, Tab C" style="display:none"/>
        <toast id="toast1" type="info" style="display:none">This is a toast notification</toast>
        <snackbar id="snackbar1" style="display:none">Action completed</snackbar>
        <table id="table1" style="display:none"/>
    )");


    // Register button handlers
    screen.registerHandler("handleButton1", [&](flexui::Widget* w) {
        auto* status = dynamic_cast<flexui::Label*>(screen.findWidget("status"));
        if (status) status->setText("Button clicked!");
        std::cout << "Button clicked!" << std::endl;
        return true;
    });

    // Setup interactive widget callbacks
    auto* switch1 = dynamic_cast<flexui::Switch*>(screen.findWidget("switch1"));
    if (switch1) {
        switch1->setChangeCallback([&](bool on) {
            auto* status = dynamic_cast<flexui::Label*>(screen.findWidget("status"));
            if (status) status->setText(on ? "Switch: ON" : "Switch: OFF");
            std::cout << "Switch toggled: " << (on ? "ON" : "OFF") << std::endl;
        });
    }

    auto* slider1 = dynamic_cast<flexui::Slider*>(screen.findWidget("slider1"));
    if (slider1) {
        slider1->setValueCallback([&](float value) {
            auto* status = dynamic_cast<flexui::Label*>(screen.findWidget("status"));
            if (status) {
                char buf[64];
                snprintf(buf, sizeof(buf), "Slider value: %.2f", value);
                status->setText(buf);
            }
        });
    }

    auto* rating1 = dynamic_cast<flexui::Rating*>(screen.findWidget("rating1"));
    if (rating1) {
        rating1->setChangeCallback([&](int value) {
            auto* status = dynamic_cast<flexui::Label*>(screen.findWidget("status"));
            if (status) {
                char buf[64];
                snprintf(buf, sizeof(buf), "Rating: %d/5 stars", value);
                status->setText(buf);
            }
            std::cout << "Rating changed to: " << value << std::endl;
        });
    }

    auto* search1 = dynamic_cast<flexui::SearchBox*>(screen.findWidget("search1"));
    if (search1) {
        search1->setSearchCallback([&](const std::string& query) {
            auto* status = dynamic_cast<flexui::Label*>(screen.findWidget("status"));
            if (status) status->setText("Searching for: " + query);
            std::cout << "Search: " << query << std::endl;
        });
    }

    // Main TabBar callback to switch content
    auto* mainTabs = dynamic_cast<flexui::TabBar*>(screen.findWidget("main-tabs"));
    if (mainTabs) {
        mainTabs->setTabChangeCallback([&](int index, const std::string& name) {
            auto* status = dynamic_cast<flexui::Label*>(screen.findWidget("status"));
            if (status) status->setText("Tab: " + name);
            std::cout << "Tab changed: " << name << std::endl;

            // Hide all tab content panels
            for (int i = 0; i < 3; i++) {
                auto* content = dynamic_cast<flexui::Widget*>(
                    screen.findWidget("tab-content-" + std::to_string(i)));
                if (content) {
                    content->setInlineStyle("display", "none");
                }
            }

            // Show selected tab content
            auto* activeContent = dynamic_cast<flexui::Widget*>(
                screen.findWidget("tab-content-" + std::to_string(index)));
            if (activeContent) {
                activeContent->setInlineStyle("display", "flex");
            }
        });
    }

    auto* dropdown1 = dynamic_cast<flexui::Dropdown*>(screen.findWidget("dropdown1"));
    if (dropdown1) {
        dropdown1->setChangeCallback([&](int index, const std::string& item) {
            auto* status = dynamic_cast<flexui::Label*>(screen.findWidget("status"));
            if (status) status->setText("Selected: " + item);
            std::cout << "Dropdown selection: " << item << std::endl;
        });
    }

    auto* pagination1 = dynamic_cast<flexui::Pagination*>(screen.findWidget("pagination1"));
    if (pagination1) {
        pagination1->setPageChangeCallback([&](int page) {
            auto* status = dynamic_cast<flexui::Label*>(screen.findWidget("status"));
            if (status) {
                char buf[64];
                snprintf(buf, sizeof(buf), "Page: %d of 10", page);
                status->setText(buf);
            }
            std::cout << "Page changed to: " << page << std::endl;
        });
    }

    std::cout << "=== FlexUI All Widgets Showcase ===" << std::endl;
    std::cout << "Showcasing all " << (11 + 17) << " refactored widgets" << std::endl;
    std::cout << "Interact with the widgets to see callbacks in action!" << std::endl;

    // Main loop
    while (screen.pollEvents()) {
        screen.draw();
    }

    return 0;
}
