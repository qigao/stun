#include <flexui/screen.h>
#include <flexui/label.h>
#include <flexui/progressbar.h>
#include <flexui/button.h>
#include <iostream>
#include <chrono>

int main() {
    flexui::Screen screen(500, 300, "File Upload Demo");

    // Load CSS for upload UI layout
    screen.loadCSS(R"(
        #upload-panel {
            display: flex;
            flex-direction: column;
            gap: 20px;
            padding: 30px;
            width: 400px;
            height: 240px;
            background: #ffffff;
            border: 2px solid #4CAF50;
            border-radius: 12px;
            position: absolute;
            top: 30px;
            left: 50px;
        }
        #title {
            width: 340px;
            height: 40px;
        }
        #status-label {
            width: 340px;
            height: 30px;
        }
        #progress {
            width: 340px;
            height: 50px;
        }
        button {
            width: 340px;
            height: 50px;
        }
    )");

    // Create upload panel container
    auto* panel = screen.addWidget("upload-panel", "div");

    // Title
    auto* title = screen.createWidget<flexui::Label>("title", "File Upload Simulator");
    flexui::LabelStyle titleStyle;
    titleStyle.fontSize = 22;
    titleStyle.textColor = nvgRGB(33, 33, 33);
    title->setLabelStyle(titleStyle);
    panel->addChild(title);

    // Status label
    auto* statusLabel = screen.createWidget<flexui::Label>("status-label", "Click 'Start Upload' to begin");
    flexui::LabelStyle statusStyle;
    statusStyle.fontSize = 14;
    statusStyle.textColor = nvgRGB(100, 100, 100);
    statusLabel->setLabelStyle(statusStyle);
    panel->addChild(statusLabel);

    // Progress bar
    auto* progressBar = screen.createWidget<flexui::ProgressBar>("progress", 0.0f);
    panel->addChild(progressBar);

    // Upload button
    auto* uploadBtn = screen.createWidget<flexui::Button>("upload-btn", "Start Upload");

    // State variables for animation
    bool uploading = false;
    float progress = 0.0f;
    auto startTime = std::chrono::steady_clock::now();

    uploadBtn->setClickCallback([&](flexui::Widget* w) {
        if (!uploading) {
            uploading = true;
            progress = 0.0f;
            startTime = std::chrono::steady_clock::now();
            statusLabel->setText("Uploading file...");
            std::cout << "=== Upload Started ===" << std::endl;
        }
        return true;
    });
    panel->addChild(uploadBtn);

    // Main loop with upload simulation
    while (screen.pollEvents()) {
        // Simulate upload progress
        if (uploading) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();

            // Upload completes in 3 seconds
            progress = std::min(elapsed / 3000.0f, 1.0f);
            progressBar->setProgress(progress);

            if (progress >= 1.0f) {
                uploading = false;
                statusLabel->setText("Upload complete!");
                uploadBtn->setText("Upload Again");
                std::cout << "=== Upload Complete ===" << std::endl;
            }
        }

        screen.draw();
    }

    return 0;
}
