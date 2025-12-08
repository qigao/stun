#include <flexui/screen.h>
#include <flexui/themes/themes.h>

int main() {
    flexui::Screen screen(800, 600, "FlexUI Theme Demo");

    // Option 1: Use the unified API
    // screen.loadCSS(flexui::themes::get(flexui::themes::Theme::Shadcn));
    
    // Option 2: Use the namespace directly
    screen.loadCSS(flexui::themes::shadcn::css);
    // Or: screen.loadCSS(flexui::themes::fluent::css);

    // App-specific styles (can override theme defaults)
    screen.loadCSS(R"(
        #root {
            display: flex;
            flex-direction: column;
            width: 100%;
            height: 100%;
            background: #fafafa;
            padding: 32px;
            gap: 24px;
        }

        .header {
            display: flex;
            flex-direction: column;
            gap: 4px;
            height: 60px;
            width: 100%;
        }

        .title {
            font-size: 24px;
            font-weight: 700;
            color: var(--foreground);
            height: 32px;
            width: 300px;
        }

        .subtitle {
            font-size: 14px;
            color: var(--muted-foreground);
            height: 20px;
            width: 400px;
        }

        .content {
            display: flex;
            flex-direction: row;
            gap: 20px;
            width: 100%;
            height: 400px;
        }

        .demo-card {
            width: 350px;
            height: 380px;
        }

        .btn-row {
            display: flex;
            flex-direction: row;
            gap: 8px;
            height: 36px;
            width: 100%;
        }

        .form-field {
            display: flex;
            flex-direction: column;
            gap: 6px;
            width: 100%;
            height: 62px;
        }

        .label {
            height: 18px;
            width: 100%;
        }

        .input {
            width: 100%;
        }

        .btn { width: 100px; }
        .btn-wide { width: 140px; }
    )");

    screen.loadXML(R"(
        <div id="root">
            <div class="header">
                <label class="title">Theme Demo</label>
                <label class="subtitle">Using shadcn/ui theme module</label>
            </div>

            <div class="separator"></div>

            <div class="content">
                <div class="card demo-card">
                    <div class="card-header">
                        <label class="card-title">Button Variants</label>
                        <label class="card-description">Different button styles</label>
                    </div>
                    <div class="card-content">
                        <div class="btn-row">
                            <button class="btn btn-default">Default</button>
                            <button class="btn btn-secondary">Secondary</button>
                        </div>
                        <div class="btn-row">
                            <button class="btn btn-outline">Outline</button>
                            <button class="btn btn-ghost">Ghost</button>
                        </div>
                        <div class="btn-row">
                            <button class="btn btn-destructive">Delete</button>
                        </div>
                    </div>
                </div>

                <div class="card demo-card">
                    <div class="card-header">
                        <label class="card-title">Login Form</label>
                        <label class="card-description">Enter your credentials</label>
                    </div>
                    <div class="card-content">
                        <div class="form-field">
                            <label class="label">Email</label>
                            <input class="input" placeholder="you@example.com" />
                        </div>
                        <div class="form-field">
                            <label class="label">Password</label>
                            <input class="input" placeholder="Enter password" />
                        </div>
                    </div>
                    <div class="card-footer">
                        <button class="btn btn-default btn-wide">Sign In</button>
                        <button class="btn btn-ghost">Cancel</button>
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
