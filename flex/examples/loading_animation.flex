// Loading Animation Demo
// Multiple animated spinners and progress indicators

scene loading {
    width: 800
    height: 600

    // Gradient background
    rect background {
        x: 0, y: 0
        width: 800, height: 600
        fill: #0f0f23
    }

    // Title
    text title {
        x: 400, y: 50
        content: "LOADING ANIMATIONS"
        fontSize: 32
        color: #00d9ff
    }

    // Spinner 1: Classic rotating circle
    group spinner1 {
        x: 200, y: 200

        text label {
            x: 0, y: 100
            content: "Classic Spinner"
            fontSize: 16
            color: #888888
        }

        group rotator {
            x: 0, y: 0
            rotation: 0

            circle arc1 {
                x: 0, y: 0
                radius: 40
                stroke: #00d9ff
                strokeWidth: 6
            }

            circle arc2 {
                x: 0, y: 0
                radius: 30
                stroke: #ff006e
                strokeWidth: 4
            }

            circle arc3 {
                x: 0, y: 0
                radius: 20
                stroke: #8338ec
                strokeWidth: 3
            }
        }
    }

    // Spinner 2: Pulsing circles
    group spinner2 {
        x: 400, y: 200

        text label {
            x: 0, y: 100
            content: "Pulse"
            fontSize: 16
            color: #888888
        }

        circle pulse1 {
            x: 0, y: 0
            radius: 20
            fill: #00d9ff
            opacity: 1.0
        }

        circle pulse2 {
            x: 0, y: 0
            radius: 30
            stroke: #00d9ff
            strokeWidth: 3
            opacity: 0.5
        }

        circle pulse3 {
            x: 0, y: 0
            radius: 40
            stroke: #00d9ff
            strokeWidth: 2
            opacity: 0.2
        }
    }

    // Spinner 3: Dot wave
    group spinner3 {
        x: 600, y: 200

        text label {
            x: 0, y: 100
            content: "Wave"
            fontSize: 16
            color: #888888
        }

        circle dot1 {
            x: -30, y: 0
            radius: 8
            fill: #ff006e
        }

        circle dot2 {
            x: -10, y: 0
            radius: 8
            fill: #ff006e
        }

        circle dot3 {
            x: 10, y: 0
            radius: 8
            fill: #ff006e
        }

        circle dot4 {
            x: 30, y: 0
            radius: 8
            fill: #ff006e
        }
    }

    // Progress bar
    group progressBar {
        x: 400, y: 400

        text label {
            x: 0, y: -50
            content: "LOADING..."
            fontSize: 20
            color: #00d9ff
        }

        // Background track
        rect track {
            x: -200, y: -8
            width: 400, height: 16
            fill: #1a1a2e
        }

        // Progress fill
        rect progress {
            x: -200, y: -8
            width: 0, height: 16
            fill: #00d9ff
        }

        // Percentage text
        text percentage {
            x: 0, y: 50
            content: "0%"
            fontSize: 18
            color: #888888
        }
    }

    // Spinner 4: Orbiting dots
    group spinner4 {
        x: 200, y: 500

        text label {
            x: 0, y: 80
            content: "Orbit"
            fontSize: 16
            color: #888888
        }

        circle orbit1 {
            x: 0, y: -40
            radius: 10
            fill: #8338ec
        }

        circle orbit2 {
            x: 0, y: -40
            radius: 10
            fill: #ff006e
            rotation: 120
        }

        circle orbit3 {
            x: 0, y: -40
            radius: 10
            fill: #00d9ff
            rotation: 240
        }
    }

    // Spinner 5: Square rotation
    group spinner5 {
        x: 400, y: 500

        text label {
            x: 0, y: 80
            content: "Geometric"
            fontSize: 16
            color: #888888
        }

        group square {
            x: 0, y: 0
            rotation: 0

            rect outer {
                x: -30, y: -30
                width: 60, height: 60
                stroke: #00d9ff
                strokeWidth: 4
            }

            rect inner {
                x: -20, y: -20
                width: 40, height: 40
                fill: #ff006e
            }
        }
    }

    // Spinner 6: Bouncing bar
    group spinner6 {
        x: 600, y: 500

        text label {
            x: 0, y: 80
            content: "Bounce"
            fontSize: 16
            color: #888888
        }

        rect bar {
            x: -25, y: -40
            width: 50, height: 10
            fill: #8338ec
        }
    }
}

// Spinner 1: Continuous rotation
anim "Spin1" {
    duration: 2
    loop: loop

    track "rotation" {
        keyframe 0 -> 0
        keyframe 2 -> 360
    }
}

// Spinner 2: Pulsing circles (scale and opacity)
anim "Pulse2" {
    duration: 1.5
    loop: pingpong

    track "opacity" {
        keyframe 0 -> 1.0
        keyframe 1.5 -> 0.2
    }
}

// Spinner 3: Wave animation (y position)
anim "Wave3" {
    duration: 0.8
    loop: pingpong

    track "y" {
        keyframe 0 -> 0
        keyframe 0.8 -> -20
    }
}

// Progress bar: 0 to 100% in 5 seconds
anim "Progress" {
    duration: 5
    loop: loop

    track "width" {
        keyframe 0 -> 0
        keyframe 5 -> 400
    }
}

// Spinner 4: Orbit rotation
anim "Orbit4" {
    duration: 1.5
    loop: loop

    track "rotation" {
        keyframe 0 -> 0
        keyframe 1.5 -> 360
    }
}

// Spinner 5: Square rotation (slower)
anim "Square5" {
    duration: 3
    loop: loop

    track "rotation" {
        keyframe 0 -> 0
        keyframe 3 -> 360
    }
}

// Spinner 6: Bouncing bar (y position)
anim "Bounce6" {
    duration: 0.6
    loop: pingpong

    track "y" {
        keyframe 0 -> -40
        keyframe 0.6 -> 20
    }
}
