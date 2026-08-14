// Analog Clock Demo
// Animated clock with rotating hour, minute, and second hands
// Demonstrates: const, data blocks, for loops, animations

// Clock dimensions as constants
const centerX = 300
const centerY = 300
const faceRadius = 200
const markerOffset = 15
const markerY = 0 - (faceRadius - markerOffset)

// Hand lengths (percentage of face radius)
const hourHandLen = faceRadius * 0.45
const minuteHandLen = faceRadius * 0.65
const secondHandLen = faceRadius * 0.75

data hour_markers {
    h12: { rot: 0,   r: 7 }
    h1:  { rot: 30,  r: 3 }
    h2:  { rot: 60,  r: 3 }
    h3:  { rot: 90,  r: 7 }
    h4:  { rot: 120, r: 3 }
    h5:  { rot: 150, r: 3 }
    h6:  { rot: 180, r: 7 }
    h7:  { rot: 210, r: 3 }
    h8:  { rot: 240, r: 3 }
    h9:  { rot: 270, r: 7 }
    h10: { rot: 300, r: 3 }
    h11: { rot: 330, r: 3 }
}

scene clock {
    width: 600
    height: 600

    rect background {
        x: 0, y: 0, width: 600, height: 600, fill: #1a1a2e
    }

    group clockFace {
        x: centerX, y: centerY

        // Rim is face radius + 10
        circle outerRim { radius: faceRadius + 10, fill: #d4af37 }
        circle face { radius: faceRadius, fill: #f5f5f5 }

        // Hour markers using for loop - markerY is pre-calculated constant
        group markers {
            for m in hour_markers {
                group marker {
                    rotation: ${m.rot}
                    circle c { x: 0, y: markerY, radius: ${m.r}, fill: #2c2c54 }
                }
            }
        }

        // Animated hands with calculated dimensions from constants
        group hourHand {
            rotation: 0
            rect shaft { x: -6, y: 0 - hourHandLen, width: 12, height: hourHandLen, fill: #2c2c54 }
            circle tip { x: 0, y: 0 - hourHandLen, radius: 8, fill: #2c2c54 }
        }

        group minuteHand {
            rotation: 0
            rect shaft { x: -4, y: 0 - minuteHandLen, width: 8, height: minuteHandLen, fill: #474787 }
            circle tip { x: 0, y: 0 - minuteHandLen, radius: 6, fill: #474787 }
        }

        group secondHand {
            rotation: 0
            rect shaft { x: -2, y: 0 - secondHandLen, width: 4, height: secondHandLen, fill: #e74c3c }
            circle tip { x: 0, y: 0 - secondHandLen, radius: 5, fill: #e74c3c }
            rect tail { x: -3, y: 10, width: 6, height: 40, fill: #e74c3c }
        }

        circle hub { radius: 15, fill: #d4af37 }
        circle innerHub { radius: 8, fill: #2c2c54 }
        text brand { x: 0, y: 70, content: "FLEX", fontSize: 24, color: #2c2c54 }
    }
}

// Second hand: 60 seconds = full rotation
anim "tickSecond" {
    duration: 60
    loop: loop
    track "#secondHand/rotation" {
        keyframe 0 -> 0
        keyframe 60 -> 360
    }
}

// Minute hand: 60 minutes = full rotation (3600 seconds)
anim "tickMinute" {
    duration: 3600
    loop: loop
    track "#minuteHand/rotation" {
        keyframe 0 -> 0
        keyframe 3600 -> 360
    }
}

// Hour hand: 12 hours = full rotation (43200 seconds)
anim "tickHour" {
    duration: 43200
    loop: loop
    track "#hourHand/rotation" {
        keyframe 0 -> 0
        keyframe 43200 -> 360
    }
}
