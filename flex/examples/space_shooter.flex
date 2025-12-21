// Space Shooter Game
// Classic vertical scrolling shooter

scene game {
    width: 400
    height: 700

    // Space background
    rect background {
        x: 0, y: 0
        width: 400, height: 700
        fill: #0a0a1a
    }

    // Stars (parallax effect)
    group stars {
        x: 0, y: 0

        circle star1 { x: 50, y: 100, radius: 2, fill: #ffffff }
        circle star2 { x: 150, y: 200, radius: 1, fill: #cccccc }
        circle star3 { x: 300, y: 150, radius: 2, fill: #ffffff }
        circle star4 { x: 100, y: 400, radius: 1, fill: #cccccc }
        circle star5 { x: 250, y: 500, radius: 2, fill: #ffffff }
        circle star6 { x: 350, y: 300, radius: 1, fill: #cccccc }
        circle star7 { x: 80, y: 600, radius: 2, fill: #ffffff }
        circle star8 { x: 200, y: 50, radius: 1, fill: #cccccc }
    }

    // UI - Score
    text scoreLabel {
        x: 20, y: 25
        content: "SCORE:"
        fontSize: 16
        color: #888888
    }

    text scoreValue {
        x: 100, y: 25
        content: "0"
        fontSize: 16
        color: #00d9ff
    }

    // UI - Health
    text healthLabel {
        x: 280, y: 25
        content: "HP:"
        fontSize: 16
        color: #888888
    }

    group healthBar {
        x: 320, y: 20

        rect healthBg {
            x: 0, y: 0
            width: 60, height: 10
            fill: #2c2c54
        }

        rect healthFill {
            x: 0, y: 0
            width: 60, height: 10
            fill: #00ff88
        }
    }

    // Player ship
    group player {
        x: 200, y: 600

        // Ship body (triangle)
        rect body {
            x: -15, y: -20
            width: 30, height: 40
            fill: #00d9ff
        }

        // Wings
        rect leftWing {
            x: -25, y: 0
            width: 10, height: 20
            fill: #8338ec
        }

        rect rightWing {
            x: 15, y: 0
            width: 10, height: 20
            fill: #8338ec
        }

        // Cockpit
        circle cockpit {
            x: 0, y: -10
            radius: 6
            fill: #ff006e
        }

        // Engine glow
        circle engineGlow {
            x: 0, y: 20
            radius: 8
            fill: #ff6b35
            opacity: 0.8
        }
    }

    // Player bullets (pool of 10)
    group bullets {
        x: 0, y: 0

        rect bullet0 { x: 0, y: -100, width: 4, height: 12, fill: #00ff88, opacity: 0.0 }
        rect bullet1 { x: 0, y: -100, width: 4, height: 12, fill: #00ff88, opacity: 0.0 }
        rect bullet2 { x: 0, y: -100, width: 4, height: 12, fill: #00ff88, opacity: 0.0 }
        rect bullet3 { x: 0, y: -100, width: 4, height: 12, fill: #00ff88, opacity: 0.0 }
        rect bullet4 { x: 0, y: -100, width: 4, height: 12, fill: #00ff88, opacity: 0.0 }
        rect bullet5 { x: 0, y: -100, width: 4, height: 12, fill: #00ff88, opacity: 0.0 }
        rect bullet6 { x: 0, y: -100, width: 4, height: 12, fill: #00ff88, opacity: 0.0 }
        rect bullet7 { x: 0, y: -100, width: 4, height: 12, fill: #00ff88, opacity: 0.0 }
        rect bullet8 { x: 0, y: -100, width: 4, height: 12, fill: #00ff88, opacity: 0.0 }
        rect bullet9 { x: 0, y: -100, width: 4, height: 12, fill: #00ff88, opacity: 0.0 }
    }

    // Enemy ships (pool of 8)
    group enemies {
        x: 0, y: 0

        // Enemy 0
        group enemy0 {
            x: 100, y: -50
            opacity: 0.0

            rect body {
                x: -12, y: -12
                width: 24, height: 24
                fill: #ff006e
            }

            rect wing1 {
                x: -18, y: -8
                width: 6, height: 16
                fill: #ff6b35
            }

            rect wing2 {
                x: 12, y: -8
                width: 6, height: 16
                fill: #ff6b35
            }
        }

        group enemy1 {
            x: 200, y: -50
            opacity: 0.0

            rect body {
                x: -12, y: -12
                width: 24, height: 24
                fill: #ff006e
            }

            rect wing1 {
                x: -18, y: -8
                width: 6, height: 16
                fill: #ff6b35
            }

            rect wing2 {
                x: 12, y: -8
                width: 6, height: 16
                fill: #ff6b35
            }
        }

        group enemy2 {
            x: 300, y: -50
            opacity: 0.0

            rect body {
                x: -12, y: -12
                width: 24, height: 24
                fill: #ff006e
            }

            rect wing1 {
                x: -18, y: -8
                width: 6, height: 16
                fill: #ff6b35
            }

            rect wing2 {
                x: 12, y: -8
                width: 6, height: 16
                fill: #ff6b35
            }
        }

        group enemy3 {
            x: 150, y: -50
            opacity: 0.0

            rect body {
                x: -12, y: -12
                width: 24, height: 24
                fill: #ff006e
            }

            rect wing1 {
                x: -18, y: -8
                width: 6, height: 16
                fill: #ff6b35
            }

            rect wing2 {
                x: 12, y: -8
                width: 6, height: 16
                fill: #ff6b35
            }
        }
    }

    // Explosions (pool of 4)
    group explosions {
        x: 0, y: 0

        group explosion0 {
            x: 0, y: 0
            opacity: 0.0

            circle outer {
                x: 0, y: 0
                radius: 30
                fill: #ff6b35
                opacity: 0.6
            }

            circle middle {
                x: 0, y: 0
                radius: 20
                fill: #ff006e
                opacity: 0.8
            }

            circle inner {
                x: 0, y: 0
                radius: 10
                fill: #ffffff
            }
        }

        group explosion1 {
            x: 0, y: 0
            opacity: 0.0

            circle outer {
                x: 0, y: 0
                radius: 30
                fill: #ff6b35
                opacity: 0.6
            }

            circle middle {
                x: 0, y: 0
                radius: 20
                fill: #ff006e
                opacity: 0.8
            }

            circle inner {
                x: 0, y: 0
                radius: 10
                fill: #ffffff
            }
        }

        group explosion2 {
            x: 0, y: 0
            opacity: 0.0

            circle outer {
                x: 0, y: 0
                radius: 30
                fill: #ff6b35
                opacity: 0.6
            }

            circle middle {
                x: 0, y: 0
                radius: 20
                fill: #ff006e
                opacity: 0.8
            }

            circle inner {
                x: 0, y: 0
                radius: 10
                fill: #ffffff
            }
        }

        group explosion3 {
            x: 0, y: 0
            opacity: 0.0

            circle outer {
                x: 0, y: 0
                radius: 30
                fill: #ff6b35
                opacity: 0.6
            }

            circle middle {
                x: 0, y: 0
                radius: 20
                fill: #ff006e
                opacity: 0.8
            }

            circle inner {
                x: 0, y: 0
                radius: 10
                fill: #ffffff
            }
        }
    }

    // Game over screen
    group gameOverScreen {
        x: 200, y: 350
        opacity: 0.0

        rect overlay {
            x: -200, y: -350
            width: 400, height: 700
            fill: #000000
            opacity: 0.8
        }

        text gameOverText {
            x: 0, y: -50
            content: "GAME OVER"
            fontSize: 40
            color: #ff006e
        }

        text finalScoreLabel {
            x: 0, y: 10
            content: "Final Score:"
            fontSize: 20
            color: #888888
        }

        text finalScoreValue {
            x: 0, y: 40
            content: "0"
            fontSize: 32
            color: #00d9ff
        }

        text pressSpaceText {
            x: 0, y: 90
            content: "Press SPACE to restart"
            fontSize: 16
            color: #888888
        }
    }
}

// Engine glow pulse
anim "EnginePulse" {
    duration: 0.3
    loop: pingpong

    track "opacity" {
        keyframe 0 -> 0.5
        keyframe 0.3 -> 1.0
    }
}
