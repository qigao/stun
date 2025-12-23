// UI Components Gallery

scene components {
    width: 800
    height: 900

    rect background {
        x: 0, y: 0
        width: 800, height: 900
        fill: #f5f5f5
    }

    text title {
        x: 400, y: 40
        content: "UI COMPONENTS GALLERY"
        fontSize: 28
        color: #2c2c54
    }

    text buttonSectionTitle {
        x: 50, y: 100
        content: "Buttons"
        fontSize: 20
        color: #666666
    }

    group primaryButton {
        x: 100, y: 150

        rect bg {
            x: 0, y: 0
            width: 120, height: 40
            fill: #00d9ff
        }

        text label {
            x: 60, y: 25
            content: "Primary"
            fontSize: 16
            color: #ffffff
        }
    }

    group secondaryButton {
        x: 280, y: 150

        rect bg {
            x: 0, y: 0
            width: 120, height: 40
            fill: #8338ec
        }

        text label {
            x: 60, y: 25
            content: "Secondary"
            fontSize: 16
            color: #ffffff
        }
    }

    group dangerButton {
        x: 460, y: 150

        rect bg {
            x: 0, y: 0
            width: 120, height: 40
            fill: #ff006e
        }

        text label {
            x: 60, y: 25
            content: "Danger"
            fontSize: 16
            color: #ffffff
        }
    }

    text progressSectionTitle {
        x: 50, y: 240
        content: "Progress Bars"
        fontSize: 20
        color: #666666
    }

    group progress1 {
        x: 100, y: 290

        text label {
            x: 0, y: 0
            content: "Uploading 65"
            fontSize: 14
            color: #666666
        }

        rect trackBar {
            x: 0, y: 20
            width: 600, height: 12
            fill: #e0e0e0
        }

        rect fill {
            x: 0, y: 20
            width: 390, height: 12
            fill: #00d9ff
        }
    }

    group progress2 {
        x: 100, y: 370

        text label {
            x: 0, y: 0
            content: "Processing 40"
            fontSize: 14
            color: #666666
        }

        rect trackBar {
            x: 0, y: 20
            width: 600, height: 12
            fill: #e0e0e0
        }

        rect fill {
            x: 0, y: 20
            width: 240, height: 12
            fill: #8338ec
        }
    }

    text toggleSectionTitle {
        x: 50, y: 460
        content: "Toggle Switches"
        fontSize: 20
        color: #666666
    }

    group toggle1 {
        x: 100, y: 510

        rect trackBar {
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
            content: "Dark Mode ON"
            fontSize: 16
            color: #2c2c54
        }
    }

    group toggle2 {
        x: 100, y: 570

        rect trackBar {
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
            content: "Auto save OFF"
            fontSize: 16
            color: #2c2c54
        }
    }

    text checkboxSectionTitle {
        x: 50, y: 630
        content: "Checkboxes"
        fontSize: 20
        color: #666666
    }

    group checkbox1 {
        x: 100, y: 680

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

    group checkbox2 {
        x: 100, y: 730

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
}
