// Component System Demo
// Demonstrates reusable UI components

scene componentDemo {
    width: 1000
    height: 700

    // Background
    rect background {
        x: 0, y: 0
        width: 1000, height: 700
        fill: #f5f5f5
    }

    // Title
    text title {
        x: 500, y: 40
        content: "Component System Demo"
        fontSize: 28
        color: #2c2c54
    }

    // Scene 1: Button Components (top section)
    group scene1 {
        x: 50, y: 100

        text sceneLabel {
            x: 0, y: 0
            content: "Buttons (Reusable Component)"
            fontSize: 18
            color: #00d9ff
        }

        // Buttons will be created programmatically using Button component
        // Primary button at (0, 40)
        // Secondary button at (120, 40)
        // Success button at (260, 40)
        // Danger button at (380, 40)
    }

    // Scene 2: Card Components (middle section)
    group scene2 {
        x: 50, y: 200

        text sceneLabel {
            x: 0, y: 0
            content: "Cards (Nested Components)"
            fontSize: 18
            color: #00d9ff
        }

        // Cards will be created programmatically
        // Card 1 at (0, 40)
        // Card 2 at (240, 40)
        // Card 3 at (480, 40)
    }

    // Scene 3: Badge Components (bottom section)
    group scene3 {
        x: 50, y: 450

        text sceneLabel {
            x: 0, y: 0
            content: "Badges (Small Components)"
            fontSize: 18
            color: #00d9ff
        }

        // Badges will be created programmatically
        // Badge "New" at (0, 40)
        // Badge "Hot" at (80, 40)
        // Badge "Sale" at (160, 40)
        // Badge "Beta" at (240, 40)
    }

    // Statistics section
    group stats {
        x: 50, y: 580

        text statsLabel {
            x: 0, y: 0
            content: "Component Stats:"
            fontSize: 16
            color: #888888
        }

        text statsValue {
            x: 0, y: 25
            content: "0 components registered, 0 instances created"
            fontSize: 14
            color: #666666
        }
    }

    // Instructions
    text instructions {
        x: 500, y: 670
        content: "All UI elements are component instances | Press R to recreate | ESC to quit"
        fontSize: 14
        color: #888888
    }
}
