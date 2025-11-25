#include <flexui/screen.h>
#include <flexui/label.h>
#include <flexui/slider.h>
#include <iostream>

int main() {
    flexui::Screen screen(500, 450, "Settings Demo");

    // Load CSS for settings layout
    screen.loadCSS(R"(
        #settings-panel {
            display: flex;
            flex-direction: column;
            gap: 25px;
            padding: 30px;
            width: 400px;
            height: 390px;
            background: #ffffff;
            border: 2px solid #9C27B0;
            border-radius: 12px;
            position: absolute;
            top: 30px;
            left: 50px;
        }
        #title {
            width: 340px;
            height: 40px;
        }
        .setting-row {
            display: flex;
            flex-direction: column;
            gap: 8px;
            width: 340px;
            height: 60px;
        }
        label {
            width: 340px;
            height: 20px;
        }
        input {
            width: 340px;
            height: 32px;
        }
    )");

    // Create settings panel
    auto* panel = screen.addWidget("settings-panel", "div");

    // Title
    auto* title = screen.createWidget<flexui::Label>("title", "Audio & Display Settings");
    flexui::LabelStyle titleStyle;
    titleStyle.fontSize = 22;
    titleStyle.textColor = nvgRGB(33, 33, 33);
    title->setLabelStyle(titleStyle);
    panel->addChild(title);

    // Volume setting
    auto* volumeRow = screen.addWidget("volume-row", "div");
    volumeRow->setClass("setting-row");
    panel->addChild(volumeRow);

    auto* volumeLabel = screen.createWidget<flexui::Label>("volume-label", "Volume: 50%");
    volumeRow->addChild(volumeLabel);

    auto* volumeSlider = screen.createWidget<flexui::Slider>("volume-slider", 0.5f, 0.0f, 1.0f);
    flexui::SliderStyle volumeStyle;
    volumeStyle.fillColor = nvgRGB(76, 175, 80);  // Green
    volumeStyle.handleColor = nvgRGB(76, 175, 80);
    volumeStyle.showValue = false;
    volumeSlider->setSliderStyle(volumeStyle);
    volumeSlider->setValueCallback([volumeLabel](float value) {
        char text[64];
        snprintf(text, sizeof(text), "Volume: %.0f%%", value * 100);
        volumeLabel->setText(text);
        std::cout << "Volume: " << (int)(value * 100) << "%" << std::endl;
    });
    volumeRow->addChild(volumeSlider);

    // Brightness setting
    auto* brightnessRow = screen.addWidget("brightness-row", "div");
    brightnessRow->setClass("setting-row");
    panel->addChild(brightnessRow);

    auto* brightnessLabel = screen.createWidget<flexui::Label>("brightness-label", "Brightness: 70%");
    brightnessRow->addChild(brightnessLabel);

    auto* brightnessSlider = screen.createWidget<flexui::Slider>("brightness-slider", 0.7f, 0.0f, 1.0f);
    flexui::SliderStyle brightnessStyle;
    brightnessStyle.fillColor = nvgRGB(255, 193, 7);  // Amber
    brightnessStyle.handleColor = nvgRGB(255, 193, 7);
    brightnessStyle.showValue = false;
    brightnessSlider->setSliderStyle(brightnessStyle);
    brightnessSlider->setValueCallback([brightnessLabel](float value) {
        char text[64];
        snprintf(text, sizeof(text), "Brightness: %.0f%%", value * 100);
        brightnessLabel->setText(text);
        std::cout << "Brightness: " << (int)(value * 100) << "%" << std::endl;
    });
    brightnessRow->addChild(brightnessSlider);

    // Opacity setting
    auto* opacityRow = screen.addWidget("opacity-row", "div");
    opacityRow->setClass("setting-row");
    panel->addChild(opacityRow);

    auto* opacityLabel = screen.createWidget<flexui::Label>("opacity-label", "Opacity: 100%");
    opacityRow->addChild(opacityLabel);

    auto* opacitySlider = screen.createWidget<flexui::Slider>("opacity-slider", 1.0f, 0.0f, 1.0f);
    flexui::SliderStyle opacityStyle;
    opacityStyle.fillColor = nvgRGB(156, 39, 176);  // Purple
    opacityStyle.handleColor = nvgRGB(156, 39, 176);
    opacityStyle.showValue = false;
    opacitySlider->setSliderStyle(opacityStyle);
    opacitySlider->setValueCallback([opacityLabel](float value) {
        char text[64];
        snprintf(text, sizeof(text), "Opacity: %.0f%%", value * 100);
        opacityLabel->setText(text);
        std::cout << "Opacity: " << (int)(value * 100) << "%" << std::endl;
    });
    opacityRow->addChild(opacitySlider);

    // Main loop
    while (screen.pollEvents()) {
        screen.draw();
    }

    return 0;
}
