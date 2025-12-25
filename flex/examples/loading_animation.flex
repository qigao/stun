// Loading Animation Demo
// Multiple animated spinners with actual animation bindings
// Demonstrates: animations, loop modes, track paths, value expressions

scene loading {
    width: 800
    height: 600

    rect background {
        x: 0, y: 0, width: 800, height: 600, fill: #0f0f23
    }

    text title {
        x: 800 / 2, y: 50
        content: "LOADING ANIMATIONS"
        fontSize: 32
        color: #00d9ff
    }

    // Spinner 1: Classic rotating rings (centered at 200, 200)
    group spinner1 {
        x: 200, y: 200

        text label { x: 0, y: 100, content: "Spinner", fontSize: 16, color: #888888 }

        group rotator {
            rotation: 0
            circle ring1 { radius: 40, stroke: #00d9ff, strokeWidth: 6 }
            circle ring2 { radius: 40 - 10, stroke: #ff006e, strokeWidth: 4 }
            circle ring3 { radius: 40 - 20, stroke: #8338ec, strokeWidth: 3 }
        }
    }

    // Spinner 2: Pulsing circle (centered at 400, 200)
    group spinner2 {
        x: 800 / 2, y: 200

        text label { x: 0, y: 100, content: "Pulse", fontSize: 16, color: #888888 }

        circle pulse {
            radius: 30
            fill: #00d9ff
            opacity: 1.0
        }
    }

    // Spinner 3: Bouncing dots (centered at 600, 200)
    group spinner3 {
        x: 600, y: 200

        text label { x: 0, y: 100, content: "Bounce", fontSize: 16, color: #888888 }

        // Dots evenly spaced: -30, -10, 10, 30 (spacing of 20)
        circle dot1 { x: 0 - 30, y: 0, radius: 8, fill: #ff006e }
        circle dot2 { x: 0 - 10, y: 0, radius: 8, fill: #ff006e }
        circle dot3 { x: 10, y: 0, radius: 8, fill: #ff006e }
        circle dot4 { x: 30, y: 0, radius: 8, fill: #ff006e }
    }

    // Progress bar (centered horizontally)
    group progressBar {
        x: 800 / 2, y: 350

        text label { x: 0, y: 0 - 50, content: "LOADING...", fontSize: 20, color: #00d9ff }

        // Track: 400px wide, centered
        rect trackBg { x: 0 - 200, y: 0 - 8, width: 400, height: 16, fill: #1a1a2e }
        rect progress { x: 0 - 200, y: 0 - 8, width: 0, height: 16, fill: #00d9ff }

        text percentage { x: 0, y: 50, content: "0%", fontSize: 18, color: #888888 }
    }

    // Spinner 4: Rotating square (y = 500)
    group spinner4 {
        x: 200, y: 500

        text label { x: 0, y: 80, content: "Geometric", fontSize: 16, color: #888888 }

        group square {
            rotation: 0
            // Outer square: 60x60, centered
            rect outer { x: 0 - 30, y: 0 - 30, width: 60, height: 60, stroke: #00d9ff, strokeWidth: 4 }
            // Inner square: 40x40, centered
            rect inner { x: 0 - 20, y: 0 - 20, width: 40, height: 40, fill: #ff006e }
        }
    }

    // Spinner 5: Fading circles (centered at 400, 500)
    group spinner5 {
        x: 800 / 2, y: 500

        text label { x: 0, y: 80, content: "Fade", fontSize: 16, color: #888888 }

        // Three circles with 25px spacing
        circle fade1 { x: 0 - 25, y: 0, radius: 12, fill: #8338ec, opacity: 1.0 }
        circle fade2 { x: 0, y: 0, radius: 12, fill: #8338ec, opacity: 0.7 }
        circle fade3 { x: 25, y: 0, radius: 12, fill: #8338ec, opacity: 0.4 }
    }

    // Spinner 6: Scale animation (centered at 600, 500)
    group spinner6 {
        x: 600, y: 500

        text label { x: 0, y: 80, content: "Scale", fontSize: 16, color: #888888 }

        circle scaler {
            radius: 25
            fill: #00ff88
            scale: 1.0
        }
    }
}

// Spinner 1: Continuous rotation
anim "spin" {
    duration: 2
    loop: loop
    track "#rotator/rotation" {
        keyframe 0 -> 0
        keyframe 2 -> 360
    }
}

// Spinner 2: Pulsing opacity
anim "pulse" {
    duration: 1.5
    loop: pingpong
    track "#pulse/opacity" {
        keyframe 0 -> 1.0
        keyframe 1.5 -> 0.3
    }
}

// Spinner 3: Bouncing dots (staggered)
anim "bounce1" {
    duration: 0.6
    loop: pingpong
    track "#dot1/y" {
        keyframe 0 -> 0
        keyframe 0.3 -> -20
        keyframe 0.6 -> 0
    }
}

anim "bounce2" {
    duration: 0.6
    loop: pingpong
    track "#dot2/y" {
        keyframe 0 -> 0
        keyframe 0.15 -> 0
        keyframe 0.45 -> -20
        keyframe 0.6 -> 0
    }
}

anim "bounce3" {
    duration: 0.6
    loop: pingpong
    track "#dot3/y" {
        keyframe 0 -> 0
        keyframe 0.3 -> 0
        keyframe 0.6 -> -20
    }
}

anim "bounce4" {
    duration: 0.6
    loop: pingpong
    track "#dot4/y" {
        keyframe 0 -> 0
        keyframe 0.45 -> 0
        keyframe 0.6 -> -15
    }
}

// Progress bar animation
anim "progressFill" {
    duration: 5
    loop: loop
    track "#progress/width" {
        keyframe 0 -> 0
        keyframe 5 -> 400
    }
}

// Spinner 4: Square rotation
anim "squareSpin" {
    duration: 3
    loop: loop
    track "#square/rotation" {
        keyframe 0 -> 0
        keyframe 3 -> 360
    }
}

// Spinner 5: Fading circles (wave effect)
anim "fadeWave1" {
    duration: 1.2
    loop: loop
    track "#fade1/opacity" {
        keyframe 0 -> 1.0
        keyframe 0.4 -> 0.2
        keyframe 0.8 -> 0.2
        keyframe 1.2 -> 1.0
    }
}

anim "fadeWave2" {
    duration: 1.2
    loop: loop
    track "#fade2/opacity" {
        keyframe 0 -> 0.7
        keyframe 0.4 -> 1.0
        keyframe 0.8 -> 0.2
        keyframe 1.2 -> 0.7
    }
}

anim "fadeWave3" {
    duration: 1.2
    loop: loop
    track "#fade3/opacity" {
        keyframe 0 -> 0.4
        keyframe 0.4 -> 0.7
        keyframe 0.8 -> 1.0
        keyframe 1.2 -> 0.4
    }
}

// Spinner 6: Scale breathing
anim "scalePulse" {
    duration: 1.0
    loop: pingpong
    track "#scaler/scale" {
        keyframe 0 -> 1.0
        keyframe 0.5 -> 1.5
        keyframe 1.0 -> 1.0
    }
}
