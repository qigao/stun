// UI Components Gallery
// Showcase of interactive UI elements

scene components {
    width: 800
    height: 900

    // Background
    rect background {
        x: 0, y: 0
        width: 800, height: 900
        fill: #f5f5f5
    }

    // Title
    text title {
        x: 400, y: 40
        content: "UI COMPONENTS GALLERY"
        fontSize: 28
        color: #2c2c54
    }

    // === BUTTONS SECTION ===
    text buttonSectionTitle {
        x: 50, y: 100
        content: "Buttons"
        fontSize: 20
        color: #666666
    }

    // Primary button
    group primaryButton {
        x: 100, y: 150

        rect bg {
            x: -60, y: -20
            width: 120, height: 40
            fill: #00d9ff
        }

        text label {
            x: 0, y: 5
            content: "Primary"
            fontSize: 16
            color: #ffffff
        }
    }

    // Secondary button
    group secondaryButton {
        x: 280, y: 150

        rect bg {
            x: -60, y: -20
            width: 120, height: 40
            fill: #8338ec
        }

        text label {
            x: 0, y: 5
            content: "Secondary"
            fontSize: 16
            color: #ffffff
        }
    }

    // Danger button
    group dangerButton {
        x: 460, y: 150

        rect bg {
            x: -60, y: -20
            width: 120, height: 40
            fill: #ff006e
        }

        text label {
            x: 0, y: 5
            content: "Danger"
            fontSize: 16
            color: #ffffff
        }
    }

    // Outlined button
    group outlinedButton {
        x: 640, y: 150

        rect bg {
            x: -60, y: -20
            width: 120, height: 40
            stroke: #00d9ff
            strokeWidth: 2
        }

        text label {
            x: 0, y: 5
            content: "Outlined"
            fontSize: 16
            color: #00d9ff
        }
    }

    // === SLIDERS SECTION ===
    text sliderSectionTitle {
        x: 50, y: 240
        content: "Sliders"
        fontSize: 20
        color: #666666
    }

    // Slider 1
    group slider1 {
        x: 100, y: 290

        text label {
            x: -50, y: -15
            content: "Volume: 75%"
            fontSize: 14
            color: #666666
        }

        rect track {
            x: 0, y: 0
            width: 200, height: 8
            fill: #cccccc
        }

        rect fill {
            x: 0, y: 0
            width: 150, height: 8
            fill: #00d9ff
        }

        circle thumb {
            x: 150, y: 4
            radius: 12
            fill: #00d9ff
        }
    }

    // Slider 2
    group slider2 {
        x: 450, y: 290

        text label {
            x: -50, y: -15
            content: "Brightness: 50%"
            fontSize: 14
            color: #666666
        }

        rect track {
            x: 0, y: 0
            width: 200, height: 8
            fill: #cccccc
        }

        rect fill {
            x: 0, y: 0
            width: 100, height: 8
            fill: #8338ec
        }

        circle thumb {
            x: 100, y: 4
            radius: 12
            fill: #8338ec
        }
    }

    // === CHECKBOXES SECTION ===
    text checkboxSectionTitle {
        x: 50, y: 360
        content: "Checkboxes"
        fontSize: 20
        color: #666666
    }

    // Checkbox 1 (checked)
    group checkbox1 {
        x: 100, y: 410

        rect box {
            x: 0, y: 0
            width: 24, height: 24
            fill: #00d9ff
        }

        rect checkmark {
            x: 6, y: 10
            width: 12, height: 6
            fill: #ffffff
        }

        text label {
            x: 40, y: 16
            content: "Enable notifications"
            fontSize: 16
            color: #2c2c54
        }
    }

    // Checkbox 2 (unchecked)
    group checkbox2 {
        x: 100, y: 460

        rect box {
            x: 0, y: 0
            width: 24, height: 24
            stroke: #cccccc
            strokeWidth: 2
        }

        text label {
            x: 40, y: 16
            content: "Subscribe to newsletter"
            fontSize: 16
            color: #2c2c54
        }
    }

    // Checkbox 3 (checked)
    group checkbox3 {
        x: 100, y: 510

        rect box {
            x: 0, y: 0
            width: 24, height: 24
            fill: #8338ec
        }

        rect checkmark {
            x: 6, y: 10
            width: 12, height: 6
            fill: #ffffff
        }

        text label {
            x: 40, y: 16
            content: "Accept terms and conditions"
            fontSize: 16
            color: #2c2c54
        }
    }

    // === RADIO BUTTONS SECTION ===
    text radioSectionTitle {
        x: 450, y: 360
        content: "Radio Buttons"
        fontSize: 20
        color: #666666
    }

    // Radio 1 (selected)
    group radio1 {
        x: 500, y: 410

        circle outer {
            x: 12, y: 12
            radius: 12
            stroke: #00d9ff
            strokeWidth: 2
        }

        circle inner {
            x: 12, y: 12
            radius: 6
            fill: #00d9ff
        }

        text label {
            x: 40, y: 16
            content: "Option A"
            fontSize: 16
            color: #2c2c54
        }
    }

    // Radio 2 (unselected)
    group radio2 {
        x: 500, y: 460

        circle outer {
            x: 12, y: 12
            radius: 12
            stroke: #cccccc
            strokeWidth: 2
        }

        text label {
            x: 40, y: 16
            content: "Option B"
            fontSize: 16
            color: #2c2c54
        }
    }

    // Radio 3 (unselected)
    group radio3 {
        x: 500, y: 510

        circle outer {
            x: 12, y: 12
            radius: 12
            stroke: #cccccc
            strokeWidth: 2
        }

        text label {
            x: 40, y: 16
            content: "Option C"
            fontSize: 16
            color: #2c2c54
        }
    }

    // === PROGRESS BARS SECTION ===
    text progressSectionTitle {
        x: 50, y: 590
        content: "Progress Bars"
        fontSize: 20
        color: #666666
    }

    // Progress bar 1
    group progress1 {
        x: 100, y: 640

        text label {
            x: -50, y: -15
            content: "Uploading... 65%"
            fontSize: 14
            color: #666666
        }

        rect track {
            x: 0, y: 0
            width: 300, height: 12
            fill: #e0e0e0
        }

        rect fill {
            x: 0, y: 0
            width: 195, height: 12
            fill: #00d9ff
        }
    }

    // Progress bar 2
    group progress2 {
        x: 100, y: 710

        text label {
            x: -50, y: -15
            content: "Processing... 40%"
            fontSize: 14
            color: #666666
        }

        rect track {
            x: 0, y: 0
            width: 300, height: 12
            fill: #e0e0e0
        }

        rect fill {
            x: 0, y: 0
            width: 120, height: 12
            fill: #8338ec
        }
    }

    // === TOGGLE SWITCH SECTION ===
    text toggleSectionTitle {
        x: 450, y: 590
        content: "Toggle Switches"
        fontSize: 20
        color: #666666
    }

    // Toggle 1 (ON)
    group toggle1 {
        x: 500, y: 640

        rect track {
            x: 0, y: 0
            width: 50, height: 26
            fill: #00d9ff
        }

        circle thumb {
            x: 30, y: 13
            radius: 10
            fill: #ffffff
        }

        text label {
            x: 70, y: 16
            content: "Dark Mode: ON"
            fontSize: 16
            color: #2c2c54
        }
    }

    // Toggle 2 (OFF)
    group toggle2 {
        x: 500, y: 700

        rect track {
            x: 0, y: 0
            width: 50, height: 26
            fill: #cccccc
        }

        circle thumb {
            x: 20, y: 13
            radius: 10
            fill: #ffffff
        }

        text label {
            x: 70, y: 16
            content: "Auto-save: OFF"
            fontSize: 16
            color: #2c2c54
        }
    }

    // === DIALOG BOX ===
    text dialogSectionTitle {
        x: 50, y: 780
        content: "Dialog Box"
        fontSize: 20
        color: #666666
    }

    group dialog {
        x: 400, y: 850

        // Shadow
        rect shadow {
            x: -152, y: -42
            width: 304, height: 84
            fill: #000000
            opacity: 0.1
        }

        // Dialog background
        rect bg {
            x: -150, y: -40
            width: 300, height: 80
            fill: #ffffff
        }

        // Border
        rect border {
            x: -150, y: -40
            width: 300, height: 80
            stroke: #cccccc
            strokeWidth: 2
        }

        // Icon
        circle icon {
            x: -110, y: 0
            radius: 16
            fill: #00d9ff
        }

        // Message
        text message {
            x: -40, y: 5
            content: "Are you sure?"
            fontSize: 16
            color: #2c2c54
        }

        // OK button
        rect okButton {
            x: 40, y: -12
            width: 50, height: 30
            fill: #00d9ff
        }

        text okLabel {
            x: 65, y: 5
            content: "OK"
            fontSize: 14
            color: #ffffff
        }

        // Cancel button
        rect cancelButton {
            x: 100, y: -12
            width: 50, height: 30
            fill: #cccccc
        }

        text cancelLabel {
            x: 125, y: 5
            content: "Cancel"
            fontSize: 14
            color: #666666
        }
    }
}

// Button hover animation
anim "ButtonHover" {
    duration: 0.2
    loop: once

    track "scaleX" {
        keyframe 0 -> 1.0
        keyframe 0.2 -> 1.05
    }

    track "scaleY" {
        keyframe 0 -> 1.0
        keyframe 0.2 -> 1.05
    }
}

// Progress bar animation
anim "ProgressAnimation" {
    duration: 3
    loop: loop

    track "width" {
        keyframe 0 -> 0
        keyframe 3 -> 300
    }
}
