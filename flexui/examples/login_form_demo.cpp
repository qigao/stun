#include <flexui/screen.h>
#include <flexui/label.h>
#include <flexui/textbox.h>
#include <flexui/checkbox.h>
#include <flexui/button.h>
#include <iostream>

int main() {
    flexui::Screen screen(500, 400, "Login Form Demo");

    // Load CSS for login form layout
    screen.loadCSS(R"(
        #login-panel {
            display: flex;
            flex-direction: column;
            gap: 15px;
            padding: 30px;
            width: 350px;
            height: 320px;
            background: #ffffff;
            border: 2px solid #2196F3;
            border-radius: 12px;
            position: absolute;
            top: 40px;
            left: 75px;
        }
        #title {
            width: 290px;
            height: 40px;
        }
        label {
            width: 290px;
            height: 24px;
        }
        input {
            width: 290px;
            height: 40px;
        }
        #remember-row {
            display: flex;
            flex-direction: row;
            gap: 8px;
            width: 290px;
            height: 32px;
        }
        checkbox {
            width: 24px;
            height: 24px;
        }
        #remember-label {
            width: 120px;
            height: 24px;
        }
        button {
            width: 290px;
            height: 48px;
        }
    )");

    // Create login panel container
    auto* panel = screen.addWidget("login-panel", "div");

    // Title
    auto* title = screen.createWidget<flexui::Label>("title", "Login to Your Account");
    flexui::LabelStyle titleStyle;
    titleStyle.fontSize = 20;
    titleStyle.textColor = nvgRGB(33, 33, 33);
    title->setLabelStyle(titleStyle);
    panel->addChild(title);

    // Username label
    auto* userLabel = screen.createWidget<flexui::Label>("user-label", "Username:");
    panel->addChild(userLabel);

    // Username input
    auto* userInput = screen.createWidget<flexui::TextBox>("user-input", "Enter username");
    userInput->setScreen(&screen);
    panel->addChild(userInput);

    // Password label
    auto* passLabel = screen.createWidget<flexui::Label>("pass-label", "Password:");
    panel->addChild(passLabel);

    // Password input (with password masking)
    auto* passInput = screen.createWidget<flexui::TextBox>("pass-input", "Enter password");
    passInput->setScreen(&screen);
    passInput->setPasswordMode(true);
    panel->addChild(passInput);

    // Remember me row
    auto* rememberRow = screen.addWidget("remember-row", "div");
    panel->addChild(rememberRow);

    auto* rememberCheck = screen.createWidget<flexui::Checkbox>("remember-check", false);
    rememberRow->addChild(rememberCheck);

    auto* rememberLabel = screen.createWidget<flexui::Label>("remember-label", "Remember me");
    rememberRow->addChild(rememberLabel);

    // Login button
    auto* loginBtn = screen.createWidget<flexui::Button>("login-btn", "Login");
    loginBtn->setClickCallback([&](flexui::Widget* w) {
        std::cout << "=== Login Attempt ===" << std::endl;
        std::cout << "Username: " << userInput->getInputText() << std::endl;
        std::cout << "Password: " << passInput->getInputText() << std::endl;
        std::cout << "Remember: " << (rememberCheck->isChecked() ? "Yes" : "No") << std::endl;
        return true;
    });
    panel->addChild(loginBtn);

    // Main loop
    while (screen.pollEvents()) {
        screen.draw();
    }

    return 0;
}
