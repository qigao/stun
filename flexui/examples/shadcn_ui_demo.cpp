#include <flexui/screen.h>

int main() {
    flexui::Screen screen(900, 750, "shadcn/ui Components Demo");

    screen.loadCSS(R"(
        /* ================================================================
           shadcn/ui Design System - CSS Variables
           ================================================================ */
        :root {
            --background: #ffffff;
            --foreground: #0a0a0a;
            --card: #ffffff;
            --card-foreground: #0a0a0a;
            --primary: #18181b;
            --primary-foreground: #fafafa;
            --secondary: #f4f4f5;
            --secondary-foreground: #18181b;
            --muted: #f4f4f5;
            --muted-foreground: #71717a;
            --accent: #f4f4f5;
            --destructive: #ef4444;
            --destructive-foreground: #fafafa;
            --border: #e4e4e7;
            --input: #e4e4e7;
            --ring: #18181b;
            --radius: 6px;
        }

        /* ================================================================
           Base Layout
           ================================================================ */
        #root {
            display: flex;
            flex-direction: column;
            width: 100%;
            height: 100%;
            background: #fafafa;
            padding: 32px;
            gap: 24px;
        }

        .page-header {
            display: flex;
            flex-direction: column;
            gap: 4px;
            width: 100%;
            height: 56px;
        }

        .page-title {
            font-size: 24px;
            font-weight: 700;
            color: var(--foreground);
            height: 32px;
            width: 400px;
        }

        .page-description {
            font-size: 14px;
            color: var(--muted-foreground);
            height: 20px;
            width: 400px;
        }

        .content-row {
            display: flex;
            flex-direction: row;
            gap: 20px;
            width: 100%;
            height: 320px;
        }

        /* ================================================================
           Card Component
           ================================================================ */
        .card {
            display: flex;
            flex-direction: column;
            background: var(--card);
            border-radius: var(--radius);
            border: 1px solid var(--border);
            box-shadow: 0 1px 3px rgba(0,0,0,0.08);
        }

        .card-header {
            display: flex;
            flex-direction: column;
            padding: 20px;
            gap: 4px;
            height: 70px;
        }

        .card-title {
            font-size: 16px;
            font-weight: 600;
            color: var(--card-foreground);
            height: 22px;
            width: 100%;
        }

        .card-description {
            font-size: 13px;
            color: var(--muted-foreground);
            height: 18px;
            width: 100%;
        }

        .card-content {
            display: flex;
            flex-direction: column;
            padding: 0 20px 20px 20px;
            gap: 16px;
        }

        .card-footer {
            display: flex;
            flex-direction: row;
            padding: 16px 20px;
            gap: 12px;
            border-top: 1px solid var(--border);
            background: var(--muted);
            height: 72px;
        }

        /* ================================================================
           Button Component
           ================================================================ */
        .btn {
            height: 36px;
            border-radius: var(--radius);
            font-size: 13px;
            font-weight: 500;
            transition: background-color 0.15s, opacity 0.15s;
        }

        .btn-default {
            background: var(--primary);
            color: var(--primary-foreground);
            width: 90px;
        }
        .btn-default:hover {
            background: #27272a;
        }

        .btn-secondary {
            background: var(--secondary);
            color: var(--secondary-foreground);
            width: 90px;
        }
        .btn-secondary:hover {
            background: #e4e4e7;
        }

        .btn-destructive {
            background: var(--destructive);
            color: var(--destructive-foreground);
            width: 100px;
        }
        .btn-destructive:hover {
            background: #dc2626;
        }

        .btn-outline {
            background: var(--background);
            color: var(--foreground);
            border: 1px solid var(--input);
            width: 80px;
        }
        .btn-outline:hover {
            background: var(--accent);
        }

        .btn-ghost {
            background: transparent;
            color: var(--foreground);
            width: 70px;
        }
        .btn-ghost:hover {
            background: var(--accent);
        }

        .btn-sm {
            height: 32px;
            font-size: 12px;
            width: 70px;
        }

        .btn-lg {
            height: 42px;
            font-size: 14px;
            width: 90px;
        }

        .btn-wide {
            width: 130px;
        }

        .btn-cancel {
            width: 80px;
        }

        /* ================================================================
           Input Component
           ================================================================ */
        .input {
            height: 36px;
            width: 100%;
            padding: 8px 12px;
            background: var(--background);
            border: 1px solid var(--input);
            border-radius: var(--radius);
            font-size: 13px;
            color: var(--foreground);
            transition: border-color 0.15s;
        }

        .input:focus {
            border-color: var(--ring);
        }

        .form-field {
            display: flex;
            flex-direction: column;
            gap: 6px;
            width: 100%;
            height: 62px;
        }

        .label {
            font-size: 13px;
            font-weight: 500;
            color: var(--foreground);
            height: 18px;
            width: 100%;
        }

        /* ================================================================
           Badge Component
           ================================================================ */
        .badge {
            height: 22px;
            border-radius: 11px;
            font-size: 11px;
            font-weight: 500;
            padding: 0 8px;
        }

        .badge-default {
            background: var(--primary);
            color: var(--primary-foreground);
            width: 60px;
        }

        .badge-secondary {
            background: var(--secondary);
            color: var(--secondary-foreground);
            width: 75px;
        }

        .badge-destructive {
            background: var(--destructive);
            color: var(--destructive-foreground);
            width: 85px;
        }

        .badge-outline {
            background: transparent;
            color: var(--foreground);
            border: 1px solid var(--border);
            width: 60px;
        }

        /* ================================================================
           Alert Component
           ================================================================ */
        .alert {
            display: flex;
            flex-direction: column;
            padding: 14px 16px;
            border-radius: var(--radius);
            border: 1px solid var(--border);
            gap: 4px;
            width: 100%;
            height: 70px;
        }

        .alert-default {
            background: var(--background);
        }

        .alert-destructive {
            background: #fef2f2;
            border-color: #fecaca;
        }

        .alert-title {
            font-size: 13px;
            font-weight: 600;
            color: var(--foreground);
            height: 18px;
            width: 100%;
        }

        .alert-description {
            font-size: 13px;
            color: var(--muted-foreground);
            height: 18px;
            width: 100%;
        }

        .alert-destructive .alert-title {
            color: var(--destructive);
        }

        /* ================================================================
           Separator
           ================================================================ */
        .separator {
            width: 100%;
            height: 1px;
            background: var(--border);
        }

        /* ================================================================
           Layout Helpers
           ================================================================ */
        .demo-section {
            display: flex;
            flex-direction: column;
            gap: 10px;
            width: 100%;
        }

        .demo-row {
            display: flex;
            flex-direction: row;
            gap: 8px;
            height: 36px;
            width: 100%;
        }

        .demo-row-sm {
            display: flex;
            flex-direction: row;
            gap: 8px;
            height: 22px;
            width: 100%;
        }

        .demo-label {
            font-size: 11px;
            font-weight: 600;
            color: var(--muted-foreground);
            text-transform: uppercase;
            letter-spacing: 0.5px;
            height: 14px;
            width: 100%;
        }

        /* Card sizes */
        .buttons-card {
            width: 300px;
            height: 320px;
        }

        .form-card {
            width: 280px;
            height: 320px;
        }

        .badges-card {
            width: 200px;
            height: 180px;
        }

        .alerts-card {
            width: 100%;
            height: 210px;
        }
    )");

    screen.loadXML(R"(
        <div id="root">
            <div class="page-header">
                <label class="page-title">shadcn/ui Components</label>
                <label class="page-description">Beautifully designed components built with CSS</label>
            </div>

            <div class="separator"></div>

            <div class="content-row">
                <div class="card buttons-card">
                    <div class="card-header">
                        <label class="card-title">Buttons</label>
                        <label class="card-description">Button variants and sizes</label>
                    </div>
                    <div class="card-content">
                        <div class="demo-section">
                            <label class="demo-label">Variants</label>
                            <div class="demo-row">
                                <button class="btn btn-default">Default</button>
                                <button class="btn btn-secondary">Secondary</button>
                            </div>
                            <div class="demo-row">
                                <button class="btn btn-destructive">Destructive</button>
                                <button class="btn btn-outline">Outline</button>
                            </div>
                        </div>
                        <div class="demo-section">
                            <label class="demo-label">Sizes</label>
                            <div class="demo-row">
                                <button class="btn btn-default btn-sm">Small</button>
                                <button class="btn btn-default">Default</button>
                                <button class="btn btn-default btn-lg">Large</button>
                            </div>
                        </div>
                    </div>
                </div>

                <div class="card form-card">
                    <div class="card-header">
                        <label class="card-title">Create Account</label>
                        <label class="card-description">Enter your details below</label>
                    </div>
                    <div class="card-content">
                        <div class="form-field">
                            <label class="label">Name</label>
                            <input class="input" placeholder="John Doe" />
                        </div>
                        <div class="form-field">
                            <label class="label">Email</label>
                            <input class="input" placeholder="john@example.com" />
                        </div>
                    </div>
                    <div class="card-footer">
                        <button class="btn btn-default btn-wide">Create</button>
                        <button class="btn btn-ghost btn-cancel">Cancel</button>
                    </div>
                </div>

                <div class="card badges-card">
                    <div class="card-header">
                        <label class="card-title">Badges</label>
                        <label class="card-description">Status indicators</label>
                    </div>
                    <div class="card-content">
                        <div class="demo-row-sm">
                            <div class="badge badge-default">Default</div>
                            <div class="badge badge-secondary">Secondary</div>
                        </div>
                        <div class="demo-row-sm">
                            <div class="badge badge-destructive">Destructive</div>
                            <div class="badge badge-outline">Outline</div>
                        </div>
                    </div>
                </div>
            </div>

            <div class="card alerts-card">
                <div class="card-header">
                    <label class="card-title">Alerts</label>
                    <label class="card-description">Contextual feedback messages</label>
                </div>
                <div class="card-content">
                    <div class="alert alert-default">
                        <label class="alert-title">Heads up!</label>
                        <label class="alert-description">You can add components using the CLI.</label>
                    </div>
                    <div class="alert alert-destructive">
                        <label class="alert-title">Error</label>
                        <label class="alert-description">Your session has expired.</label>
                    </div>
                </div>
            </div>
        </div>
    )");

    while (screen.pollEvents()) {
        screen.draw();
    }

    return 0;
}
