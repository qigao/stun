// Tesla Speedometer - Modern Minimalist Design
// Speed range: 0-200 km/h

scene speedometer {
    width: 800
    height: 600

    // Dark background (Tesla style)
    rect background {
        x: 0, y: 0
        width: 800, height: 600
        fill: #0a0a0a
    }

    // Main speedometer group
    group meter {
        x: 400, y: 350

        // Outer ring (subtle border)
        circle outerRing {
            x: 0, y: 0
            radius: 220
            fill: #1a1a1a
        }

        // Inner background
        circle innerBg {
            x: 0, y: 0
            radius: 210
            fill: #0f0f0f
        }

        // Speed range arc indicator (background)
        circle arcBg {
            x: 0, y: 0
            radius: 180
            stroke: #2a2a2a
            strokeWidth: 8
        }

        // Active speed arc (will be animated)
        circle activeArc {
            x: 0, y: 0
            radius: 180
            stroke: #00ff88
            strokeWidth: 8
        }

        // Tick marks group (0-200 km/h, every 20)
        group ticks {
            x: 0, y: 0

            // We'll create major ticks at 0, 20, 40...200
            // Positioned in a circle

            // 0 km/h (bottom left, -135 deg)
            rect tick0 {
                x: -127, y: 127
                width: 30, height: 3
                fill: #666666
                rotation: -45
            }

            // 20 km/h
            rect tick20 {
                x: -168, y: 60
                width: 30, height: 3
                fill: #666666
                rotation: -72
            }

            // 40 km/h
            rect tick40 {
                x: -180, y: -15
                width: 30, height: 3
                fill: #666666
                rotation: -99
            }

            // 60 km/h
            rect tick60 {
                x: -155, y: -85
                width: 30, height: 3
                fill: #666666
                rotation: -126
            }

            // 80 km/h
            rect tick80 {
                x: -100, y: -140
                width: 30, height: 3
                fill: #666666
                rotation: -153
            }

            // 100 km/h (top, -180 deg)
            rect tick100 {
                x: -15, y: -165
                width: 40, height: 4
                fill: #00ff88
                rotation: 180
            }

            // 120 km/h
            rect tick120 {
                x: 70, y: -155
                width: 30, height: 3
                fill: #00ff88
                rotation: 153
            }

            // 140 km/h
            rect tick140 {
                x: 130, y: -115
                width: 30, height: 3
                fill: #00ff88
                rotation: 126
            }

            // 160 km/h
            rect tick160 {
                x: 165, y: -50
                width: 30, height: 3
                fill: #ffa500
                rotation: 99
            }

            // 180 km/h
            rect tick180 {
                x: 168, y: 30
                width: 30, height: 3
                fill: #ff4444
                rotation: 72
            }

            // 200 km/h (bottom right, 135 deg)
            rect tick200 {
                x: 135, y: 110
                width: 30, height: 3
                fill: #ff4444
                rotation: 45
            }
        }

        // Speed labels
        group labels {
            x: 0, y: 0

            text label0 {
                x: -110, y: 110
                content: "0"
                fontSize: 18
                color: #666666
            }

            text label100 {
                x: -8, y: -140
                content: "100"
                fontSize: 20
                color: #00ff88
            }

            text label200 {
                x: 110, y: 100
                content: "200"
                fontSize: 18
                color: #ff4444
            }
        }

        // Center needle/pointer
        group needle {
            x: 0, y: 0
            rotation: -135

            // Needle shaft
            rect shaft {
                x: -3, y: -150
                width: 6, height: 150
                fill: #00ff88
            }

            // Needle tip (triangle effect)
            rect tip {
                x: -5, y: -160
                width: 10, height: 15
                fill: #00ff88
            }

            // Center hub
            circle hub {
                x: 0, y: 0
                radius: 15
                fill: #00ff88
            }

            // Inner hub detail
            circle innerHub {
                x: 0, y: 0
                radius: 8
                fill: #0f0f0f
            }
        }

        // Digital speed display
        group digitalDisplay {
            x: 0, y: 80

            // Background panel
            rect panel {
                x: -60, y: -25
                width: 120, height: 50
                fill: #1a1a1a
            }

            // Speed number
            text speedNumber {
                x: 0, y: 10
                content: "0"
                fontSize: 48
                color: #00ff88
            }

            // Unit label
            text unit {
                x: 0, y: 35
                content: "km/h"
                fontSize: 14
                color: #666666
            }
        }

        // Tesla logo area
        text brand {
            x: 0, y: -80
            content: "TESLA"
            fontSize: 16
            color: #444444
        }
    }

    // Status indicators (top)
    group statusBar {
        x: 400, y: 50

        text mode {
            x: -200, y: 0
            content: "SPORT"
            fontSize: 20
            color: #00ff88
        }

        text battery {
            x: 200, y: 0
            content: "85%"
            fontSize: 20
            color: #00aaff
        }
    }
}

// Speed up animation (0 to 120 km/h in 3 seconds)
anim "Accelerate" {
    duration: 3
    loop: once

    // Needle rotation: -135 deg (0 km/h) to +27 deg (120 km/h)
    // Total range: 270 degrees for 0-200 km/h
    // 1 km/h = 1.35 degrees
    track "rotation" {
        keyframe 0 -> -135
        keyframe 3 -> 27
    }
}

// Smooth oscillation (idle)
anim "Idle" {
    duration: 2
    loop: pingpong

    track "rotation" {
        keyframe 0 -> -135
        keyframe 2 -> -130
    }
}
