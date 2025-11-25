#include <flexui/screen.h>

int main() {
    flexui::Screen screen(500, 400, "JavaScript Demo");

    // CSS styling
    screen.loadCSS(R"(
        #main-panel {
            display: flex;
            flex-direction: column;
            gap: 15px;
            padding: 25px;
            width: 400px;
            height: 340px;
            background: #ffffff;
            border: 2px solid #FF5722;
            border-radius: 12px;
            position: absolute;
            top: 30px;
            left: 50px;
        }
        #title {
            width: 350px;
            height: 36px;
        }
        #counter-display {
            width: 350px;
            height: 50px;
        }
        .button-row {
            display: flex;
            flex-direction: row;
            gap: 15px;
            width: 350px;
            height: 50px;
        }
        button {
            width: 110px;
            height: 45px;
        }
        #status {
            width: 350px;
            height: 30px;
        }
        input {
            width: 350px;
            height: 40px;
        }
        slider {
            width: 350px;
            height: 35px;
        }
    )");

    // XML-based UI
    screen.loadXML(R"(
        <div id="main-panel">
            <label id="title">JavaScript Counter Demo</label>
            <label id="counter-display">Count: 0</label>

            <div class="button-row">
                <button id="inc-btn" onclick="increment">+1</button>
                <button id="dec-btn" onclick="decrement">-1</button>
                <button id="reset-btn" onclick="reset">Reset</button>
            </div>

            <button id="auto-btn" onclick="startAutoIncrement">Auto +1 (every 500ms)</button>

            <input id="name-input" placeholder="Enter your name"/>
            <button id="greet-btn" onclick="greet">Greet</button>

            <label id="status">Ready</label>
        </div>
    )");

    // JavaScript logic - this is where the magic happens!
    // Note: Using R"JS(...)JS" delimiter to handle ) characters in JS code
    screen.loadJS(R"JS(
        // Global counter variable
        var counter = 0;
        var autoTimerId = null;

        // Update the display
        function updateDisplay() {
            var display = getWidget("counter-display");
            display.setText("Count: " + counter);
        }

        // Increment counter
        function increment() {
            counter++;
            updateDisplay();
            console.log("Counter incremented to " + counter);
        }

        // Decrement counter
        function decrement() {
            counter--;
            updateDisplay();
            console.log("Counter decremented to " + counter);
        }

        // Reset counter
        function reset() {
            counter = 0;
            updateDisplay();
            console.log("Counter reset");

            // Stop auto-increment if running
            if (autoTimerId !== null) {
                clearTimeout(autoTimerId);
                autoTimerId = null;
                getWidget("auto-btn").setText("Auto +1 every 500ms");
            }
        }

        // Auto increment with setTimeout
        function startAutoIncrement() {
            if (autoTimerId !== null) {
                // Stop if already running
                clearTimeout(autoTimerId);
                autoTimerId = null;
                getWidget("auto-btn").setText("Auto +1 every 500ms");
                getWidget("status").setText("Auto-increment stopped");
            } else {
                // Start auto-increment
                autoIncrement();
                getWidget("auto-btn").setText("Stop Auto");
                getWidget("status").setText("Auto-incrementing...");
            }
        }

        function autoIncrement() {
            increment();
            // Schedule next increment
            autoTimerId = setTimeout(function() {
                autoIncrement();
            }, 500);
        }

        // Greet the user
        function greet() {
            var nameInput = getWidget("name-input");
            var name = nameInput.getText();

            if (name === "") {
                getWidget("status").setText("Please enter your name!");
            } else {
                getWidget("status").setText("Hello, " + name + "!");
                console.log("Greeting: Hello, " + name);
            }
        }

        // Initial log
        console.log("JavaScript Counter Demo loaded!");
        console.log("Available functions: increment, decrement, reset, greet");
    )JS");

    // Main loop
    while (screen.pollEvents()) {
        screen.draw();
    }

    return 0;
}
