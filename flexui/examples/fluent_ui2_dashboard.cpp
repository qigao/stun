#include <flexui/screen.h>
#include <flexui/tabbar.h>
#include <flexui/table.h>

int main() {
    flexui::Screen screen(1200, 800, "Fluent UI 2 Dashboard");

    // Load all CSS with Fluent UI 2 design tokens + CSS3 features
    std::string css = R"(
        :root {
            --brand: #0078d4;
            --brand-hover: #106ebe;
            --bg-card: #ffffff;
            --bg-hover: #f5f5f5;
            --text-primary: #323130;
            --text-secondary: #605e5c;
            --stroke: #e1dfdd;
            --success: #107c10;
            --error: #d13438;

            /* Stat card colors (blue theme) */
            --stat-bg: #e3f2fd;
            --stat-bg-hover: #bbdefb;
            --stat-border: #1976d2;
            --stat-label: #1565c0;
            --stat-value: #0d47a1;

            /* Action card colors (orange theme) */
            --action-bg: #fff3e0;
            --action-bg-hover: #ffe0b2;
            --action-border: #ff9800;
            --action-title: #e65100;
            --action-desc: #bf360c;

            /* Table colors (green theme) */
            --table-bg: #e8f5e9;
            --table-border: #4caf50;
            --table-row-bg: #c8e6c9;
            --table-row-border: #81c784;
            --table-text: #1b5e20;
            --table-header-bg: #a5d6a7;
            --table-header-text: #2e7d32;

            /* Form colors (purple theme) */
            --form-label: #4a148c;
            --form-input-bg: #f3e5f5;
            --form-input-border: #9c27b0;
            --form-input-focus-border: #7b1fa2;
            --form-input-focus-bg: #e1bee7;

            /* Status colors */
            --positive-text: #2e7d32;
            --positive-bg: #e8f5e9;
            --negative-text: #c62828;
            --negative-bg: #ffebee;

            /* Responsive spacing */
            --sidebar-width: 200px;
            --content-padding: 32px;
            --card-gap: 20px;
        }

        /* @keyframes for loading states (use .skeleton or .loading class) */
        @keyframes pulse {
            0% { opacity: 1; }
            50% { opacity: 0.6; }
            100% { opacity: 1; }
        }

        @keyframes shimmer {
            0% { background-position: -200% 0; }
            100% { background-position: 200% 0; }
        }

        #root {
            display: flex;
            flex-direction: column;
            width: 1200px;
            height: 800px;
            background: #fafafa;
        }

        #header {
            display: flex;
            flex-direction: row;
            width: 100%;
            height: 56px;
            background: #ffffff;
            border-bottom-width: 1px;
            border-bottom-style: solid;
            border-bottom-color: #e0e0e0;
            align-items: center;
            padding: 0 24px;
        }

        #logo {
            width: 160px;
            height: 56px;
            font-size: 16px;
            font-weight: 600;
            color: var(--brand);
            padding: 16px 0;
        }

        #nav-tabs {
            width: 400px;
            height: 48px;
            background: #ffffff;
            color: #605e5c;
            font-size: 14px;
        }

        #user-section {
            display: flex;
            flex-direction: row;
            width: 140px;
            height: 36px;
            gap: 10px;
            align-items: center;
            padding: 6px 12px;
            border-radius: 18px;
            /* CSS3: Smooth background transition */
            transition: background 0.2s ease-out;
        }

        #user-section:hover {
            background: var(--bg-hover);
        }

        .user-avatar {
            width: 28px;
            height: 28px;
            border-radius: 14px;
            background: #0078d4;
            color: white;
            font-size: 12px;
            font-weight: 600;
            text-align: center;
            padding: 6px 0;
        }

        .user-name {
            flex-grow: 1;
            height: 20px;
            font-size: 13px;
            color: #323130;
        }

        #content {
            display: flex;
            flex-direction: column;
            flex-grow: 1;
            padding: var(--content-padding);
            gap: 24px;
            overflow-y: auto;
            /* CSS3: calc() for responsive height minus header */
            height: calc(100% - 56px);
        }

        .stats-row {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr;
            gap: var(--card-gap);
            width: 100%;
            /* CSS3: calc() for responsive height */
            height: calc(140px + 8px);
        }

        .stat-card {
            display: flex;
            flex-direction: column;
            padding: 16px;
            height: 140px;
            background: var(--stat-bg);
            border-radius: 8px;
            border-width: 2px;
            border-style: solid;
            border-color: var(--stat-border);
            gap: 8px;
            /* CSS3: Smooth color transitions */
            transition: background 0.2s ease-out, border-color 0.2s ease-out;
        }

        .stat-card:hover {
            background: var(--stat-bg-hover);
            border-color: var(--stat-value);
        }

        .stat-label {
            width: 100%;
            height: 20px;
            font-size: 13px;
            color: var(--stat-label);
            background: transparent;
        }

        .stat-value {
            width: 100%;
            height: 36px;
            font-size: 28px;
            font-weight: 600;
            color: var(--stat-value);
            background: transparent;
        }

        .stat-change {
            width: 100%;
            height: 18px;
            font-size: 12px;
            background: transparent;
        }

        .change-positive { color: var(--positive-text); background: var(--positive-bg); height: 18px; }
        .change-negative { color: var(--negative-text); background: var(--negative-bg); height: 18px; }

        .section-title {
            width: 100%;
            height: 24px;
            font-size: 16px;
            font-weight: 600;
            color: var(--text-primary);
        }

        .action-grid {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr;
            gap: 16px;
            width: 100%;
            /* CSS3: calc() for action card container height */
            height: calc(160px + 16px);
        }

        .action-card {
            display: flex;
            flex-direction: column;
            padding: 16px;
            height: 160px;
            background: var(--action-bg);
            border-radius: 8px;
            border-width: 2px;
            border-style: solid;
            border-color: var(--action-border);
            gap: 12px;
            /* CSS3: Smooth color transitions */
            transition: background 0.2s ease-out, border-color 0.2s ease-out;
        }

        .action-card:hover {
            background: var(--action-bg-hover);
            border-color: var(--action-title);
        }

        .action-title {
            width: 100%;
            height: 20px;
            font-size: 15px;
            font-weight: 600;
            color: var(--action-title);
            background: transparent;
        }

        .action-desc {
            width: 100%;
            height: 20px;
            font-size: 13px;
            color: var(--action-desc);
            background: transparent;
        }

        .btn {
            width: 100%;
            height: 36px;
            border-radius: 4px;
            font-size: 14px;
            font-weight: 600;
            /* CSS3: Smooth color transitions */
            transition: background 0.15s ease-out, border-color 0.15s ease-out;
        }

        .btn-primary {
            background: var(--brand);
            color: white;
        }

        .btn-primary:hover {
            background: var(--brand-hover);
        }

        .btn-outline {
            background: transparent;
            color: var(--brand);
            border-width: 1px;
            border-style: solid;
            border-color: var(--stroke);
        }

        .btn-outline:hover {
            background: var(--bg-hover);
            border-color: var(--brand);
        }

        .table-container {
            display: flex;
            flex-direction: column;
            width: 100%;
            height: 250px;
            background: var(--table-bg);
            border-radius: 8px;
            border-width: 2px;
            border-style: solid;
            border-color: var(--table-border);
            padding: 20px;
            gap: 16px;
            /* CSS3: Smooth border transition */
            transition: border-color 0.2s ease-out;
        }

        .table-container:hover {
            border-color: var(--table-header-text);
        }

        .table-row {
            display: flex;
            flex-direction: row;
            width: 100%;
            height: 48px;
            padding: 0 12px;
            border-bottom-width: 1px;
            border-bottom-style: solid;
            border-bottom-color: var(--table-row-border);
            align-items: center;
            background: var(--table-row-bg);
            /* CSS3: Smooth background transition */
            transition: background 0.15s ease-out;
        }

        .table-row:hover {
            background: var(--table-header-bg);
        }

        .table-cell {
            flex-grow: 1;
            height: 20px;
            font-size: 14px;
            color: var(--table-text);
            background: transparent;
        }

        .table-header {
            font-weight: 600;
            color: var(--table-header-text);
            background: var(--table-header-bg);
        }

        .form-group {
            display: flex;
            flex-direction: column;
            gap: 8px;
            width: 100%;
            background: transparent;
        }

        .form-label {
            width: 100%;
            height: 20px;
            font-size: 14px;
            font-weight: 600;
            color: var(--form-label);
            background: transparent;
        }

        .form-input {
            width: 100%;
            height: 40px;
            padding: 8px 12px;
            background: var(--form-input-bg);
            border-width: 2px;
            border-style: solid;
            border-color: var(--form-input-border);
            border-radius: 4px;
            font-size: 14px;
            color: var(--form-label);
            /* CSS3: Smooth border/background transitions */
            transition: border-color 0.2s ease-out, background 0.2s ease-out;
        }

        .form-input:focus {
            border-color: var(--form-input-focus-border);
            border-width: 3px;
            background: var(--form-input-focus-bg);
        }

        .form-input:hover {
            border-color: var(--form-input-focus-border);
        }

        /* CSS3: Loading skeleton animation - use .skeleton class when loading */
        .skeleton {
            background: linear-gradient(90deg, #f0f0f0 25%, #e0e0e0 50%, #f0f0f0 75%);
            background-size: 200% 100%;
            animation: shimmer 1.5s infinite;
            border-radius: 4px;
        }

        /* CSS3: Pulse animation for loading indicators - use .loading class */
        .loading {
            animation: pulse 1.2s ease-in-out infinite;
        }

        #page-overview, #page-analytics, #page-settings {
            display: flex;
            flex-direction: column;
            width: 100%;
            gap: 24px;
        }

        /* Must be AFTER and MORE SPECIFIC than ID selectors to override display */
        #page-overview.page-hidden,
        #page-analytics.page-hidden,
        #page-settings.page-hidden {
            display: none;
        }
    )";
    
    screen.loadCSS(css);

    // XML Structure
    screen.loadXML(R"(
        <div id="root">
            <div id="header">
                <label id="logo">FlexUI Dashboard</label>
                <tabbar id="nav-tabs" tabs="Overview,Analytics,Settings"/>
                <div id="user-section">
                    <label class="user-avatar">JD</label>
                    <label class="user-name">John Doe</label>
                </div>
            </div>
            <div id="content">
                <div id="page-overview">
                    <div class="stats-row">
                        <div class="stat-card">
                            <label class="stat-label">Total Users</label>
                            <label class="stat-value">24,583</label>
                            <label class="stat-change change-positive">+12.5%</label>
                        </div>
                        <div class="stat-card">
                            <label class="stat-label">Revenue</label>
                            <label class="stat-value">$48.2K</label>
                            <label class="stat-change change-positive">+8.3%</label>
                        </div>
                        <div class="stat-card">
                            <label class="stat-label">Active Sessions</label>
                            <label class="stat-value">1,429</label>
                            <label class="stat-change change-negative">-3.2%</label>
                        </div>
                    </div>
                    <label class="section-title">Quick Actions</label>
                    <div class="action-grid">
                        <div class="action-card">
                            <label class="action-title">Create Report</label>
                            <label class="action-desc">Generate analytics report</label>
                            <button class="btn btn-primary">Create</button>
                        </div>
                        <div class="action-card">
                            <label class="action-title">Export Data</label>
                            <label class="action-desc">Download as CSV</label>
                            <button class="btn btn-outline">Export</button>
                        </div>
                        <div class="action-card">
                            <label class="action-title">Invite Users</label>
                            <label class="action-desc">Add team members</label>
                            <button class="btn btn-outline">Invite</button>
                        </div>
                    </div>
                </div>
                <div id="page-analytics" class="page-hidden">
                    <label class="section-title">Recent Activity</label>
                    <div id="activity-table-container" class="table-container"></div>
                </div>
                <div id="page-settings" class="page-hidden">
                    <label class="section-title">Account Settings</label>
                    <div class="table-container">
                        <div class="form-group">
                            <label class="form-label">Display Name</label>
                            <input class="form-input" placeholder="Enter your name"/>
                        </div>
                        <div class="form-group">
                            <label class="form-label">Email Address</label>
                            <input class="form-input" placeholder="name@example.com"/>
                        </div>
                        <div class="form-group">
                            <label class="form-label">Company</label>
                            <input class="form-input" placeholder="Your company"/>
                        </div>
                        <button class="btn btn-primary">Save Changes</button>
                    </div>
                    <label class="section-title">Preferences</label>
                    <div class="table-container">
                        <div class="form-group">
                            <label class="form-label">Email notifications</label>
                            <checkbox id="check1" checked="true"/>
                        </div>
                        <div class="form-group">
                            <label class="form-label">Activity status</label>
                            <checkbox id="check2" checked="false"/>
                        </div>
                    </div>
                </div>
            </div>
        </div>
    )");                            

    auto* navTabs = dynamic_cast<flexui::TabBar*>(screen.findWidget("nav-tabs"));
    auto* pageOverview = screen.findWidget("page-overview");
    auto* pageAnalytics = screen.findWidget("page-analytics");
    auto* pageSettings = screen.findWidget("page-settings");

    if (navTabs && pageOverview && pageAnalytics && pageSettings) {
        // TabBar now manages page visibility automatically
        navTabs->registerPages({pageOverview, pageAnalytics, pageSettings});
    }

    // Create activity table programmatically
    auto* tableContainer = screen.findWidget("activity-table-container");
    if (tableContainer) {
        std::vector<std::string> headers = {"User", "Action", "Time"};
        std::vector<float> widths = {300.0f, 250.0f, 150.0f};
        
        auto* table = screen.createWidget<flexui::Table>(
            "activity-table",
            headers,
            widths
        );
        
        table->setInlineStyle("height", "200px");
        
        table->addRow({"john@example.com", "Created report", "2 min ago"});
        table->addRow({"sarah@example.com", "Updated settings", "15 min ago"});
        table->addRow({"mike@example.com", "Exported data", "1 hour ago"});
        table->addRow({"alice@example.com", "Logged in", "2 hours ago"});
        
        table->setSelectionCallback([](int row, int col) {
            printf("Selected row %d, column %d\n", row, col);
        });
        
        tableContainer->addChild(table);
    }

    while (screen.pollEvents()) {
        screen.draw();
    }

    return 0;
}
