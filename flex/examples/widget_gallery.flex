// Widget Gallery - Standard UI Component Library
// Demonstrates Slider, ProgressBar, Checkbox, Radio, Toggle

scene widgetGallery {
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
        content: "Flex Widget Library"
        fontSize: 32
        color: #212529
    }

    text subtitle {
        x: 600, y: 75
        content: "Standard UI Components - Built with Component System"
        fontSize: 16
        color: #6c757d
    }

    // Section 1: Sliders
    group sliderSection {
        x: 50, y: 120

        text sectionTitle {
            x: 0, y: 0
            content: "Sliders"
            fontSize: 20
            color: #495057
        }

        // Sliders will be created programmatically at:
        // (0, 40), (0, 100), (0, 160)
    }

    // Section 2: Progress Bars
    group progressSection {
        x: 450, y: 120

        text sectionTitle {
            x: 0, y: 0
            content: "Progress Bars"
            fontSize: 20
            color: #495057
        }

        // Progress bars at (0, 40), (0, 100), (0, 160)
    }

    // Section 3: Checkboxes
    group checkboxSection {
        x: 850, y: 120

        text sectionTitle {
            x: 0, y: 0
            content: "Checkboxes"
            fontSize: 20
            color: #495057
        }

        // Checkboxes at (0, 40), (0, 90), (0, 140)
    }

    // Section 4: Radio Buttons
    group radioSection {
        x: 50, y: 400

        text sectionTitle {
            x: 0, y: 0
            content: "Radio Buttons"
            fontSize: 20
            color: #495057
        }

        // Radios at (0, 40), (0, 90), (0, 140)
    }

    // Section 5: Toggles
    group toggleSection {
        x: 450, y: 400

        text sectionTitle {
            x: 0, y: 0
            content: "Toggle Switches"
            fontSize: 20
            color: #495057
        }

        // Toggles at (0, 40), (0, 100), (0, 160)
    }

    // Section 6: Combined Demo
    group demoSection {
        x: 50, y: 620

        text sectionTitle {
            x: 0, y: 0
            content: "Interactive Demo"
            fontSize: 20
            color: #495057
        }

        text demoLabel {
            x: 0, y: 40
            content: "Volume:"
            fontSize: 16
            color: #495057
        }

        // Volume slider at (100, 35)

        text volumeValue {
            x: 450, y: 40
            content: "50%"
            fontSize: 16
            color: #0d6efd
        }

        // Volume progress bar at (100, 80)

        text muteLabel {
            x: 0, y: 130
            content: "Muted:"
            fontSize: 16
            color: #495057
        }

        // Mute toggle at (100, 125)
    }

    // Footer
    text footer {
        x: 600, y: 770
        content: "🖱️ Click and drag widgets | ⌨️ UP/DOWN:volume SPACE:mute ESC:quit"
        fontSize: 14
        color: #adb5bd
    }
}
