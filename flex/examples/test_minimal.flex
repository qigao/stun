// Minimal test for state machine parsing

scene test {
    width: 800
    height: 600

    rect bg {
        x: 0, y: 0
        width: 800, height: 600
        fill: #f5f5f5
    }

    group button {
        x: 100, y: 100

        machine {
            layer interaction {
                state normal {
                    initial: true
                    bg.fill: #00d9ff
                    scale: 1.0
                }

                state hover {
                    bg.fill: #00e5ff
                    scale: 1.05
                }

                transition normal -> hover on "mouseenter"
                transition hover -> normal on "mouseleave"
            }
        }

        rect bg {
            x: -60, y: -20
            width: 120, height: 40
            fill: #00d9ff
        }

        text label {
            x: 0, y: 5
            content: "Click Me"
            fontSize: 16
            color: #ffffff
        }
    }
}
