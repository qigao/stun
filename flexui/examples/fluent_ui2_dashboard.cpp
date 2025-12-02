#include <flexui/screen.h>
#include <flexui/tabbar.h>

int main() {
    flexui::Screen screen(1200, 800, "Fluent UI 2 Dashboard");

    // Load all CSS with Fluent UI 2 design tokens
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
            padding: 32px;
            gap: 24px;
            overflow-y: auto;
        }

        .stats-row {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr;
            gap: 20px;
            width: 100%;
            height: 140px;
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
        }

        .stat-card:hover {
            transform: translateY(-2px);
            background: var(--stat-bg-hover);
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
            height: 160px;
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
        }

        .action-card:hover {
            background: var(--action-bg-hover);
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
            background: var(--table-bg);
            border-radius: 8px;
            border-width: 2px;
            border-style: solid;
            border-color: var(--table-border);
            padding: 20px;
            gap: 16px;
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
        }

        .form-input:focus {
            border-color: var(--form-input-focus-border);
            border-width: 3px;
            background: var(--form-input-focus-bg);
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
                    <div class="table-container">
                        <div class="table-row">
                            <label class="table-cell table-header">User</label>
                            <label class="table-cell table-header">Action</label>
                            <label class="table-cell table-header">Time</label>
                        </div>
                        <div class="table-row">
                            <label class="table-cell">john@example.com</label>
                            <label class="table-cell">Created report</label>
                            <label class="table-cell">2 min ago</label>
                        </div>
                        <div class="table-row">
                            <label class="table-cell">sarah@example.com</label>
                            <label class="table-cell">Updated settings</label>
                            <label class="table-cell">15 min ago</label>
                        </div>
                        <div class="table-row">
                            <label class="table-cell">mike@example.com</label>
                            <label class="table-cell">Exported data</label>
                            <label class="table-cell">1 hour ago</label>
                        </div>
                    </div>
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

    while (screen.pollEvents()) {
        screen.draw();
    }

    return 0;
}
