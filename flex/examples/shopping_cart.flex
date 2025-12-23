// Shopping Cart Demo
// E-commerce shopping cart with dynamic price calculation
// Refactored to use repeat for item generation

scene cart {
    width: 500
    height: 700

    // Background
    rect background {
        x: 0, y: 0
        width: 500, height: 700
        fill: #f5f5f5
    }

    // Header
    group header {
        x: 250, y: 40

        text title {
            x: 0, y: 0
            content: "Shopping Cart"
            fontSize: 28
            color: #2c2c54
        }

        // Cart icon
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

    // Cart items container - uses flex layout for automatic spacing
    group cartItems {
        x: 0, y: 100
        layout: flex
        flexDirection: column
        gap: 20

        // Generate 3 cart item slots using repeat
        // Note: item IDs are item0, item1, item2 (0-indexed)
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
                    content: "$0"
                    fontSize: 16
                    color: #666666
                }

                // Quantity controls
                group qtyControls {
                    x: 320, y: 40

                    group minusBtn {
                        x: 0, y: 0

                        rect minusBg {
                            x: 0, y: 0
                            width: 30, height: 30
                            fill: #ff006e
                        }

                        text minusLabel {
                            x: 10, y: 0
                            content: "-"
                            fontSize: 20
                            color: #ffffff
                        }
                    }

                    group qtyInput {
                        x: 35, y: 0

                        rect qtyBg {
                            x: 0, y: 0
                            width: 50, height: 30
                            fill: #e0e0e0
                        }

                        text qty {
                            x: 10, y: 0
                            content: "1"
                            fontSize: 16
                            color: #2c2c54
                        }
                    }

                    group plusBtn {
                        x: 90, y: 0

                        rect plusBg {
                            x: 0, y: 0
                            width: 30, height: 30
                            fill: #00d9ff
                        }

                        text plusLabel {
                            x: 10, y: 0
                            content: "+"
                            fontSize: 20
                            color: #ffffff
                        }
                    }
                }

                text subtotal {
                    x: 430, y: 55
                    content: "$0"
                    fontSize: 18
                    color: #00d9ff
                }
            }
        }
    }

    // Divider
    rect divider {
        x: 20, y: 470
        width: 460, height: 2
        fill: #cccccc
    }

    // Summary section
    group summary {
        x: 0, y: 500

        rect summaryBg {
            x: 20, y: 0
            width: 460, height: 120
            fill: #ffffff
        }

        text subtotalLabel {
            x: 40, y: 30
            content: "Subtotal:"
            fontSize: 18
            color: #666666
        }

        text subtotalValue {
            x: 420, y: 30
            content: "$0"
            fontSize: 18
            color: #2c2c54
        }

        text taxLabel {
            x: 40, y: 60
            content: "Tax (10%):"
            fontSize: 18
            color: #666666
        }

        text taxValue {
            x: 420, y: 60
            content: "$0"
            fontSize: 18
            color: #2c2c54
        }

        text totalLabel {
            x: 40, y: 95
            content: "Total:"
            fontSize: 22
            color: #2c2c54
        }

        text totalValue {
            x: 420, y: 95
            content: "$0"
            fontSize: 22
            color: #00d9ff
        }
    }

    // Action buttons
    group actions {
        x: 250, y: 650

        // Checkout button
        rect checkoutBtn {
            x: -100, y: 0
            width: 150, height: 45
            fill: #00d9ff
        }

        text checkoutLabel {
            x: -25, y: 25
            content: "Checkout"
            fontSize: 18
            color: #ffffff
        }

        // Clear cart button
        rect clearBtn {
            x: 60, y: 0
            width: 100, height: 45
            fill: #ff006e
        }

        text clearLabel {
            x: 110, y: 25
            content: "Clear"
            fontSize: 18
            color: #ffffff
        }
    }

    // Empty cart message (hidden by default, visible: false disables hit-testing)
    group emptyMessage {
        x: 250, y: 500
        visible: false

        text emptyText {
            x: 0, y: 0
            content: "Your cart is empty"
            fontSize: 24
            color: #888888
        }

        text emptyHint {
            x: 0, y: 35
            content: "Add some items to get started!"
            fontSize: 16
            color: #aaaaaa
        }
    }
}
