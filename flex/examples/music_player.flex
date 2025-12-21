// Music Player UI Demo
// Modern music player interface with animated visualizer

scene player {
    width: 400
    height: 700

    // Gradient background
    rect background {
        x: 0, y: 0
        width: 400, height: 700
        fill: #1a1a2e
    }

    // Album art area
    group albumArt {
        x: 200, y: 180

        // Shadow/glow effect
        circle shadow {
            x: 0, y: 0
            radius: 125
            fill: #0f0f23
            opacity: 0.8
        }

        // Album cover
        circle cover {
            x: 0, y: 0
            radius: 120
            fill: #8338ec
        }

        // Inner ring
        circle ring {
            x: 0, y: 0
            radius: 100
            stroke: #ff006e
            strokeWidth: 3
        }

        // Center dot
        circle center {
            x: 0, y: 0
            radius: 15
            fill: #00d9ff
        }

        // Play icon (triangle)
        group playIcon {
            x: 0, y: 0
            rotation: 0

            rect bar1 {
                x: -5, y: -15
                width: 4, height: 30
                fill: #f5f5f5
            }

            rect bar2 {
                x: 3, y: -15
                width: 4, height: 30
                fill: #f5f5f5
            }
        }
    }

    // Song info
    group songInfo {
        x: 200, y: 350

        text songTitle {
            x: 0, y: 0
            content: "Neon Dreams"
            fontSize: 24
            color: #f5f5f5
        }

        text artist {
            x: 0, y: 30
            content: "Flex Engine"
            fontSize: 16
            color: #888888
        }

        text album {
            x: 0, y: 55
            content: "Synthwave Collection"
            fontSize: 14
            color: #666666
        }
    }

    // Audio visualizer bars
    group visualizer {
        x: 200, y: 450

        rect bar1 {
            x: -120, y: 0
            width: 12, height: 40
            fill: #00d9ff
        }

        rect bar2 {
            x: -90, y: 0
            width: 12, height: 60
            fill: #00d9ff
        }

        rect bar3 {
            x: -60, y: 0
            width: 12, height: 80
            fill: #8338ec
        }

        rect bar4 {
            x: -30, y: 0
            width: 12, height: 100
            fill: #8338ec
        }

        rect bar5 {
            x: 0, y: 0
            width: 12, height: 120
            fill: #ff006e
        }

        rect bar6 {
            x: 30, y: 0
            width: 12, height: 100
            fill: #8338ec
        }

        rect bar7 {
            x: 60, y: 0
            width: 12, height: 80
            fill: #8338ec
        }

        rect bar8 {
            x: 90, y: 0
            width: 12, height: 60
            fill: #00d9ff
        }

        rect bar9 {
            x: 120, y: 0
            width: 12, height: 40
            fill: #00d9ff
        }
    }

    // Progress bar
    group progressBar {
        x: 200, y: 550

        // Time labels
        text currentTime {
            x: -150, y: -20
            content: "1:23"
            fontSize: 12
            color: #888888
        }

        text totalTime {
            x: 150, y: -20
            content: "3:45"
            fontSize: 12
            color: #888888
        }

        // Track background
        rect track {
            x: -150, y: -3
            width: 300, height: 6
            fill: #2c2c54
        }

        // Progress fill
        rect progress {
            x: -150, y: -3
            width: 120, height: 6
            fill: #00d9ff
        }

        // Playhead
        circle playhead {
            x: -30, y: 0
            radius: 8
            fill: #00d9ff
        }
    }

    // Control buttons
    group controls {
        x: 200, y: 630

        // Previous button
        circle prevBg {
            x: -80, y: 0
            radius: 25
            fill: #2c2c54
        }

        // Play/Pause button (larger)
        circle playBg {
            x: 0, y: 0
            radius: 35
            fill: #00d9ff
        }

        // Next button
        circle nextBg {
            x: 80, y: 0
            radius: 25
            fill: #2c2c54
        }

        // Prev icon (left triangle)
        rect prevIcon {
            x: -85, y: -8
            width: 16, height: 16
            fill: #f5f5f5
        }

        // Play icon (triangle)
        rect playTriangle {
            x: -5, y: -10
            width: 20, height: 20
            fill: #1a1a2e
        }

        // Next icon (right triangle)
        rect nextIcon {
            x: 75, y: -8
            width: 16, height: 16
            fill: #f5f5f5
        }
    }
}

// Album rotation (slow spin)
anim "AlbumSpin" {
    duration: 10
    loop: loop

    track "rotation" {
        keyframe 0 -> 0
        keyframe 10 -> 360
    }
}

// Visualizer bar animations (staggered)
anim "Bar1Pulse" {
    duration: 0.6
    loop: pingpong

    track "height" {
        keyframe 0 -> 40
        keyframe 0.6 -> 80
    }
}

anim "Bar2Pulse" {
    duration: 0.7
    loop: pingpong

    track "height" {
        keyframe 0 -> 60
        keyframe 0.7 -> 100
    }
}

anim "Bar3Pulse" {
    duration: 0.5
    loop: pingpong

    track "height" {
        keyframe 0 -> 80
        keyframe 0.5 -> 120
    }
}

anim "Bar4Pulse" {
    duration: 0.8
    loop: pingpong

    track "height" {
        keyframe 0 -> 100
        keyframe 0.8 -> 140
    }
}

anim "Bar5Pulse" {
    duration: 0.4
    loop: pingpong

    track "height" {
        keyframe 0 -> 120
        keyframe 0.4 -> 160
    }
}

// Progress bar animation (3:45 = 225 seconds)
anim "Progress" {
    duration: 225
    loop: loop

    track "width" {
        keyframe 0 -> 0
        keyframe 225 -> 300
    }
}
