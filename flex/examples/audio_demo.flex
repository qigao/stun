// Audio Demo - Demonstrates the assets system with state machine audio control
//
// Features demonstrated:
// 1. assets { } block for preloading audio resources
// 2. play: action in state blocks
// 3. stop: action in state blocks

assets {
    audio click: "sounds/click.wav"
    audio hover: "sounds/hover.wav"
    audio success: "sounds/success.mp3"
    audio bgm: "sounds/background.mp3" {
        loop: true
        volume: 0.5
    }
}

scene audioDemo {
    width: 600
    height: 400

    // Background
    rect background {
        x: 0, y: 0
        width: 600, height: 400
        fill: #1a1a2e
    }

    // Title
    text title {
        x: 200, y: 30
        content: "Audio Demo"
        fontSize: 32
        color: #00d9ff
    }

    // Play Button
    group playButton {
        x: 200, y: 100
        width: 200, height: 60

        rect buttonBg {
            width: 200, height: 60
            radius: 10
            fill: #00d9ff
        }

        text buttonLabel {
            x: 70, y: 20
            content: "PLAY"
            fontSize: 20
            color: #1a1a2e
        }
    }

    // Stop Button
    group stopButton {
        x: 200, y: 180
        width: 200, height: 60

        rect buttonBg2 {
            width: 200, height: 60
            radius: 10
            fill: #ff006e
        }

        text buttonLabel2 {
            x: 70, y: 20
            content: "STOP"
            fontSize: 20
            color: white
        }
    }

    // Status indicator
    text statusText {
        x: 200, y: 280
        content: "Ready"
        fontSize: 18
        color: #888888
    }
}

// State machine for audio control
machine audioController {
    layer main {
        state idle {
            initial: true
        }

        state playing {
            play: bgm
            animation: "toPlaying"
        }

        state stopped {
            stop: bgm
            animation: "toStopped"
        }

        // Transitions based on input
        transition idle -> playing when playClicked > 0
        transition playing -> stopped when stopClicked > 0
        transition stopped -> playing when playClicked > 0
    }
}

// UI feedback animations
anim "toPlaying" {
    duration: 0.2s

    track "#statusText/content" {
        keyframe 0s -> "Playing..."
    }

    track "#statusText/color" {
        keyframe 0s -> #00ff88
    }
}

anim "toStopped" {
    duration: 0.2s

    track "#statusText/content" {
        keyframe 0s -> "Stopped"
    }

    track "#statusText/color" {
        keyframe 0s -> #ff006e
    }
}
