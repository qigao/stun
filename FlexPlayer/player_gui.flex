scene main {
    width: 1280
    height: 720
    fill: #00000000

    group video_placeholder {
        width: 1280
        height: 720
    }

    group controls {
        width: 1280
        height: 100
        x: 0
        y: 620

        rect bg {
            width: 1280
            height: 100
            fill: #000000
            opacity: 0.7
        }

        group progress {
            x: 20
            y: 20
            width: 1240
            height: 10
            
            rect progress_track {
                width: 1240
                height: 10
                fill: #444444
            }
            
            rect progress_fill {
                width: 0
                height: 10
                fill: #3498db
            }
        }

        // --- Navigation Buttons ---

        group start_btn {
            x: 515
            y: 45
            width: 40
            height: 40
            
            circle btn_bg {
                cx: 20
                cy: 20
                radius: 20
                fill: #ffffff
                opacity: 0.8
            }
            // Bar + Triangle
            rect bar { x: 10, y: 12, width: 4, height: 16, fill: #000000 }
            path icon {
                d: "M 28 12 L 16 20 L 28 28 Z"
                fill: #000000
            }
        }

        group prev_btn {
            x: 565
            y: 45
            width: 40
            height: 40
            
            circle btn_bg {
                cx: 20
                cy: 20
                radius: 20
                fill: #ffffff
                opacity: 0.8
            }
            // Double triangle
            path icon1 { d: "M 22 12 L 12 20 L 22 28 Z", fill: #000000 }
            path icon2 { d: "M 32 12 L 22 20 L 32 28 Z", fill: #000000 }
        }

        group play_btn {
            x: 615
            y: 40
            width: 50
            height: 50
            
            circle btn_bg {
                cx: 25
                cy: 25
                radius: 25
                fill: #ffffff
            }
            
            // Triangle icon (will be toggled to pause in future)
            path play_icon {
                d: "M 20 15 L 35 25 L 20 35 Z"
                fill: #000000
            }
            // Pause icon (initially hidden)
            group pause_icon {
                visible: false
                rect bar1 { x: 18, y: 15, width: 5, height: 20, fill: #000000 }
                rect bar2 { x: 27, y: 15, width: 5, height: 20, fill: #000000 }
            }
        }

        group next_btn {
            x: 675
            y: 45
            width: 40
            height: 40
            
            circle btn_bg {
                cx: 20
                cy: 20
                radius: 20
                fill: #ffffff
                opacity: 0.8
            }
            // Double triangle
            path icon1 { d: "M 8 12 L 18 20 L 8 28 Z", fill: #000000 }
            path icon2 { d: "M 18 12 L 28 20 L 18 28 Z", fill: #000000 }
        }

        group end_btn {
            x: 725
            y: 45
            width: 40
            height: 40
            
            circle btn_bg {
                cx: 20
                cy: 20
                radius: 20
                fill: #ffffff
                opacity: 0.8
            }
            // Triangle + Bar
            path icon {
                d: "M 12 12 L 24 20 L 12 28 Z"
                fill: #000000
            }
            rect bar { x: 26, y: 12, width: 4, height: 16, fill: #000000 }
        }

        text time_label {
            name: "time_label"
            content: "00:00 / 00:00"
            x: 20
            y: 50
            fill: #ffffff
            fontSize: 16
        }
    }
}
