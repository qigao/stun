// Analog Clock Demo
// Real-time clock with hour, minute, and second hands

scene clock {
    width: 600
    height: 600

    rect background {
        x: 0, y: 0, width: 600, height: 600, fill: #1a1a2e
    }

    // Clock face group (centered at 300, 300)
    group clockFace {
        x: 300, y: 300

        // Outer rim (gold)
        circle outerRim { radius: 210, fill: #d4af37 }

        // Face background (white)
        circle face { radius: 200, fill: #f5f5f5 }

        // Hour markers (Circular dots for a premium look)
        group markers {
            x: 0, y: 0
            
            group m12 { rotation: 0,   circle c { x: 0, y: -185, radius: 7, fill: #2c2c54 } }
            group m1   { rotation: 30,  circle c { x: 0, y: -185, radius: 3, fill: #2c2c54 } }
            group m2   { rotation: 60,  circle c { x: 0, y: -185, radius: 3, fill: #2c2c54 } }
            group m3   { rotation: 90,  circle c { x: 0, y: -185, radius: 7, fill: #2c2c54 } }
            group m4   { rotation: 120, circle c { x: 0, y: -185, radius: 3, fill: #2c2c54 } }
            group m5   { rotation: 150, circle c { x: 0, y: -185, radius: 3, fill: #2c2c54 } }
            group m6   { rotation: 180, circle c { x: 0, y: -185, radius: 7, fill: #2c2c54 } }
            group m7   { rotation: 210, circle c { x: 0, y: -185, radius: 3, fill: #2c2c54 } }
            group m8   { rotation: 240, circle c { x: 0, y: -185, radius: 3, fill: #2c2c54 } }
            group m9   { rotation: 270, circle c { x: 0, y: -185, radius: 7, fill: #2c2c54 } }
            group m10  { rotation: 300, circle c { x: 0, y: -185, radius: 3, fill: #2c2c54 } }
            group m11  { rotation: 330, circle c { x: 0, y: -185, radius: 3, fill: #2c2c54 } }
        }

        // All hands grouped together for unified centering
        group hands {
            x: 0, y: 0

            // Hour hand (pivot at center)
            group hourHand {
                x: 0, y: 0
                rect hourShaft { x: -6, y: -90, width: 12, height: 90, fill: #2c2c54 }
                circle hourTip { x: 0, y: -90, radius: 8, fill: #2c2c54 }
            }

            // Minute hand (pivot at center)
            group minuteHand {
                x: 0, y: 0
                rect minuteShaft { x: -4, y: -130, width: 8, height: 130, fill: #474787 }
                circle minuteTip { x: 0, y: -130, radius: 6, fill: #474787 }
            }

            // Second hand (pivot at center)
            group secondHand {
                x: 0, y: 0
                rect secondShaft { x: -2, y: -150, width: 4, height: 150, fill: #e74c3c }
                circle secondTip { x: 0, y: -150, radius: 5, fill: #e74c3c }
                rect secondTail { x: -3, y: 10, width: 6, height: 40, fill: #e74c3c }
            }
        }

        // Center hub
        circle hub { radius: 15, fill: #d4af37 }
        circle innerHub { radius: 8, fill: #2c2c54 }

        // Brand
        text brand { x: 0, y: 70, content: "FLEX", fontSize: 24, color: #2c2c54 }
    }
}
