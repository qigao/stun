// Shopping Cart Demo
// E-commerce shopping cart with dynamic price calculation

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

    // Item 1 - Laptop
    group item1 {
        x: 0, y: 100

        rect itemBg {
            x: 20, y: 0
            width: 460, height: 100
            fill: #ffffff
        }

        text itemName {
            x: 40, y: 30
            content: "Gaming Laptop"
            fontSize: 18
            color: #2c2c54
        }

        text itemPrice {
            x: 40, y: 55
            content: "$999"
            fontSize: 16
            color: #666666
        }

        // Quantity controls
        group qtyControls {
            x: 320, y: 40

            rect minusBtn {
                x: 0, y: 0
                width: 30, height: 30
                fill: #ff006e
            }

            text minusLabel {
                x: 15, y: 20
                content: "-"
                fontSize: 20
                color: #ffffff
            }

            rect qtyBg {
                x: 35, y: 0
                width: 50, height: 30
                fill: #e0e0e0
            }

            text qty {
                x: 60, y: 20
                content: "1"
                fontSize: 16
                color: #2c2c54
            }

            rect plusBtn {
                x: 90, y: 0
                width: 30, height: 30
                fill: #00d9ff
            }

            text plusLabel {
                x: 105, y: 20
                content: "+"
                fontSize: 20
                color: #ffffff
            }
        }

        text subtotal1 {
            x: 430, y: 55
            content: "$999"
            fontSize: 18
            color: #00d9ff
        }
    }

    // Item 2 - Mouse
    group item2 {
        x: 0, y: 220

        rect itemBg {
            x: 20, y: 0
            width: 460, height: 100
            fill: #ffffff
        }

        text itemName {
            x: 40, y: 30
            content: "Wireless Mouse"
            fontSize: 18
            color: #2c2c54
        }

        text itemPrice {
            x: 40, y: 55
            content: "$49"
            fontSize: 16
            color: #666666
        }

        group qtyControls {
            x: 320, y: 40

            rect minusBtn {
                x: 0, y: 0
                width: 30, height: 30
                fill: #ff006e
            }

            text minusLabel {
                x: 15, y: 20
                content: "-"
                fontSize: 20
                color: #ffffff
            }

            rect qtyBg {
                x: 35, y: 0
                width: 50, height: 30
                fill: #e0e0e0
            }

            text qty {
                x: 60, y: 20
                content: "2"
                fontSize: 16
                color: #2c2c54
            }

            rect plusBtn {
                x: 90, y: 0
                width: 30, height: 30
                fill: #00d9ff
            }

            text plusLabel {
                x: 105, y: 20
                content: "+"
                fontSize: 20
                color: #ffffff
            }
        }

        text subtotal2 {
            x: 430, y: 55
            content: "$98"
            fontSize: 18
            color: #00d9ff
        }
    }

    // Item 3 - Keyboard
    group item3 {
        x: 0, y: 340

        rect itemBg {
            x: 20, y: 0
            width: 460, height: 100
            fill: #ffffff
        }

        text itemName {
            x: 40, y: 30
            content: "Mechanical Keyboard"
            fontSize: 18
            color: #2c2c54
        }

        text itemPrice {
            x: 40, y: 55
            content: "$129"
            fontSize: 16
            color: #666666
        }

        group qtyControls {
            x: 320, y: 40

            rect minusBtn {
                x: 0, y: 0
                width: 30, height: 30
                fill: #ff006e
            }

            text minusLabel {
                x: 15, y: 20
                content: "-"
                fontSize: 20
                color: #ffffff
            }

            rect qtyBg {
                x: 35, y: 0
                width: 50, height: 30
                fill: #e0e0e0
            }

            text qty {
                x: 60, y: 20
                content: "1"
                fontSize: 16
                color: #2c2c54
            }

            rect plusBtn {
                x: 90, y: 0
                width: 30, height: 30
                fill: #00d9ff
            }

            text plusLabel {
                x: 105, y: 20
                content: "+"
                fontSize: 20
                color: #ffffff
            }
        }

        text subtotal3 {
            x: 430, y: 55
            content: "$129"
            fontSize: 18
            color: #00d9ff
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
            content: "$1226"
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
            content: "$122.60"
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
            content: "$1348.60"
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

    // Empty cart message (hidden by default)
    group emptyMessage {
        x: 250, y: 350
        opacity: 0.0

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
