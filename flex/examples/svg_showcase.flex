scene SvgShowcase {
    width: 900
    height: 640

    text heading {
        x: 48
        y: 42
        content: "Flex + PlutoSVG"
        fontFamily: "sans-serif"
        fontSize: 26
        color: #ffffff
    }

    text subtitle {
        x: 48
        y: 78
        content: "file svg and inline svg rendered through the NanoVG backend"
        fontFamily: "sans-serif"
        fontSize: 14
        color: #b9c2d0
    }

    svg fileBadge {
        x: 48
        y: 120
        width: 220
        height: 220
        src: "nanovg_badge.svg"
    }

    svg inlineBadge {
        x: 320
        y: 120
        width: 220
        height: 220
        data: "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 120 120'><defs><linearGradient id='bg' x1='0' y1='0' x2='1' y2='1'><stop offset='0%' stop-color='#0f172a'/><stop offset='100%' stop-color='#1d4ed8'/></linearGradient></defs><rect x='10' y='10' width='100' height='100' rx='22' fill='url(#bg)'/><circle cx='60' cy='60' r='24' fill='#f8fafc'/><path d='M43 78 L60 32 L77 78 Z' fill='#fb7185'/></svg>"
    }

    rect frame {
        x: 30
        y: 104
        width: 530
        height: 252
        radius: 24
        stroke: #3b4252
        strokeWidth: 2
    }
}
