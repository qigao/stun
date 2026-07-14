// Component props binding demo
// ${$input} is an expression binding that references inputs.

scene ComponentBindingDemo {
    BadgeBinding badge {
        x: 20
        y: 20
        width: ${$badgeW}
        height: ${$badgeH}
    }

    rect box {
        x: 20
        y: 80
        width: ${$boxW}
        height: ${$boxH}
    }
}
