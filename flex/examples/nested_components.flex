// Nested Components Demo - Component Composition
// Demonstrates building complex UIs from simple components

scene nestedComponentsDemo {
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
        content: "Nested Components Demo"
        fontSize: 32
        color: #212529
    }

    text subtitle {
        x: 600, y: 75
        content: "Building Complex UIs from Simple Widgets"
        fontSize: 16
        color: #6c757d
    }

    // Section 1: LabeledSlider Components
    group section1 {
        x: 50, y: 120

        text sectionTitle {
            x: 0, y: 0
            content: "LabeledSlider (Label + Slider + Value)"
            fontSize: 20
            color: #495057
        }

        // LabeledSlider instances will be created programmatically at:
        // (0, 40), (0, 120), (0, 200)
    }

    // Section 2: SettingsRow Components
    group section2 {
        x: 650, y: 120

        text sectionTitle {
            x: 0, y: 0
            content: "SettingsRow (Label + Toggle)"
            fontSize: 20
            color: #495057
        }

        // SettingsRow instances at (0, 40), (0, 100), (0, 160)
    }

    // Section 3: VolumeControl Component
    group section3 {
        x: 50, y: 450

        text sectionTitle {
            x: 0, y: 0
            content: "VolumeControl (Slider + ProgressBar + Toggle)"
            fontSize: 20
            color: #495057
        }

        // VolumeControl at (0, 40)
    }

    // Section 4: Complete SettingsPanel
    group section4 {
        x: 650, y: 450

        text sectionTitle {
            x: 0, y: 0
            content: "SettingsPanel (Multiple Components)"
            fontSize: 20
            color: #495057
        }

        // SettingsPanel at (0, 40)
    }

    // Footer
    text footer {
        x: 600, y: 770
        content: "All nested components are built from base widgets | Click to interact"
        fontSize: 14
        color: #adb5bd
    }
}
