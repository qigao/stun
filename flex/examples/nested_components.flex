// Nested Components Demo - Component Composition in DSL
// Demonstrates using C++ registered components directly in .flex files
// Components are nested: LabeledSlider contains Slider, SettingsRow contains Toggle, etc.

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
        content: "Components used directly in DSL"
        fontSize: 16
        color: #6c757d
    }

    // ========================================================================
    // Section 1: Basic Components (Slider, Toggle, ProgressBar)
    // ========================================================================
    group section1 {
        x: 50, y: 120

        text sectionTitle {
            x: 0, y: 0
            content: "Basic Components"
            fontSize: 20
            color: #495057
        }

        // Basic Slider
        Slider basicSlider {
            x: 0, y: 40
            value: 0.65
            width: 400
            color: #0D6EFD
        }

        // Basic Toggle
        Toggle basicToggle {
            x: 0, y: 80
            on: true
            width: 50
            height: 26
            color: #198754
        }

        // Basic ProgressBar
        ProgressBar basicProgress {
            x: 0, y: 120
            progress: 0.75
            width: 400
            height: 20
            color: #198754
        }
    }

    // ========================================================================
    // Section 2: Nested Components - LabeledSlider
    // LabeledSlider = Text + Slider (nested component)
    // ========================================================================
    group section2 {
        x: 50, y: 300

        text sectionTitle {
            x: 0, y: 0
            content: "LabeledSlider (Label + Slider + Value)"
            fontSize: 20
            color: #495057
        }

        // LabeledSlider #1
        LabeledSlider volume {
            x: 0, y: 40
            label: "Volume"
            value: 0.65
            width: 400
            color: #0D6EFD
        }

        // LabeledSlider #2
        LabeledSlider brightness {
            x: 0, y: 120
            label: "Brightness"
            value: 0.80
            width: 400
            color: #FFC107
        }

        // LabeledSlider #3
        LabeledSlider contrast {
            x: 0, y: 200
            label: "Contrast"
            value: 0.50
            width: 400
            color: #6C757D
        }
    }

    // ========================================================================
    // Section 3: Nested Components - SettingsRow
    // SettingsRow = Text + Toggle (nested component)
    // ========================================================================
    group section3 {
        x: 650, y: 120

        text sectionTitle {
            x: 0, y: 0
            content: "SettingsRow (Label + Toggle)"
            fontSize: 20
            color: #495057
        }

        // SettingsRow #1
        SettingsRow darkMode {
            x: 0, y: 40
            label: "Dark Mode"
            on: false
        }

        // SettingsRow #2
        SettingsRow notifications {
            x: 0, y: 100
            label: "Notifications"
            on: true
        }

        // SettingsRow #3
        SettingsRow autoSave {
            x: 0, y: 160
            label: "Auto Save"
            on: true
        }
    }

    // ========================================================================
    // Section 4: Complex Nested Components - VolumeControl
    // VolumeControl = Slider + ProgressBar + Toggle (multiple nested components)
    // ========================================================================
    group section4 {
        x: 50, y: 580

        text sectionTitle {
            x: 0, y: 0
            content: "VolumeControl (Slider + ProgressBar + Toggle)"
            fontSize: 20
            color: #495057
        }

        text description {
            x: 0, y: 25
            content: "This component nests multiple base components"
            fontSize: 12
            color: #6c757d
        }

        // VolumeControl combines Slider, ProgressBar, and Toggle
        VolumeControl audioMaster {
            x: 0, y: 60
            volume: 0.75
            muted: false
        }
    }

    // ========================================================================
    // Section 5: Composition - SettingsPanel
    // SettingsPanel = Multiple SettingsRow components (component composition)
    // ========================================================================
    group section5 {
        x: 650, y: 350

        text sectionTitle {
            x: 0, y: 0
            content: "SettingsPanel (Multiple Components)"
            fontSize: 20
            color: #495057
        }

        text description {
            x: 0, y: 25
            content: "Composition of multiple SettingsRow components"
            fontSize: 12
            color: #6c757d
        }

        // Panel background
        rect panelBg {
            x: 0, y: 50
            width: 500, height: 280
            fill: #ffffff
        }

        // SettingsPanel - groups multiple SettingsRow components
        group settingsPanel {
            x: 20, y: 70

            text panelTitle {
                x: 0, y: 0
                content: "Application Settings"
                fontSize: 16
                color: #212529
            }

            SettingsRow s1 {
                x: 0, y: 40
                label: "Enable HDR"
                on: true
            }

            SettingsRow s2 {
                x: 0, y: 100
                label: "V-Sync"
                on: false
            }

            SettingsRow s3 {
                x: 0, y: 160
                label: "Show FPS"
                on: true
            }

            SettingsRow s4 {
                x: 0, y: 220
                label: "Full Screen"
                on: false
            }
        }
    }

    // Footer
    text footer {
        x: 600, y: 770
        content: "All components built from base widgets | Composed in DSL"
        fontSize: 14
        color: #adb5bd
    }
}
