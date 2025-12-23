// Data Binding Demo
// Simple counter with reactive UI

scene counter {
    width: 400
    height: 300

    // Background
    rect background {
        x: 0, y: 0
        width: 400, height: 300
        fill: #1a1a2e
    }

    // Main layout container
    group mainLayout {
        layout: flex
        flexDirection: column
        justifyContent: center
        alignItems: center
        x: 0, y: 0
        width: 400, height: 300
        gap: 30

        // Title
        text title {
            content: "Data Binding Demo"
            fontSize: 28
            color: #00d9ff
        }

        // Counter display with background
        group counterBox {
            width: 160, height: 80
            
            rect displayBg {
                x: 0, y: 0
                width: 160, height: 80
                fill: #2c2c54
                // cornerRadius: 8 // If supported
            }

            group counterLabel {
                layout: flex
                justifyContent: center
                alignItems: center
                width: 160, height: 80
                
                text counterValue {
                    content: "0"
                    fontSize: 48
                    color: #00ff88
                }
            }
        }

        // Buttons container
        group buttonRow {
            layout: flex
            flexDirection: row
            justifyContent: center
            alignItems: center
            gap: 40
            width: 400, height: 60

            // Increment button
            group incrementButton {
                width: 80, height: 45
                
                rect bg {
                    width: 80, height: 45
                    fill: #00d9ff
                }

                group labelContainer {
                    layout: flex
                    justifyContent: center
                    alignItems: center
                    width: 80, height: 45
                    
                    text label {
                        content: "+"
                        fontSize: 32
                        color: #ffffff
                    }
                }
            }

            // Decrement button
            group decrementButton {
                width: 80, height: 45
                
                rect bg {
                    width: 80, height: 45
                    fill: #ff006e
                }

                group labelContainer {
                    layout: flex
                    justifyContent: center
                    alignItems: center
                    width: 80, height: 45
                    
                    text label {
                        content: "-"
                        fontSize: 32
                        color: #ffffff
                        textAlign: center
                    }
                }
            }
        }

        // Status text
        text statusText {
            content: "Neutral"
            fontSize: 18
            color: #888888
        }
    }
}

// ----------------------------------------------------------------------------
// Status Animations
// ----------------------------------------------------------------------------

anim "toVeryHigh" {
    duration: 0.1s
    track "#statusText/content" { keyframe 0s -> "Very High!" }
    track "#statusText/text.color" { keyframe 0s -> #ff0000 }
}

anim "toHigh" {
    duration: 0.1s
    track "#statusText/content" { keyframe 0s -> "High" }
    track "#statusText/text.color" { keyframe 0s -> #ffaa00 }
}

anim "toPositive" {
    duration: 0.1s
    track "#statusText/content" { keyframe 0s -> "Positive" }
    track "#statusText/text.color" { keyframe 0s -> #00ff88 }
}

anim "toNeutral" {
    duration: 0.1s
    track "#statusText/content" { keyframe 0s -> "Neutral" }
    track "#statusText/text.color" { keyframe 0s -> #888888 }
}

anim "toNegative" {
    duration: 0.1s
    track "#statusText/content" { keyframe 0s -> "Negative" }
    track "#statusText/text.color" { keyframe 0s -> #ffaa00 }
}

anim "toVeryLow" {
    duration: 0.1s
    track "#statusText/content" { keyframe 0s -> "Very Low!" }
    track "#statusText/text.color" { keyframe 0s -> #ff006e }
}

// ----------------------------------------------------------------------------
// Logic State Machine
// ----------------------------------------------------------------------------

machine statusTracker {
    layer status {
        state neutral { initial: true, animation: "toNeutral" }
        state positive { animation: "toPositive" }
        state high { animation: "toHigh" }
        state veryHigh { animation: "toVeryHigh" }
        state negative { animation: "toNegative" }
        state veryLow { animation: "toVeryLow" }

        // Forward transitions
        transition neutral -> positive when counter > 0
        transition positive -> high when counter > 5
        transition high -> veryHigh when counter > 10

        // Backward transitions
        transition veryHigh -> high when counter < 10.1
        transition high -> positive when counter < 5.1
        transition positive -> neutral when counter < 0.1
        
        // Negative transitions
        transition neutral -> negative when counter < 0
        transition negative -> veryLow when counter < -5
        
        transition veryLow -> negative when counter > -5.1
        transition negative -> neutral when counter > -0.1
    }
}
