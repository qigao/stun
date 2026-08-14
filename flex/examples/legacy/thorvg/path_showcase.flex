// Path Animation Showcase
// Demonstrates various path animation use cases

scene showcase {
    width: 1000
    height: 700

    // Background
    rect background {
        x: 0, y: 0
        width: 1000, height: 700
        fill: #1a1a2e
    }

    // Title
    text title {
        x: 500, y: 40
        content: "Path Animation Showcase"
        fontSize: 28
        color: #eaeaea
    }

    // Scene 1: Enemy Patrol Route (top-left)
    group scene1 {
        x: 0, y: 80

        rect sceneBg {
            x: 20, y: 0
            width: 460, height: 280
            fill: #16213e
        }

        text sceneLabel {
            x: 250, y: 25
            content: "1. Enemy Patrol (Loop)"
            fontSize: 16
            color: #00d9ff
        }

        // Patrol path visualization (just the area)
        rect patrolArea {
            x: 80, y: 60
            width: 340, height: 180
            fill: #0f3460
            opacity: 0.3
        }

        // Enemy
        group enemy {
            x: 100, y: 150

            circle body {
                x: 0, y: 0
                radius: 20
                fill: #ff006e
            }

            circle eye1 {
                x: -8, y: -5
                radius: 4
                fill: #ffffff
            }

            circle eye2 {
                x: 8, y: -5
                radius: 4
                fill: #ffffff
            }
        }
    }

    // Scene 2: UI Menu Slide-In (top-right)
    group scene2 {
        x: 520, y: 80

        rect sceneBg {
            x: 0, y: 0
            width: 460, height: 280
            fill: #16213e
        }

        text sceneLabel {
            x: 230, y: 25
            content: "2. Menu Slide-In (Elastic)"
            fontSize: 16
            color: #00d9ff
        }

        // Menu panel
        group menu {
            x: -200, y: 140

            rect menuBg {
                x: 0, y: 0
                width: 180, height: 120
                fill: #e94560
            }

            text menuItem1 {
                x: 90, y: 30
                content: "New Game"
                fontSize: 14
                color: #ffffff
            }

            text menuItem2 {
                x: 90, y: 60
                content: "Load Game"
                fontSize: 14
                color: #ffffff
            }

            text menuItem3 {
                x: 90, y: 90
                content: "Settings"
                fontSize: 14
                color: #ffffff
            }
        }
    }

    // Scene 3: Particle Flow (bottom-left)
    group scene3 {
        x: 0, y: 380

        rect sceneBg {
            x: 20, y: 0
            width: 460, height: 280
            fill: #16213e
        }

        text sceneLabel {
            x: 250, y: 25
            content: "3. Particle Flow (Multi-Object)"
            fontSize: 16
            color: #00d9ff
        }

        // Path visualization curve
        circle pathStart {
            x: 60, y: 150
            radius: 8
            fill: #00d9ff
            opacity: 0.5
        }

        circle pathEnd {
            x: 440, y: 150
            radius: 8
            fill: #00ff88
            opacity: 0.5
        }

        // Particles (10 particles)
        circle particle0 { x: 60, y: 150, radius: 5, fill: #00d9ff, opacity: 0.0 }
        circle particle1 { x: 60, y: 150, radius: 5, fill: #00d9ff, opacity: 0.0 }
        circle particle2 { x: 60, y: 150, radius: 5, fill: #00d9ff, opacity: 0.0 }
        circle particle3 { x: 60, y: 150, radius: 5, fill: #00d9ff, opacity: 0.0 }
        circle particle4 { x: 60, y: 150, radius: 5, fill: #00d9ff, opacity: 0.0 }
        circle particle5 { x: 60, y: 150, radius: 5, fill: #00d9ff, opacity: 0.0 }
        circle particle6 { x: 60, y: 150, radius: 5, fill: #00d9ff, opacity: 0.0 }
        circle particle7 { x: 60, y: 150, radius: 5, fill: #00d9ff, opacity: 0.0 }
        circle particle8 { x: 60, y: 150, radius: 5, fill: #00d9ff, opacity: 0.0 }
        circle particle9 { x: 60, y: 150, radius: 5, fill: #00d9ff, opacity: 0.0 }
    }

    // Scene 4: Camera Movement (bottom-right)
    group scene4 {
        x: 520, y: 380

        rect sceneBg {
            x: 0, y: 0
            width: 460, height: 280
            fill: #16213e
        }

        text sceneLabel {
            x: 230, y: 25
            content: "4. Camera Pan (Smooth)"
            fontSize: 16
            color: #00d9ff
        }

        // Viewport (what camera sees)
        group viewport {
            x: 50, y: 60

            rect viewportBg {
                x: 0, y: 0
                width: 360, height: 180
                fill: #0f3460
            }

            // Static objects in world
            rect building1 {
                x: 50, y: 120
                width: 60, height: 80
                fill: #555555
            }

            rect building2 {
                x: 150, y: 100
                width: 80, height: 100
                fill: #666666
            }

            rect building3 {
                x: 280, y: 110
                width: 50, height: 90
                fill: #555555
            }

            // Camera indicator (moves)
            group camera {
                x: 50, y: 50

                rect cameraBody {
                    x: -20, y: -15
                    width: 40, height: 30
                    fill: #ff006e
                    opacity: 0.5
                }

                rect cameraLens {
                    x: 20, y: 0
                    width: 15, height: 10
                    fill: #ffffff
                    opacity: 0.5
                }
            }
        }
    }

    // Instructions
    text instructions {
        x: 500, y: 680
        content: "Press 1-4 to trigger animations | R to reset all | ESC to quit"
        fontSize: 14
        color: #888888
    }
}
