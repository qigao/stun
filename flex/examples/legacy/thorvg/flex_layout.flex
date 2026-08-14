// Flex Layout Demo
// Demonstrates automatic layout using Flexbox

scene layoutDemo {
    width: 1000
    height: 700

    // Background
    rect background {
        x: 0, y: 0
        width: 1000, height: 700
        fill: #f0f0f0
    }

    // Title
    text title {
        x: 500, y: 40
        content: "Flexbox Layout Demo"
        fontSize: 28
        color: #2c2c54
    }

    // Scene 1: Horizontal Toolbar (top-left)
    group scene1 {
        x: 20, y: 80

        rect sceneBg {
            x: 0, y: 0
            width: 460, height: 150
            fill: #ffffff
        }

        text sceneLabel {
            x: 230, y: 20
            content: "1. Horizontal Toolbar (Flex Row)"
            fontSize: 14
            color: #00d9ff
        }

        // Toolbar container - will be layout programmatically
        group toolbar {
            x: 20, y: 50
            // NOTE: Layout properties set in C++
            // layout: flex, direction: row, gap: 10

            rect btn1 { x: 0, y: 0, width: 80, height: 40, fill: #00d9ff }
            rect btn2 { x: 0, y: 0, width: 80, height: 40, fill: #00d9ff }
            rect btn3 { x: 0, y: 0, width: 80, height: 40, fill: #00d9ff }
            rect btn4 { x: 0, y: 0, width: 80, height: 40, fill: #00d9ff }
        }
    }

    // Scene 2: Vertical Menu (top-right)
    group scene2 {
        x: 520, y: 80

        rect sceneBg {
            x: 0, y: 0
            width: 460, height: 150
            fill: #ffffff
        }

        text sceneLabel {
            x: 230, y: 20
            content: "2. Vertical Menu (Flex Column)"
            fontSize: 14
            color: #00d9ff
        }

        // Menu container - layout set in C++
        group menu {
            x: 20, y: 50
            // layout: flex, direction: column, gap: 5

            rect menuItem1 { x: 0, y: 0, width: 200, height: 30, fill: #e94560 }
            rect menuItem2 { x: 0, y: 0, width: 200, height: 30, fill: #e94560 }
            rect menuItem3 { x: 0, y: 0, width: 200, height: 30, fill: #e94560 }
        }
    }

    // Scene 3: Centered Layout (middle-left)
    group scene3 {
        x: 20, y: 260

        rect sceneBg {
            x: 0, y: 0
            width: 460, height: 180
            fill: #ffffff
        }

        text sceneLabel {
            x: 230, y: 20
            content: "3. Center Alignment (justify: center)"
            fontSize: 14
            color: #00d9ff
        }

        // Centered container
        group centerContainer {
            x: 30, y: 50
            // width/height set in C++, justify: center

            rect centerBox {
                x: 0, y: 0
                width: 100, height: 100
                fill: #ff006e
            }
        }
    }

    // Scene 4: Space Between (middle-right)
    group scene4 {
        x: 520, y: 260

        rect sceneBg {
            x: 0, y: 0
            width: 460, height: 180
            fill: #ffffff
        }

        text sceneLabel {
            x: 230, y: 20
            content: "4. Space Between (justify: space-between)"
            fontSize: 14
            color: #00d9ff
        }

        // Space between container
        group spaceBetweenContainer {
            x: 30, y: 50
            // justify: space-between

            rect box1 { x: 0, y: 0, width: 60, height: 60, fill: #00ff88 }
            rect box2 { x: 0, y: 0, width: 60, height: 60, fill: #00ff88 }
            rect box3 { x: 0, y: 0, width: 60, height: 60, fill: #00ff88 }
        }
    }

    // Scene 5: Nested Layout (bottom-left)
    group scene5 {
        x: 20, y: 470

        rect sceneBg {
            x: 0, y: 0
            width: 460, height: 180
            fill: #ffffff
        }

        text sceneLabel {
            x: 230, y: 20
            content: "5. Nested Layout (Column > Row)"
            fontSize: 14
            color: #00d9ff
        }

        // Outer column container
        group outerColumn {
            x: 30, y: 50
            // direction: column, gap: 10

            // Row 1
            group row1 {
                x: 0, y: 0
                // direction: row, gap: 5

                rect r1c1 { x: 0, y: 0, width: 50, height: 40, fill: #3498db }
                rect r1c2 { x: 0, y: 0, width: 50, height: 40, fill: #3498db }
                rect r1c3 { x: 0, y: 0, width: 50, height: 40, fill: #3498db }
            }

            // Row 2
            group row2 {
                x: 0, y: 0
                // direction: row, gap: 5

                rect r2c1 { x: 0, y: 0, width: 50, height: 40, fill: #9b59b6 }
                rect r2c2 { x: 0, y: 0, width: 50, height: 40, fill: #9b59b6 }
                rect r2c3 { x: 0, y: 0, width: 50, height: 40, fill: #9b59b6 }
            }
        }
    }

    // Scene 6: Padding & Gap (bottom-right)
    group scene6 {
        x: 520, y: 470

        rect sceneBg {
            x: 0, y: 0
            width: 460, height: 180
            fill: #ffffff
        }

        text sceneLabel {
            x: 230, y: 20
            content: "6. Padding & Gap (padding: 20, gap: 15)"
            fontSize: 14
            color: #00d9ff
        }

        // Container with padding
        group paddedContainer {
            x: 30, y: 50
            // padding: 20, gap: 15

            rect item1 { x: 0, y: 0, width: 70, height: 30, fill: #f39c12 }
            rect item2 { x: 0, y: 0, width: 70, height: 30, fill: #f39c12 }
            rect item3 { x: 0, y: 0, width: 70, height: 30, fill: #f39c12 }
        }
    }

    // Instructions
    text instructions {
        x: 500, y: 680
        content: "All layouts are automatic! | Press R to re-layout | ESC to quit"
        fontSize: 14
        color: #888888
    }
}
