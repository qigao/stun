// hello.flex - Simplified syntax for new parser
// Scene with player and target

scene main {
    width: 800
    height: 600

    // Background
    rect background {
        x: 0
        y: 0
        width: 800
        height: 600
        fill: #1a1a2e
    }

    // Player group
    group player {
        x: 100
        y: 300

        // Body
        rect body {
            x: 0
            y: 0
            width: 80
            height: 80
            fill: #e94560
            stroke: #ffffff
            strokeWidth: 2
        }

        // Eye
        circle eye {
            x: 50
            y: 20
            radius: 10
            fill: #ffffff
        }
    }

    // Target circle
    circle target {
        x: 600
        y: 300
        radius: 40
        fill: #0f3460
    }
}

// Animation: move player to the right
anim "moveRight" {
    duration: 2
    loop: once

    track "x" {
        keyframe 0 -> 100
        keyframe 1 -> 400
        keyframe 2 -> 600
    }
}

// Animation: pulse the target
anim "pulse" {
    duration: 1
    loop: loop

    track "scaleX" {
        keyframe 0 -> 1.0
        keyframe 0.5 -> 1.2
        keyframe 1 -> 1.0
    }

    track "scaleY" {
        keyframe 0 -> 1.0
        keyframe 0.5 -> 1.2
        keyframe 1 -> 1.0
    }
}
