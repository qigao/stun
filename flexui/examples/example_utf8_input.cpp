#include <flexui/screen.h>
#include <flexui/textbox.h>
#include <flexui/button.h>
#include <flexui/font_manager.h>

int main() {
    flexui::Screen screen(800, 750, "UTF-8 Input Test");

    screen.loadCSS(R"(
        :root {
            --font-family: sans-serif;
            --font-size: 16px;
        }

        #container {
            display: flex;
            flex-direction: column;
            gap: 20px;
            padding: 30px;
            width: 100%;
            height: 100%;
            background: #f5f5f5;
        }

        .title {
            width: 100%;
            height: 40px;
            font-size: 24px;
            font-weight: bold;
            color: #333;
        }

        .info {
            width: 100%;
            height: 30px;
            font-size: 14px;
            color: #666;
        }

        .row {
            display: flex;
            flex-direction: row;
            gap: 15px;
            width: 100%;
            height: 50px;
        }



        .input {
            width: 540px;
            height: 50px;
            background: white;
            border: 2px solid #ddd;
            border-radius: 6px;
            padding: 12px;
            font-family: var(--font-family);
            font-size: var(--font-size);
        }
        
        .label {
            width: 150px;
            height: 50px;
            font-size: 16px;
            color: #333;
            font-family: var(--font-family);
        }

        .input:focus {
            border-color: #2196F3;
        }

        .btn {
            width: 100px;
            height: 36px;
            background: #2196F3;
            color: white;
            border: none;
            border-radius: 6px;
            font-size: 13px;
            cursor: pointer;
        }

        .btn:hover {
            background: #1976D2;
        }

        .btn-small {
            width: 60px;
            height: 36px;
            background: #4CAF50;
            color: white;
            border: none;
            border-radius: 6px;
            font-size: 13px;
            cursor: pointer;
        }

        .btn-small:hover {
            background: #45a049;
        }

        .font-row {
            display: flex;
            flex-direction: row;
            gap: 10px;
            width: 100%;
            height: 50px;
            align-items: center;
        }

        .help {
            width: 100%;
            height: 30px;
            font-size: 12px;
            color: #888;
            margin-top: 20px;
        }
    )");

    screen.loadXML(R"(
        <div id="container">
            <label class="title">UTF-8 Text Input Test</label>
            <label class="info">Test: Chinese (中文), Japanese (日本語), Emoji (🎉🚀💻)</label>
            
            <div class="font-row">
                <label class="label">Font:</label>
                <button id="btn-arial" class="btn">Arial</button>
                <button id="btn-times" class="btn">Times</button>
                <button id="btn-courier" class="btn">Courier</button>
                <button id="btn-yahei" class="btn">微软雅黑</button>
            </div>
            
            <div class="font-row">
                <label class="label">Size:</label>
                <button id="btn-size-12" class="btn-small">12px</button>
                <button id="btn-size-16" class="btn-small">16px</button>
                <button id="btn-size-20" class="btn-small">20px</button>
                <button id="btn-size-24" class="btn-small">24px</button>
            </div>
            
            <div class="row">
                <label class="label">Arial:</label>
                <input id="input1" class="input" placeholder="Type anything..." style="font-family: arial;"/>
            </div>
            
            <div class="row">
                <label class="label">Times:</label>
                <input id="input2" class="input" placeholder="输入中文..." style="font-family: times;"/>
            </div>
            
            <div class="row">
                <label class="label">Courier:</label>
                <input id="input3" class="input" placeholder="Add emoji..." style="font-family: courier;"/>
            </div>
            
            <div class="row">
                <label class="label">微软雅黑:</label>
                <input id="input4" class="input" placeholder="Mix everything..." style="font-family: yahei;"/>
            </div>
            
            <div class="row">
                <label class="label">Output:</label>
                <label id="output" class="label" style="width: 540px;">Click an input to start</label>
            </div>
            
            <label class="help">Keys: ← → Home End | Shift+Arrow = Select | Ctrl+A = All | Ctrl+C/X/V = Copy/Cut/Paste</label>
        </div>
    )");

    // Set up TextBox instances
    auto* input1 = dynamic_cast<flexui::TextBox*>(screen.findWidget("input1"));
    auto* input2 = dynamic_cast<flexui::TextBox*>(screen.findWidget("input2"));
    auto* input3 = dynamic_cast<flexui::TextBox*>(screen.findWidget("input3"));
    auto* input4 = dynamic_cast<flexui::TextBox*>(screen.findWidget("input4"));
    auto* output = screen.findWidget("output");

    if (input1) input1->setScreen(&screen);
    if (input2) {
        input2->setScreen(&screen);
        input2->setInputText("你好世界");
    }
    if (input3) {
        input3->setScreen(&screen);
        input3->setInputText("🎉🚀💻🌟");
    }
    if (input4) {
        input4->setScreen(&screen);
        input4->setInputText("Hello 世界 🌍");
    }

    // Set callbacks
    auto updateOutput = [output](const std::string& text) {
        if (output) {
            output->setText("Value: " + text);
        }
    };

    if (input1) input1->setChangeCallback(updateOutput);
    if (input2) input2->setChangeCallback(updateOutput);
    if (input3) input3->setChangeCallback(updateOutput);
    if (input4) input4->setChangeCallback(updateOutput);

    // Font switching buttons
    auto* btnArial = dynamic_cast<flexui::Button*>(screen.findWidget("btn-arial"));
    auto* btnTimes = dynamic_cast<flexui::Button*>(screen.findWidget("btn-times"));
    auto* btnCourier = dynamic_cast<flexui::Button*>(screen.findWidget("btn-courier"));
    auto* btnYahei = dynamic_cast<flexui::Button*>(screen.findWidget("btn-yahei"));

    auto* fontMgr = screen.fontManager();

    // Pre-load all fonts
    fontMgr->loadFont("arial", std::vector<std::string>{
        "C:/Windows/Fonts/arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf"
    });
    
    fontMgr->loadFont("times", std::vector<std::string>{
        "C:/Windows/Fonts/times.ttf",
        "/System/Library/Fonts/Times.ttc",
        "/usr/share/fonts/truetype/liberation/LiberationSerif-Regular.ttf"
    });
    
    fontMgr->loadFont("courier", std::vector<std::string>{
        "C:/Windows/Fonts/cour.ttf",
        "/System/Library/Fonts/Courier.ttc",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf"
    });
    
    fontMgr->loadFont("yahei", std::vector<std::string>{
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/msyhbd.ttc",
        "/System/Library/Fonts/PingFang.ttc"
    });
    
    // Add CJK and Emoji fallbacks to all fonts
    int cjk_font = fontMgr->getFontId("sans-serif-cjk");
    int emoji_font = fontMgr->getFontId("emoji");
    
    for (const char* font_name : {"arial", "times", "courier", "yahei"}) {
        int font_id = fontMgr->getFontId(font_name);
        if (font_id != -1) {
            if (cjk_font != -1) {
                nvgAddFallbackFontId(screen.vg(), font_id, cjk_font);
            }
            if (emoji_font != -1) {
                nvgAddFallbackFontId(screen.vg(), font_id, emoji_font);
            }
        }
    }

    if (btnArial) {
        btnArial->setClickCallback([&screen, output, input1](flexui::Widget*) {
            if (input1) {
                input1->setInlineStyle("font-family", "arial");
            }
            if (output) {
                output->setText("Input1: Arial");
            }
            return true;
        });
    }

    if (btnTimes) {
        btnTimes->setClickCallback([&screen, output, input2](flexui::Widget*) {
            if (input2) {
                input2->setInlineStyle("font-family", "times");
            }
            if (output) {
                output->setText("Input2: Times");
            }
            return true;
        });
    }

    if (btnCourier) {
        btnCourier->setClickCallback([&screen, output, input3](flexui::Widget*) {
            if (input3) {
                input3->setInlineStyle("font-family", "courier");
            }
            if (output) {
                output->setText("Input3: Courier");
            }
            return true;
        });
    }

    if (btnYahei) {
        btnYahei->setClickCallback([&screen, output, input4](flexui::Widget*) {
            if (input4) {
                input4->setInlineStyle("font-family", "yahei");
            }
            if (output) {
                output->setText("Input4: 微软雅黑");
            }
            return true;
        });
    }

    // Font size buttons
    auto* btnSize12 = dynamic_cast<flexui::Button*>(screen.findWidget("btn-size-12"));
    auto* btnSize16 = dynamic_cast<flexui::Button*>(screen.findWidget("btn-size-16"));
    auto* btnSize20 = dynamic_cast<flexui::Button*>(screen.findWidget("btn-size-20"));
    auto* btnSize24 = dynamic_cast<flexui::Button*>(screen.findWidget("btn-size-24"));

    auto setFontSize = [&screen, output](int size) {
        screen.setCSSVariable("--font-size", std::to_string(size) + "px");
        if (output) {
            output->setText("Size: " + std::to_string(size) + "px");
        }
    };

    if (btnSize12) btnSize12->setClickCallback([setFontSize](flexui::Widget*) { setFontSize(12); return true; });
    if (btnSize16) btnSize16->setClickCallback([setFontSize](flexui::Widget*) { setFontSize(16); return true; });
    if (btnSize20) btnSize20->setClickCallback([setFontSize](flexui::Widget*) { setFontSize(20); return true; });
    if (btnSize24) btnSize24->setClickCallback([setFontSize](flexui::Widget*) { setFontSize(24); return true; });

    // Main loop
    while (screen.pollEvents()) {
        screen.draw();
    }

    return 0;
}
