// Widget Gallery - Responsive Grid + Tailwind-style utilities
#include <flexui/screen.h>
#include <flexui/scrollview.h>
#include <flexui/modal.h>
#include <flexui/toast.h>
#include <flexui/tabbar.h>
#include <flexui/label.h>
#include <iostream>

int main() {
    flexui::Screen screen(1200, 800, "FlexUI Widget Gallery");

    screen.loadCSS(R"(
        /* ================================================================
           Tailwind-style Design Tokens
           ================================================================ */
        :root {
            /* Colors */
            --blue-500: #3b82f6;
            --blue-600: #2563eb;
            --green-500: #22c55e;
            --green-600: #16a34a;
            --amber-500: #f59e0b;
            --red-500: #ef4444;
            --gray-50: #f9fafb;
            --gray-100: #f3f4f6;
            --gray-200: #e5e7eb;
            --gray-300: #d1d5db;
            --gray-500: #6b7280;
            --gray-700: #374151;
            --gray-900: #111827;
            --white: #ffffff;

            /* Spacing */
            --space-1: 4px;
            --space-2: 8px;
            --space-3: 12px;
            --space-4: 16px;
            --space-5: 20px;
            --space-6: 24px;

            /* Border Radius */
            --rounded-sm: 4px;
            --rounded: 6px;
            --rounded-lg: 8px;
            --rounded-xl: 12px;
            --rounded-full: 9999px;

            /* Shadows */
            --shadow-sm: 0 1px 2px rgba(0,0,0,0.05);
            --shadow: 0 1px 3px rgba(0,0,0,0.1);
            --shadow-md: 0 4px 6px rgba(0,0,0,0.1);
        }

        /* ================================================================
           Responsive Grid Gallery Container
           ================================================================ */
        .root-scroll {
            width: 1200px;
            height: 800px;
        }

        .gallery {
            display: grid;
            grid-template-columns: 280px 280px 280px 280px;
            grid-auto-rows: 220px;
            gap: 20px;
            padding: 24px;
            background: var(--gray-50);
            /* CSS3: calc() for width based on columns + gaps + padding */
            width: calc(280px * 4 + 20px * 3 + 48px);
            height: auto;
        }

        /* 3 columns for medium screens */
        @media (max-width: 1200px) {
            .gallery {
                grid-template-columns: 280px 280px 280px;
                /* CSS3: calc() for 3 columns */
                width: calc(280px * 3 + 20px * 2 + 48px);
            }
        }

        /* 2 columns for small screens */
        @media (max-width: 900px) {
            .gallery {
                grid-template-columns: 280px 280px;
                /* CSS3: calc() for 2 columns */
                width: calc(280px * 2 + 20px + 48px);
            }
        }

        /* 1 column for mobile */
        @media (max-width: 600px) {
            .gallery {
                grid-template-columns: 280px;
                /* CSS3: calc() for 1 column */
                width: calc(280px + 48px);
            }
        }

        /* ================================================================
           Card Component
           ================================================================ */
        .card {
            display: flex;
            flex-direction: column;
            width: 280px;
            height: 200px;
            background: var(--white);
            border-radius: var(--rounded-lg);
            padding: 16px;
            box-shadow: var(--shadow);
            gap: 12px;
            /* CSS3: Smooth shadow transition */
            transition: box-shadow 0.2s ease-out;
        }

        .card:hover {
            box-shadow: var(--shadow-md);
        }

        .card-header {
            width: 248px;
            height: 20px;
            font-size: 14px;
            font-weight: 600;
            color: var(--gray-700);
        }

        .card-body {
            display: flex;
            flex-direction: column;
            width: 248px;
            height: 160px;
            gap: 8px;
        }

        /* ================================================================
           Layout Classes (Complete Definitions)
           ================================================================ */
        .row {
            display: flex;
            flex-direction: row;
            width: 248px;
            height: 32px;
            gap: 8px;
            align-items: center;
        }

        .col {
            display: flex;
            flex-direction: column;
            width: 180px;
            height: 36px;
            gap: 2px;
        }

        .row-wrap {
            display: flex;
            flex-direction: row;
            flex-wrap: wrap;
            width: 248px;
            height: 24px;
            gap: 8px;
            align-items: center;
        }

        /* ================================================================
           Button Variants
           ================================================================ */
        .btn {
            display: flex;
            align-items: center;
            justify-content: center;
            padding: var(--space-2) var(--space-3);
            border-radius: var(--rounded);
            font-size: 13px;
            font-weight: 500;
            width: 70px;
            height: 32px;
            cursor: pointer;
            /* CSS3: Smooth color transition */
            transition: background 0.15s ease-out, border-color 0.15s ease-out;
        }

        .btn-primary {
            background: var(--blue-500);
            color: var(--white);
        }
        .btn-primary:hover { background: var(--blue-600); }

        .btn-success {
            background: var(--green-500);
            color: var(--white);
        }
        .btn-success:hover { background: var(--green-600); }

        .btn-outline {
            background: var(--white);
            color: var(--gray-700);
            border: 1px solid var(--gray-200);
        }
        .btn-outline:hover { background: var(--gray-100); }

        .btn-sm {
            height: 32px;
            padding: var(--space-1) var(--space-3);
            font-size: 13px;
        }

        /* ================================================================
           Form Controls
           ================================================================ */
        .input {
            padding: var(--space-1) var(--space-3);
            border: 1px solid var(--gray-200);
            border-radius: var(--rounded);
            height: 32px;
            width: 240px;
            background: var(--white);
            /* CSS3: Smooth border transition */
            transition: border-color 0.15s ease-out;
        }
        .input:focus {
            border-color: var(--blue-500);
            border-width: 2px;
        }
        .input:hover {
            border-color: var(--gray-300);
        }

        .form-label {
            font-size: 13px;
            color: var(--gray-500);
            height: 18px;
        }

        /* ================================================================
           Badge & Chip
           ================================================================ */
        .badge {
            padding: 2px var(--space-2);
            border-radius: var(--rounded-full);
            font-size: 12px;
            font-weight: 500;
            color: var(--white);
        }
        .badge-primary { background: var(--blue-500); }
        .badge-success { background: var(--green-500); }
        .badge-warning { background: var(--amber-500); }

        .chip {
            padding: var(--space-1) var(--space-3);
            background: var(--gray-100);
            border-radius: var(--rounded-full);
            font-size: 13px;
            color: var(--gray-700);
        }

        /* ================================================================
           Avatar
           ================================================================ */
        .avatar {
            width: 40px;
            height: 40px;
            border-radius: var(--rounded-full);
            background: var(--blue-500);
            color: var(--white);
            font-size: 14px;
            font-weight: 600;
        }

        .avatar-sm { width: 32px; height: 32px; font-size: 12px; }
        .avatar-lg { width: 48px; height: 48px; font-size: 16px; }

        /* ================================================================
           Progress & Slider
           ================================================================ */
        .progress {
            width: 240px;
            height: 12px;
            border-radius: var(--rounded-full);
            background: var(--gray-200);
        }

        .slider {
            width: 240px;
            height: 24px;
        }

        /* ================================================================
           Alerts
           ================================================================ */
        .alert {
            padding: var(--space-2) var(--space-3);
            border-radius: var(--rounded);
            font-size: 12px;
            border-left: 4px solid;
            width: 240px;
            height: 32px;
        }
        .alert-info {
            background: #dbeafe;
            border-color: var(--blue-500);
            color: var(--blue-600);
        }
        .alert-success {
            background: #dcfce7;
            border-color: var(--green-500);
            color: var(--green-600);
        }
        .alert-warning {
            background: #fef3c7;
            border-color: var(--amber-500);
            color: #b45309;
        }

        /* ================================================================
           Components
           ================================================================ */
        .spinner { width: 24px; height: 24px; }
        .calendar { width: 240px; height: 160px; }
        .colorpicker { width: 120px; height: 140px; }
        .rating { font-size: 20px; width: 120px; height: 24px; }
        .breadcrumb { font-size: 13px; width: 240px; height: 20px; }
        .pagination { font-size: 13px; width: 240px; height: 36px; }
        .dropdown { width: 240px; height: 36px; }
        .tabbar { width: 240px; height: 36px; }

        /* Radio and Checkbox */
        radio {
            width: 18px;
            height: 18px;
            border: 2px solid var(--gray-300);
            border-radius: var(--rounded-full);
            background: var(--white);
            /* CSS3: Smooth border transition */
            transition: border-color 0.15s ease-out;
        }
        radio.checked {
            border-color: var(--blue-500);
        }

        checkbox {
            width: 18px;
            height: 18px;
            border: 2px solid var(--gray-300);
            border-radius: var(--rounded-sm);
            background: var(--white);
            /* CSS3: Smooth color transition */
            transition: border-color 0.15s ease-out, background 0.15s ease-out;
        }
        checkbox.checked {
            border-color: var(--blue-500);
            background: var(--blue-500);
        }

        .tab-content {
            width: 240px;
            height: 50px;
            padding: 8px;
            background: var(--gray-100);
            border-radius: var(--rounded);
            font-size: 12px;
            color: var(--gray-700);
        }

        /* ================================================================
           Demo Card
           ================================================================ */
        .demo-card {
            background: var(--gray-50);
            border: 1px solid var(--gray-200);
            border-radius: var(--rounded);
            padding: var(--space-3);
        }

        /* ================================================================
           Typography
           ================================================================ */
        .text-sm { font-size: 12px; height: 16px; }
        .text-base { font-size: 14px; height: 18px; }
        .text-lg { font-size: 16px; height: 20px; }
        .text-bold { font-weight: 600; }
        .text-gray { color: var(--gray-500); }
        .text-dark { color: var(--gray-900); }

        /* ================================================================
           Image
           ================================================================ */
        .img-thumb {
            width: 240px;
            height: 140px;
            border-radius: var(--rounded);
        }
    )");

    screen.loadXML(R"(
        <scrollview id="root-scroll" class="root-scroll">
            <div class="gallery">
            <!-- Buttons -->
            <div class="card">
                <label class="card-header">Buttons</label>
                <div class="card-body">
                    <div class="row">
                        <button class="btn btn-primary" onclick="onPrimaryClick">Primary</button>
                        <button class="btn btn-success">Success</button>
                    </div>
                    <div class="row">
                        <button class="btn btn-outline">Outline</button>
                        <iconbutton icon="settings" class="btn btn-outline"/>
                    </div>
                    <div class="row">
                        <switch id="switch1"/>
                        <label class="text-sm text-gray">Dark Mode</label>
                    </div>
                </div>
            </div>

            <!-- Data Display -->
            <div class="card">
                <label class="card-header">Data Display</label>
                <div class="card-body">
                    <div class="row-wrap">
                        <badge class="badge badge-primary">New</badge>
                        <badge class="badge badge-success">Active</badge>
                        <chip class="chip">Tag</chip>
                    </div>
                    <div class="row">
                        <avatar class="avatar">JD</avatar>
                        <div class="col">
                            <label class="text-base text-bold">John Doe</label>
                            <label class="text-sm text-gray">Developer</label>
                        </div>
                    </div>
                    <progressbar class="progress" value="0.75"/>
                </div>
            </div>

            <!-- Selection -->
            <div class="card">
                <label class="card-header">Selection</label>
                <div class="card-body">
                    <tabbar id="demo-tabs" class="tabbar" tabs="Tab 1,Tab 2,Tab 3"/>
                    <dropdown class="dropdown" items="Option 1,Option 2,Option 3"/>
                    <label id="tab-content" class="tab-content">Tab 1 content</label>
                </div>
            </div>

            <!-- Cards -->
            <div class="card">
                <label class="card-header">Cards</label>
                <div class="card-body">
                    <card class="demo-card">
                        <div class="col">
                            <label class="text-base text-bold">Card Title</label>
                            <label class="text-sm text-gray">Card content goes here.</label>
                            <button class="btn btn-primary btn-sm">Action</button>
                        </div>
                    </card>
                </div>
            </div>

            <!-- Inputs -->
            <div class="card">
                <label class="card-header">Inputs</label>
                <div class="card-body">
                    <input class="input" placeholder="Enter text..."/>
                    <searchbox class="input" placeholder="Search..."/>
                    <div class="row">
                        <checkbox id="cb1" checked="true"/>
                        <label class="text-sm">Remember me</label>
                    </div>
                    <div class="row">
                        <radio name="option" value="a" checked="true"/>
                        <label class="text-sm">A</label>
                        <radio name="option" value="b"/>
                        <label class="text-sm">B</label>
                    </div>
                </div>
            </div>

            <!-- Feedback -->
            <div class="card">
                <label class="card-header">Feedback</label>
                <div class="card-body">
                    <alert type="info" class="alert alert-info">Info message</alert>
                    <alert type="success" class="alert alert-success">Success!</alert>
                    <div class="row">
                        <spinner class="spinner"/>
                        <label class="text-sm text-gray">Loading...</label>
                    </div>
                </div>
            </div>

            <!-- Calendar -->
            <div class="card">
                <label class="card-header">Calendar</label>
                <div class="card-body">
                    <calendar class="calendar"/>
                </div>
            </div>

            <!-- Modal & Toast -->
            <div class="card">
                <label class="card-header">Modal & Toast</label>
                <div class="card-body">
                    <button class="btn btn-outline" onclick="showModal">Show Modal</button>
                    <button class="btn btn-outline" onclick="showToast">Show Toast</button>
                </div>
            </div>

            <!-- Slider & Color -->
            <div class="card">
                <label class="card-header">Slider & Color</label>
                <div class="card-body">
                    <slider class="slider" value="0.6" min="0" max="1"/>
                    <colorpicker class="colorpicker"/>
                </div>
            </div>

            <!-- Navigation -->
            <div class="card">
                <label class="card-header">Navigation</label>
                <div class="card-body">
                    <breadcrumb items="Home,Products,Details" class="breadcrumb"/>
                    <pagination total="5" current="2" class="pagination"/>
                </div>
            </div>

            <!-- Image -->
            <div class="card">
                <label class="card-header">Image</label>
                <div class="card-body">
                    <img src="test1.jpg" class="img-thumb"/>
                </div>
            </div>
        </div>
        </scrollview>
    )");

    screen.registerHandler("onPrimaryClick", [](flexui::Widget* w) {
        std::cout << "Primary button clicked!" << std::endl;
        return true;
    });

    // Create modal widget
    auto* modal = screen.createWidget<flexui::Modal>(
        "demo-modal",
        "Demo Modal",
        "This is a modal dialog.\nClick outside to close."
    );
    modal->setPosition(400, 250);
    modal->setSize(400, 200);

    // Create toast widget
    auto* toast = screen.createWidget<flexui::Toast>(
        "demo-toast",
        "This is a toast notification!",
        flexui::ToastType::Success
    );
    toast->setPosition(450, 700);
    toast->setSize(300, 48);

    screen.registerHandler("showModal", [modal](flexui::Widget* w) {
        modal->show();
        return true;
    });

    screen.registerHandler("showToast", [toast](flexui::Widget* w) {
        toast->show(3.0f);
        return true;
    });

    // Setup tab change callback
    if (auto* tabs = dynamic_cast<flexui::TabBar*>(screen.findWidget("demo-tabs"))) {
        if (auto* content = dynamic_cast<flexui::Label*>(screen.findWidget("tab-content"))) {
            tabs->setTabChangeCallback([content](int index, const std::string& name) {
                std::string texts[] = {"Tab 1: Home content", "Tab 2: Profile content", "Tab 3: Settings content"};
                content->setLabelText(texts[index]);
            });
        }
    }

    // Setup root scroll for window-level scrolling
    flexui::ScrollView* rootScrollView = nullptr;
    if (auto* rootScroll = screen.findWidget("root-scroll")) {
        rootScrollView = dynamic_cast<flexui::ScrollView*>(rootScroll);
    }

    // Helper to calculate content height based on window width
    auto calculateContentHeight = [](int windowWidth) -> float {
        // 11 cards, row height 220px, gap 20px, padding 48px
        int cols = 4;
        if (windowWidth <= 600) cols = 1;
        else if (windowWidth <= 900) cols = 2;
        else if (windowWidth <= 1200) cols = 3;

        int rows = (11 + cols - 1) / cols;  // ceil(11 / cols)
        return rows * 220.0f + (rows - 1) * 20.0f + 48.0f;
    };

    int lastWidth = 0, lastHeight = 0;

    while (screen.pollEvents()) {
        toast->updateTimer();

        // Update scrollview size when window resizes (ImGui-style)
        int w = screen.getWidth();
        int h = screen.getHeight();
        if (rootScrollView && (w != lastWidth || h != lastHeight)) {
            rootScrollView->setViewportSize((float)w, (float)h);
            rootScrollView->setContentHeight(calculateContentHeight(w));
            lastWidth = w;
            lastHeight = h;
        }

        screen.draw();
    }

    return 0;
}
