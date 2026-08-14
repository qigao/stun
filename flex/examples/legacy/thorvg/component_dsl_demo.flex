// Component DSL Demo - Using Components in .flex Files
// Components are registered in C++, then used directly in DSL

scene componentDslDemo {
    width: 1200
    height: 800

    // Background
    rect background {
        x: 0, y: 0
        width: 1200, height: 800
        fill: #f8f9fa
    }

    // Title
    text title {
        x: 600, y: 40
        content: "Component DSL Demo"
        fontSize: 32
        color: #212529
    }

    text subtitle {
        x: 600, y: 75
        content: "Using C++ Components Directly in .flex Files"
        fontSize: 16
        color: #6c757d
    }

    // Section 1: Sliders (using Slider component from C++)
    group sliderSection {
        x: 50, y: 120

        text sectionTitle {
            x: 0, y: 0
            content: "Sliders (from DSL)"
            fontSize: 20
            color: #495057
        }

        // Slider instances defined in DSL!
        Slider slider1 {
            x: 0, y: 40
            value: 0.3
            width: 350
            color: #0d6efd
        }

        Slider slider2 {
            x: 0, y: 100
            value: 0.7
            width: 350
            color: #dc3545
        }

        Slider slider3 {
            x: 0, y: 160
            value: 1.0
            width: 350
            color: #198754
        }
    }

    // Section 2: Progress Bars
    group progressSection {
        x: 500, y: 120

        text sectionTitle {
            x: 0, y: 0
            content: "Progress Bars (from DSL)"
            fontSize: 20
            color: #495057
        }

        ProgressBar progress1 {
            x: 0, y: 40
            progress: 0.25
            width: 350
            height: 20
            color: #0d6efd
        }

        ProgressBar progress2 {
            x: 0, y: 100
            progress: 0.65
            width: 350
            height: 20
            color: #ffc107
        }

        ProgressBar progress3 {
            x: 0, y: 160
            progress: 1.0
            width: 350
            height: 20
            color: #198754
        }
    }

    // Section 3: Nested Components (LabeledSlider)
    group nestedSection {
        x: 50, y: 400

        text sectionTitle {
            x: 0, y: 0
            content: "Nested Components (from DSL)"
            fontSize: 20
            color: #495057
        }

        LabeledSlider brightness {
            x: 0, y: 40
            label: "Brightness"
            value: 0.7
            width: 400
            color: #0d6efd
        }

        LabeledSlider contrast {
            x: 0, y: 120
            label: "Contrast"
            value: 0.5
            width: 400
            color: #198754
        }

        LabeledSlider saturation {
            x: 0, y: 200
            label: "Saturation"
            value: 0.9
            width: 400
            color: #ffc107
        }
    }

    // Section 4: Settings Rows
    group settingsSection {
        x: 600, y: 400

        text sectionTitle {
            x: 0, y: 0
            content: "Settings (from DSL)"
            fontSize: 20
            color: #495057
        }

        SettingsRow notifications {
            x: 0, y: 40
            label: "Enable Notifications"
            on: true
        }

        SettingsRow autosave {
            x: 0, y: 100
            label: "Auto-save"
            on: false
        }

        SettingsRow darkmode {
            x: 0, y: 160
            label: "Dark Mode"
            on: true
        }
    }

    // Footer
    text footer {
        x: 600, y: 770
        content: "All components defined in C++, used in .flex file | Pure declarative UI!"
        fontSize: 14
        color: #adb5bd
    }
}
