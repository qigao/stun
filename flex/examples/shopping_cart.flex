// Shopping Cart Demo
// E-commerce cart with state machine for UI states
// Demonstrates: repeat blocks, state machines, animations

scene cart {
    width: 500
    height: 700

    rect background {
        x: 0, y: 0, width: 500, height: 700, fill: #f5f5f5
    }

    // Header
    group header {
        x: 250, y: 40

        text title {
            content: "Shopping Cart"
            fontSize: 28
            color: #2c2c54
        }

        circle cartIcon {
            x: 120, y: 0
            radius: 16
            stroke: #00d9ff
            strokeWidth: 2
        }

        text itemCount {
            x: 120, y: 5
            content: "3"
            fontSize: 14
            color: #00d9ff
        }
    }

    // Cart items using repeat
    group cartItems {
        x: 0, y: 100
        layout: flex
        flexDirection: column
        gap: 20

        repeat 3 {
            group item@index {
                width: 460, height: 100

                rect itemBg {
                    x: 20, y: 0
                    width: 460, height: 100
                    fill: #ffffff
                }

                text itemName {
                    x: 40, y: 30
                    content: "Product Item"
                    fontSize: 18
                    color: #2c2c54
                }

                text itemPrice {
                    x: 40, y: 55
                    content: "$29.99"
                    fontSize: 16
                    color: #666666
                }

                group qtyControls {
                    x: 320, y: 40

                    group minusBtn {
                        rect bg { width: 30, height: 30, fill: #ff006e }
                        text label { x: 10, content: "-", fontSize: 20, color: #ffffff }
                    }

                    group qtyInput {
                        x: 35
                        rect bg { width: 50, height: 30, fill: #e0e0e0 }
                        text qty { x: 20, content: "1", fontSize: 16, color: #2c2c54 }
                    }

                    group plusBtn {
                        x: 90
                        rect bg { width: 30, height: 30, fill: #00d9ff }
                        text label { x: 10, content: "+", fontSize: 20, color: #ffffff }
                    }
                }

                text subtotal {
                    x: 430, y: 55
                    content: "$29.99"
                    fontSize: 18
                    color: #00d9ff
                }
            }
        }
    }

    rect divider {
        x: 20, y: 470
        width: 460, height: 2
        fill: #cccccc
    }

    // Summary
    group summary {
        x: 0, y: 500

        rect summaryBg {
            x: 20, y: 0
            width: 460, height: 120
            fill: #ffffff
        }

        text subtotalLabel { x: 40, y: 30, content: "Subtotal:", fontSize: 18, color: #666666 }
        text subtotalValue { x: 420, y: 30, content: "$89.97", fontSize: 18, color: #2c2c54 }

        text taxLabel { x: 40, y: 60, content: "Tax (10%):", fontSize: 18, color: #666666 }
        text taxValue { x: 420, y: 60, content: "$9.00", fontSize: 18, color: #2c2c54 }

        text totalLabel { x: 40, y: 95, content: "Total:", fontSize: 22, color: #2c2c54 }
        text totalValue { x: 420, y: 95, content: "$98.97", fontSize: 22, color: #00d9ff }
    }

    // Action buttons
    group actions {
        x: 250, y: 650

        group checkoutBtn {
            x: -100
            rect bg { width: 150, height: 45, fill: #00d9ff }
            text label { x: 35, y: 13, content: "Checkout", fontSize: 18, color: #ffffff }
        }

        group clearBtn {
            x: 60
            rect bg { width: 100, height: 45, fill: #ff006e }
            text label { x: 25, y: 13, content: "Clear", fontSize: 18, color: #ffffff }
        }
    }

    // Status message
    text statusMessage {
        x: 250, y: 380
        content: ""
        fontSize: 16
        color: #00ff88
        opacity: 0
    }

    // Empty cart overlay (hidden by default)
    group emptyOverlay {
        x: 250, y: 350
        opacity: 0

        text emptyText {
            content: "Your cart is empty"
            fontSize: 24
            color: #888888
        }

        text emptyHint {
            y: 35
            content: "Add items to get started!"
            fontSize: 16
            color: #aaaaaa
        }
    }
}

// Cart state machine
machine cartController {
    layer cartState {
        state hasItems {
            initial: true
            animation: "showCart"
        }

        state empty {
            animation: "showEmpty"
        }

        state checkingOut {
            animation: "checkoutPulse"
        }

        state cleared {
            animation: "clearFade"
        }

        transition hasItems -> empty when itemCount < 1
        transition empty -> hasItems when itemCount > 0
        transition hasItems -> checkingOut when checkout > 0
        transition checkingOut -> hasItems when checkout < 1
        transition hasItems -> cleared when clear > 0
        transition cleared -> empty when clear < 1
    }

    layer feedback {
        state idle {
            initial: true
        }

        state success {
            animation: "showSuccess"
        }

        state error {
            animation: "showError"
        }

        transition idle -> success when actionSuccess > 0
        transition success -> idle when actionSuccess < 1
        transition idle -> error when actionError > 0
        transition error -> idle when actionError < 1
    }
}

// Animations
anim "showCart" {
    duration: 0.3
    track "#cartItems/opacity" {
        keyframe 0 -> 0
        keyframe 0.3 -> 1
    }
    track "#emptyOverlay/opacity" {
        keyframe 0 -> 1
        keyframe 0.3 -> 0
    }
}

anim "showEmpty" {
    duration: 0.3
    track "#cartItems/opacity" {
        keyframe 0 -> 1
        keyframe 0.3 -> 0
    }
    track "#emptyOverlay/opacity" {
        keyframe 0 -> 0
        keyframe 0.3 -> 1
    }
}

anim "checkoutPulse" {
    duration: 0.5
    loop: pingpong
    track "#checkoutBtn/opacity" {
        keyframe 0 -> 1
        keyframe 0.25 -> 0.5
        keyframe 0.5 -> 1
    }
}

anim "clearFade" {
    duration: 0.3
    track "#cartItems/opacity" {
        keyframe 0 -> 1
        keyframe 0.3 -> 0
    }
}

anim "showSuccess" {
    duration: 0.5
    track "#statusMessage/content" {
        keyframe 0 -> "Success!"
    }
    track "#statusMessage/color" {
        keyframe 0 -> #00ff88
    }
    track "#statusMessage/opacity" {
        keyframe 0 -> 0
        keyframe 0.2 -> 1
        keyframe 0.5 -> 0
    }
}

anim "showError" {
    duration: 0.5
    track "#statusMessage/content" {
        keyframe 0 -> "Error occurred"
    }
    track "#statusMessage/color" {
        keyframe 0 -> #ff006e
    }
    track "#statusMessage/opacity" {
        keyframe 0 -> 0
        keyframe 0.2 -> 1
        keyframe 0.5 -> 0
    }
}
