// Data Binding Demo
// Simple counter with reactive UI

scene counter {
    width: 400
    height: 300

    // Background
    rect background {
        x: 0, y: 0
        width: 400, height: 300
        fill: #1a1a2e
    }

    // Title
    text title {
        x: 200, y: 50
        content: "Data Binding Demo"
        fontSize: 24
        color: #00d9ff
    }

    // Counter display
    group counterDisplay {
        x: 200, y: 120

        // Background
        rect displayBg {
            x: -80, y: -30
            width: 160, height: 60
            fill: #2c2c54
        }

        // Counter value (will be bound to data)
        text counterValue {
            x: 0, y: 5
            content: "0"
            fontSize: 36
            color: #00ff88
        }
    }

    // Increment button
    group incrementButton {
        x: 130, y: 220

        rect bg {
            x: -40, y: -20
            width: 80, height: 40
            fill: #00d9ff
        }

        text label {
            x: 0, y: 5
            content: "+"
            fontSize: 24
            color: #ffffff
        }
    }

    // Decrement button
    group decrementButton {
        x: 270, y: 220

        rect bg {
            x: -40, y: -20
            width: 80, height: 40
            fill: #ff006e
        }

        text label {
            x: 0, y: 5
            content: "-"
            fontSize: 24
            color: #ffffff
        }
    }

    // Status text (color changes based on value)
    text statusText {
        x: 200, y: 270
        content: "Neutral"
        fontSize: 16
        color: #888888
    }
}
