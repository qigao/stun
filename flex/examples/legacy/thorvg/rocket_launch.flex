// Rocket Launch Demo
// Demonstrates path animation with rocket following a launch trajectory

scene launch {
    width: 800
    height: 600

    // Background - Sky
    rect sky {
        x: 0, y: 0
        width: 800, height: 600
        fill: #0a1128
    }

    // Stars
    circle star1 { x: 100, y: 100, radius: 2, fill: #ffffff }
    circle star2 { x: 250, y: 150, radius: 2, fill: #ffffff }
    circle star3 { x: 400, y: 80, radius: 2, fill: #ffffff }
    circle star4 { x: 600, y: 120, radius: 2, fill: #ffffff }
    circle star5 { x: 700, y: 200, radius: 2, fill: #ffffff }
    circle star6 { x: 150, y: 300, radius: 2, fill: #ffffff }
    circle star7 { x: 500, y: 250, radius: 2, fill: #ffffff }
    circle star8 { x: 350, y: 400, radius: 2, fill: #ffffff }

    // Launch pad
    rect pad {
        x: 80, y: 520
        width: 100, height: 80
        fill: #555555
    }

    // Rocket (will follow path)
    group rocket {
        x: 130, y: 500

        // Rocket body
        rect body {
            x: -15, y: -40
            width: 30, height: 60
            fill: #ff006e
        }

        // Rocket nose cone
        rect nose {
            x: -12, y: -50
            width: 24, height: 10
            fill: #c9184a
        }

        // Left fin
        rect finLeft {
            x: -25, y: 10
            width: 10, height: 20
            fill: #00d9ff
        }

        // Right fin
        rect finRight {
            x: 15, y: 10
            width: 10, height: 20
            fill: #00d9ff
        }

        // Window
        circle window {
            x: 0, y: -20
            radius: 6
            fill: #90e0ef
        }

        // Exhaust flame
        circle flame1 {
            x: 0, y: 25
            radius: 8
            fill: #ff9500
            opacity: 0.0
        }

        circle flame2 {
            x: 0, y: 35
            radius: 6
            fill: #ffaa00
            opacity: 0.0
        }
    }

    // Title
    text title {
        x: 400, y: 50
        content: "Rocket Launch - Path Animation"
        fontSize: 24
        color: #ffffff
    }

    // Instructions
    text instructions {
        x: 400, y: 580
        content: "Press SPACE to launch | R to reset | ESC to quit"
        fontSize: 14
        color: #888888
    }

    // Path progress indicator
    group progress {
        x: 50, y: 50

        text progressLabel {
            x: 0, y: 0
            content: "Progress:"
            fontSize: 14
            color: #888888
        }

        text progressValue {
            x: 80, y: 0
            content: "0%"
            fontSize: 14
            color: #00d9ff
        }
    }

    // Altitude indicator
    group altitude {
        x: 50, y: 80

        text altitudeLabel {
            x: 0, y: 0
            content: "Altitude:"
            fontSize: 14
            color: #888888
        }

        text altitudeValue {
            x: 80, y: 0
            content: "0 km"
            fontSize: 14
            color: #00d9ff
        }
    }

    // Speed indicator
    group speed {
        x: 50, y: 110

        text speedLabel {
            x: 0, y: 0
            content: "Speed:"
            fontSize: 14
            color: #888888
        }

        text speedValue {
            x: 80, y: 0
            content: "0 km/s"
            fontSize: 14
            color: #00d9ff
        }
    }
}
