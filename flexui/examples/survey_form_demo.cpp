#include <flexui/screen.h>
#include <flexui/radiobutton.h>
#include <iostream>

int main() {
    flexui::Screen screen(500, 500, "Survey Form Demo");

    // CSS styling
    screen.loadCSS(R"(
        #survey-panel {
            display: flex;
            flex-direction: column;
            gap: 20px;
            padding: 30px;
            width: 400px;
            height: 440px;
            background: #ffffff;
            border: 2px solid #673AB7;
            border-radius: 12px;
            position: absolute;
            top: 30px;
            left: 50px;
        }
        #title {
            width: 340px;
            height: 36px;
        }
        .question {
            width: 340px;
            height: 24px;
        }
        .option-row {
            display: flex;
            flex-direction: row;
            gap: 10px;
            width: 340px;
            height: 28px;
        }
        radio {
            width: 24px;
            height: 24px;
        }
        .option-label {
            width: 120px;
            height: 24px;
        }
        button {
            width: 340px;
            height: 48px;
        }
    )");

    // XML-based UI definition
    screen.loadXML(R"(
        <div id="survey-panel">
            <label id="title">Customer Survey</label>

            <label class="question">1. How satisfied are you?</label>
            <div class="option-row">
                <radio id="sat-1" name="satisfaction" value="very_satisfied"/>
                <label class="option-label">Very Satisfied</label>
            </div>
            <div class="option-row">
                <radio id="sat-2" name="satisfaction" value="satisfied"/>
                <label class="option-label">Satisfied</label>
            </div>
            <div class="option-row">
                <radio id="sat-3" name="satisfaction" value="neutral" checked="true"/>
                <label class="option-label">Neutral</label>
            </div>
            <div class="option-row">
                <radio id="sat-4" name="satisfaction" value="dissatisfied"/>
                <label class="option-label">Dissatisfied</label>
            </div>

            <label class="question">2. Would you recommend us?</label>
            <div class="option-row">
                <radio id="rec-1" name="recommend" value="yes" checked="true"/>
                <label class="option-label">Yes</label>
            </div>
            <div class="option-row">
                <radio id="rec-2" name="recommend" value="no"/>
                <label class="option-label">No</label>
            </div>

            <button id="submit-btn" onclick="handleSubmit">Submit Survey</button>
        </div>
    )");

    // Register submit handler
    screen.registerHandler("handleSubmit", [&](flexui::Widget* w) {
        std::cout << "=== Survey Results ===" << std::endl;
        std::cout << "Satisfaction: " << screen.getRadioGroupValue("satisfaction") << std::endl;
        std::cout << "Would Recommend: " << screen.getRadioGroupValue("recommend") << std::endl;
        return true;
    });

    // Main loop
    while (screen.pollEvents()) {
        screen.draw();
    }

    return 0;
}
