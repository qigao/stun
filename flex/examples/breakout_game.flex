// Breakout Game - Classic brick breaker
// Player controls paddle to bounce ball and break bricks

scene game {
    width: 600
    height: 800

    // Background
    rect background {
        x: 0, y: 0
        width: 600, height: 800
        fill: #0a0a0a
    }

    // Score display
    text scoreLabel {
        x: 50, y: 30
        content: "SCORE:"
        fontSize: 20
        color: #888888
    }

    text scoreValue {
        x: 150, y: 30
        content: "0"
        fontSize: 20
        color: #00d9ff
    }

    // Lives display
    text livesLabel {
        x: 450, y: 30
        content: "LIVES:"
        fontSize: 20
        color: #888888
    }

    text livesValue {
        x: 540, y: 30
        content: "3"
        fontSize: 20
        color: #ff006e
    }

    // Bricks (8 rows x 10 columns)
    group bricks {
        x: 0, y: 80

        // Row 1 - Red
        rect brick0 { x: 30, y: 0, width: 50, height: 20, fill: #ff006e }
        rect brick1 { x: 85, y: 0, width: 50, height: 20, fill: #ff006e }
        rect brick2 { x: 140, y: 0, width: 50, height: 20, fill: #ff006e }
        rect brick3 { x: 195, y: 0, width: 50, height: 20, fill: #ff006e }
        rect brick4 { x: 250, y: 0, width: 50, height: 20, fill: #ff006e }
        rect brick5 { x: 305, y: 0, width: 50, height: 20, fill: #ff006e }
        rect brick6 { x: 360, y: 0, width: 50, height: 20, fill: #ff006e }
        rect brick7 { x: 415, y: 0, width: 50, height: 20, fill: #ff006e }
        rect brick8 { x: 470, y: 0, width: 50, height: 20, fill: #ff006e }
        rect brick9 { x: 525, y: 0, width: 50, height: 20, fill: #ff006e }

        // Row 2 - Orange
        rect brick10 { x: 30, y: 25, width: 50, height: 20, fill: #ff6b35 }
        rect brick11 { x: 85, y: 25, width: 50, height: 20, fill: #ff6b35 }
        rect brick12 { x: 140, y: 25, width: 50, height: 20, fill: #ff6b35 }
        rect brick13 { x: 195, y: 25, width: 50, height: 20, fill: #ff6b35 }
        rect brick14 { x: 250, y: 25, width: 50, height: 20, fill: #ff6b35 }
        rect brick15 { x: 305, y: 25, width: 50, height: 20, fill: #ff6b35 }
        rect brick16 { x: 360, y: 25, width: 50, height: 20, fill: #ff6b35 }
        rect brick17 { x: 415, y: 25, width: 50, height: 20, fill: #ff6b35 }
        rect brick18 { x: 470, y: 25, width: 50, height: 20, fill: #ff6b35 }
        rect brick19 { x: 525, y: 25, width: 50, height: 20, fill: #ff6b35 }

        // Row 3 - Yellow
        rect brick20 { x: 30, y: 50, width: 50, height: 20, fill: #f7931e }
        rect brick21 { x: 85, y: 50, width: 50, height: 20, fill: #f7931e }
        rect brick22 { x: 140, y: 50, width: 50, height: 20, fill: #f7931e }
        rect brick23 { x: 195, y: 50, width: 50, height: 20, fill: #f7931e }
        rect brick24 { x: 250, y: 50, width: 50, height: 20, fill: #f7931e }
        rect brick25 { x: 305, y: 50, width: 50, height: 20, fill: #f7931e }
        rect brick26 { x: 360, y: 50, width: 50, height: 20, fill: #f7931e }
        rect brick27 { x: 415, y: 50, width: 50, height: 20, fill: #f7931e }
        rect brick28 { x: 470, y: 50, width: 50, height: 20, fill: #f7931e }
        rect brick29 { x: 525, y: 50, width: 50, height: 20, fill: #f7931e }

        // Row 4 - Green
        rect brick30 { x: 30, y: 75, width: 50, height: 20, fill: #00d9ff }
        rect brick31 { x: 85, y: 75, width: 50, height: 20, fill: #00d9ff }
        rect brick32 { x: 140, y: 75, width: 50, height: 20, fill: #00d9ff }
        rect brick33 { x: 195, y: 75, width: 50, height: 20, fill: #00d9ff }
        rect brick34 { x: 250, y: 75, width: 50, height: 20, fill: #00d9ff }
        rect brick35 { x: 305, y: 75, width: 50, height: 20, fill: #00d9ff }
        rect brick36 { x: 360, y: 75, width: 50, height: 20, fill: #00d9ff }
        rect brick37 { x: 415, y: 75, width: 50, height: 20, fill: #00d9ff }
        rect brick38 { x: 470, y: 75, width: 50, height: 20, fill: #00d9ff }
        rect brick39 { x: 525, y: 75, width: 50, height: 20, fill: #00d9ff }

        // Row 5 - Blue
        rect brick40 { x: 30, y: 100, width: 50, height: 20, fill: #8338ec }
        rect brick41 { x: 85, y: 100, width: 50, height: 20, fill: #8338ec }
        rect brick42 { x: 140, y: 100, width: 50, height: 20, fill: #8338ec }
        rect brick43 { x: 195, y: 100, width: 50, height: 20, fill: #8338ec }
        rect brick44 { x: 250, y: 100, width: 50, height: 20, fill: #8338ec }
        rect brick45 { x: 305, y: 100, width: 50, height: 20, fill: #8338ec }
        rect brick46 { x: 360, y: 100, width: 50, height: 20, fill: #8338ec }
        rect brick47 { x: 415, y: 100, width: 50, height: 20, fill: #8338ec }
        rect brick48 { x: 470, y: 100, width: 50, height: 20, fill: #8338ec }
        rect brick49 { x: 525, y: 100, width: 50, height: 20, fill: #8338ec }
    }

    // Ball
    circle ball {
        x: 300, y: 400
        radius: 8
        fill: #ffffff
    }

    // Ball trail effect
    circle trail1 {
        x: 300, y: 400
        radius: 6
        fill: #00d9ff
        opacity: 0.5
    }

    circle trail2 {
        x: 300, y: 400
        radius: 4
        fill: #8338ec
        opacity: 0.3
    }

    // Paddle
    group paddle {
        x: 300, y: 720

        rect body {
            x: -60, y: -8
            width: 120, height: 16
            fill: #00d9ff
        }

        rect highlight {
            x: -55, y: -6
            width: 110, height: 4
            fill: #ffffff
            opacity: 0.3
        }
    }

    // Game over overlay (hidden by default)
    group gameOverScreen {
        x: 300, y: 400
        opacity: 0.0

        rect overlay {
            x: -300, y: -400
            width: 600, height: 800
            fill: #000000
            opacity: 0.8
        }

        text gameOverText {
            x: 0, y: -50
            content: "GAME OVER"
            fontSize: 48
            color: #ff006e
        }

        text pressSpaceText {
            x: 0, y: 10
            content: "Press SPACE to restart"
            fontSize: 20
            color: #888888
        }
    }

    // Win screen (hidden by default)
    group winScreen {
        x: 300, y: 400
        opacity: 0.0

        rect overlay {
            x: -300, y: -400
            width: 600, height: 800
            fill: #000000
            opacity: 0.8
        }

        text winText {
            x: 0, y: -50
            content: "YOU WIN!"
            fontSize: 48
            color: #00d9ff
        }

        text pressSpaceText {
            x: 0, y: 10
            content: "Press SPACE to restart"
            fontSize: 20
            color: #888888
        }
    }
}

// Ball pulse animation
anim "BallPulse" {
    duration: 0.5
    loop: pingpong

    track "radius" {
        keyframe 0 -> 8
        keyframe 0.5 -> 10
    }
}
