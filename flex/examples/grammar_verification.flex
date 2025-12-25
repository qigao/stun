// 1. Data Block (Must be defined BEFORE usage in Scene)
data fruits {
    apple: { name: "Apple", color: "#ff0000" }
    banana: { name: "Banana", color: "#ffff00" }
}

// 2. Assets (Global)
assets {
    audio click: "sounds/click.wav"
    image logo: "images/logo.png" {
        preload: true
    }
}

// 3. Scene
scene verification {
    width: 800
    height: 600

    // Standard Shapes
    rect background {
        width: 800
        height: 600
        fill: #f0f0f0
    }

    circle status {
        x: 750, y: 50
        radius: 20
        fill: #00ff00
    }

    // Custom Component
    Slider volume {
        x: 50
        y: 50
        width: 200
        value: 0.75 
    }

    // Repeat Block
    group grid {
        x: 50
        y: 150
        width: 300
        layout: flex
        flexDirection: row
        flexWrap: wrap
        gap: 10

        repeat 5 {
            rect item@index {
                width: 50
                height: 50
                fill: #3498db
            }
        }
    }

    // For Loop using Data
    group list {
        x: 400
        y: 150
        layout: flex
        flexDirection: column
        gap: 5

        for item in fruits {
            group row {
                width: 200
                height: 30

                rect bg {
                    width: 200, height: 30
                    fill: ${item.color}
                }

                text label {
                    content: ${item.name}
                    color: #000000
                    fontSize: 14
                    x: 10, y: 5
                }
            }
        }
    }
}

// 4. Animation
anim "fadeAlert" {
    duration: 1.0s
    loop: pingpong

    track "#status/opacity" {
        keyframe 0s -> 1.0
        keyframe 1s -> 0.2
    }
}

// 5. State Machine
machine mainController {
    layer ui {
        state normal {
            initial: true
        }

        state alert {
            animation: "fadeAlert"
        }

        transition normal -> alert when hasWarning > 0
        transition alert -> normal when hasWarning == 0
    }
}
