// Analog Clock Demo
// Real-time clock with hour, minute, and second hands

scene clock {
    width: 600
    height: 600

    // Dark background
    rect background {
        x: 0, y: 0
        width: 600, height: 600
        fill: #1a1a2e
    }

    // Clock face group (centered)
    group clockFace {
        x: 300, y: 300

        // Outer rim (gold)
        circle outerRim {
            x: 0, y: 0
            radius: 210
            fill: #d4af37
        }

        // Face background (white)
        circle face {
            x: 0, y: 0
            radius: 200
            fill: #f5f5f5
        }

        // Hour markers group
        group hourMarkers {
            x: 0, y: 0

            // 12 o'clock
            rect mark12 {
                x: -4, y: -180
                width: 8, height: 30
                fill: #2c2c54
            }

            // 3 o'clock
            rect mark3 {
                x: 150, y: -4
                width: 30, height: 8
                fill: #2c2c54
                rotation: 0
            }

            // 6 o'clock
            rect mark6 {
                x: -4, y: 150
                width: 8, height: 30
                fill: #2c2c54
            }

            // 9 o'clock
            rect mark9 {
                x: -180, y: -4
                width: 30, height: 8
                fill: #2c2c54
            }

            // Smaller markers for 1,2,4,5,7,8,10,11
            rect mark1 {
                x: -3, y: -175
                width: 6, height: 20
                fill: #7f8fa6
                rotation: 30
            }

            rect mark2 {
                x: -3, y: -175
                width: 6, height: 20
                fill: #7f8fa6
                rotation: 60
            }

            rect mark4 {
                x: -3, y: -175
                width: 6, height: 20
                fill: #7f8fa6
                rotation: 120
            }

            rect mark5 {
                x: -3, y: -175
                width: 6, height: 20
                fill: #7f8fa6
                rotation: 150
            }

            rect mark7 {
                x: -3, y: -175
                width: 6, height: 20
                fill: #7f8fa6
                rotation: 210
            }

            rect mark8 {
                x: -3, y: -175
                width: 6, height: 20
                fill: #7f8fa6
                rotation: 240
            }

            rect mark10 {
                x: -3, y: -175
                width: 6, height: 20
                fill: #7f8fa6
                rotation: 300
            }

            rect mark11 {
                x: -3, y: -175
                width: 6, height: 20
                fill: #7f8fa6
                rotation: 330
            }
        }

        // Hour hand (short, thick)
        group hourHand {
            x: 0, y: 0
            rotation: 0

            rect shaft {
                x: -6, y: -90
                width: 12, height: 90
                fill: #2c2c54
            }

            circle tip {
                x: 0, y: -90
                radius: 8
                fill: #2c2c54
            }
        }

        // Minute hand (longer, thinner)
        group minuteHand {
            x: 0, y: 0
            rotation: 0

            rect shaft {
                x: -4, y: -130
                width: 8, height: 130
                fill: #474787
            }

            circle tip {
                x: 0, y: -130
                radius: 6
                fill: #474787
            }
        }

        // Second hand (longest, thinnest, red)
        group secondHand {
            x: 0, y: 0
            rotation: 0

            rect shaft {
                x: -2, y: -150
                width: 4, height: 150
                fill: #e74c3c
            }

            circle tip {
                x: 0, y: -150
                radius: 5
                fill: #e74c3c
            }

            // Counterweight
            rect counterweight {
                x: -3, y: 10
                width: 6, height: 30
                fill: #e74c3c
            }
        }

        // Center hub (covers hand bases)
        circle hub {
            x: 0, y: 0
            radius: 15
            fill: #d4af37
        }

        circle innerHub {
            x: 0, y: 0
            radius: 8
            fill: #2c2c54
        }

        // Brand text
        text brand {
            x: 0, y: 60
            content: "FLEX"
            fontSize: 20
            color: #2c2c54
        }
    }
}

// Second hand animation (60 seconds full rotation)
anim "SecondTick" {
    duration: 60
    loop: loop

    track "rotation" {
        keyframe 0 -> 0
        keyframe 60 -> 360
    }
}

// Minute hand animation (3600 seconds = 60 minutes)
anim "MinuteTick" {
    duration: 3600
    loop: loop

    track "rotation" {
        keyframe 0 -> 0
        keyframe 3600 -> 360
    }
}

// Hour hand animation (43200 seconds = 12 hours)
anim "HourTick" {
    duration: 43200
    loop: loop

    track "rotation" {
        keyframe 0 -> 0
        keyframe 43200 -> 360
    }
}
