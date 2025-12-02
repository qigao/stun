#include <flexui/screen.h>

int main() {
    flexui::Screen screen(800, 600, "FlexUI Tailwind Features Demo");

    // CSS with Tailwind-inspired features
    screen.loadCSS(R"(
        /* CSS Variables (Tailwind color palette) */
        :root {
            --blue-500: #3b82f6;
            --blue-600: #2563eb;
            --green-500: #10b981;
            --green-600: #059669;
            --gray-300: #d1d5db;
            --gray-500: #6b7280;
            --gray-600: #4b5563;
            --gray-700: #374151;
        }

        /* Main container */
        #main-container {
            display: flex;
            flex-direction: column;
            gap: 30px;
            padding: 30px;
            width: 100%;
            height: 100%;
            background: #f9fafb;
        }

        /* Section headers */
        .section-title {
            width: 100%;
            height: 30px;
            font-size: 18px;
            font-weight: bold;
            color: var(--gray-700);
        }

        /* ========================================================================
           1. RESPONSIVE BUTTON GROUP LAYOUT
           ======================================================================== */
        
        /* Default: Horizontal layout for desktop */
        .button-group {
            display: flex;
            flex-direction: row;
            gap: 15px;
            width: 100%;
        }

        /* Small screens: Stack buttons vertically */
        @media (max-width: 600px) {
            .button-group {
                flex-direction: column;
            }
        }

        /* ========================================================================
           2. BUTTON HOVER EFFECTS
           ======================================================================== */
        
        .btn {
            width: 150px;
            height: 45px;
            border-radius: 6px;
            font-weight: 600;
            transition: background-color 0.2s;
        }

        /* Small screens: Full width buttons */
        @media (max-width: 600px) {
            .btn {
                width: 100%;
            }
        }

        .btn-primary {
            background-color: var(--blue-500);
            color: white;
        }

        .btn-primary:hover {
            background-color: var(--blue-600);
        }

        .btn-secondary {
            background-color: var(--gray-500);
            color: white;
        }

        .btn-secondary:hover {
            background-color: var(--gray-600);
        }

        .btn-success {
            background-color: var(--green-500);
            color: white;
        }

        .btn-success:hover {
            background-color: var(--green-600);
        }

        /* ========================================================================
           3. FORM INPUT FOCUS STATES
           ======================================================================== */
        
        .form-group {
            display: flex;
            flex-direction: column;
            gap: 10px;
            width: 100%;
        }

        .input-label {
            width: 100%;
            height: 25px;
            font-size: 14px;
            color: var(--gray-700);
        }

        .input {
            width: 100%;
            height: 45px;
            padding: 10px;
            border-width: 1px;
            border-style: solid;
            border-color: var(--gray-300);
            border-radius: 6px;
            background: white;
            transition: border-color 0.2s, border-width 0.2s;
        }

        .input:focus {
            border-color: var(--blue-500);
            border-width: 2px;
            outline: none;
        }

        /* Info text */
        .info-text {
            width: 100%;
            height: 20px;
            font-size: 12px;
            color: var(--gray-500);
        }

        /* Divider */
        .divider {
            width: 100%;
            height: 2px;
            background: var(--gray-300);
        }
    )");

    // XML-based UI layout
    screen.loadXML(R"(
        <div id="main-container">
            <!-- Section 1: Responsive Button Group -->
            <label class="section-title">1. Responsive Button Layout</label>
            <label class="info-text">Resize window to &lt; 600px to see vertical layout</label>
            
            <div class="button-group">
                <button class="btn btn-primary">Primary</button>
                <button class="btn btn-secondary">Secondary</button>
                <button class="btn btn-success">Success</button>
            </div>

            <div class="divider"></div>

            <!-- Section 2: Button Hover Effects -->
            <label class="section-title">2. Button Hover Effects</label>
            <label class="info-text">Hover over buttons to see color transitions</label>
            
            <div class="button-group">
                <button class="btn btn-primary">Hover Me</button>
                <button class="btn btn-secondary">Try This</button>
                <button class="btn btn-success">And This</button>
            </div>

            <div class="divider"></div>

            <!-- Section 3: Form Input Focus States -->
            <label class="section-title">3. Input Focus States</label>
            <label class="info-text">Click on inputs to see focus styling</label>
            
            <div class="form-group">
                <label class="input-label">Email Address</label>
                <input class="input" placeholder="Enter your email"/>
                
                <label class="input-label">Full Name</label>
                <input class="input" placeholder="Enter your name"/>
            </div>
        </div>
    )");

    // Main event loop
    while (screen.pollEvents()) {
        screen.draw();
    }

    return 0;
}
