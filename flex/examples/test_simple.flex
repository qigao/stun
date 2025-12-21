// Simple test scene for the simplified parser

scene game {
    width: 800
    height: 600

    rect player {
        x: 100
        y: 450
        width: 40
        height: 80
        color: white
    }

    rect ground {
        x: 400
        y: 580
        width: 800
        height: 40
        color: green
    }

    circle coin {
        x: 300
        y: 400
        radius: 20
        color: blue
    }
}

anim "PlayerMove" {
    duration: 2
    loop: loop

    track "x" {
        keyframe 0 -> 100
        keyframe 1 -> 500
        keyframe 2 -> 100
    }

    track "y" {
        keyframe 0 -> 450
        keyframe 1 -> 350
        keyframe 2 -> 450
    }
}

anim "CoinSpin" {
    duration: 1
    loop: loop

    track "rotation" {
        keyframe 0 -> 0
        keyframe 0.5 -> 180
        keyframe 1 -> 360
    }
}
