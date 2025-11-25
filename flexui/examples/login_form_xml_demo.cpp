#include <flexui/screen.h>
#include <flexui/textbox.h>
#include <flexui/checkbox.h>
#include <iostream>

int main() {
    flexui::Screen screen(500, 400, "Login Form XML Demo");

    // Load CSS for styling
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

    // Load XML for UI structure - Much cleaner than C++ code!
    screen.loadXML(R"(
        <div id="login-panel">
            <label id="title">Login to Your Account</label>
            <label>Username:</label>
            <input id="user-input" placeholder="Enter username"/>
            <label>Password:</label>
            <input id="pass-input" placeholder="Enter password" password="true"/>
            <div id="remember-row">
                <checkbox id="remember-check"/>
                <label id="remember-label">Remember me</label>
            </div>
            <button id="login-btn" onclick="handleLogin">Login</button>
        </div>
    )");

    // Register event handler in C++
    screen.registerHandler("handleLogin", [&](flexui::Widget* w) {
        // Find widgets by ID and access their data
        auto* userInput = dynamic_cast<flexui::TextBox*>(screen.findWidget("user-input"));
        auto* passInput = dynamic_cast<flexui::TextBox*>(screen.findWidget("pass-input"));
        auto* rememberCheck = dynamic_cast<flexui::Checkbox*>(screen.findWidget("remember-check"));

        std::cout << "=== Login Attempt ===" << std::endl;
        if (userInput)
            std::cout << "Username: " << userInput->getInputText() << std::endl;
        if (passInput)
            std::cout << "Password: " << passInput->getInputText() << std::endl;
        if (rememberCheck)
            std::cout << "Remember: " << (rememberCheck->isChecked() ? "Yes" : "No") << std::endl;

        return true;
    });

    // Main loop
    while (screen.pollEvents()) {
        screen.draw();
    }

    return 0;
}
