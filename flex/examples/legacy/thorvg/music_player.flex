// Music Player UI - Modern Design
// Uses Flex DSL with const, data, flex layout, and position: absolute

// ============================================================================
// LAYOUT CONSTANTS
// ============================================================================

const WINDOW_WIDTH = 420
const WINDOW_HEIGHT = 720

const ALBUM_SIZE = 280
const ALBUM_X = (WINDOW_WIDTH - ALBUM_SIZE) / 2
const ALBUM_Y = 80

const CONTROLS_Y = 520
const PROGRESS_WIDTH = 340
const PROGRESS_X = (WINDOW_WIDTH - PROGRESS_WIDTH) / 2

const VISUALIZER_Y = 420
const VISUALIZER_HEIGHT = 60
const BAR_WIDTH = 8
const BAR_GAP = 6
const BAR_COUNT = 20
const VISUALIZER_WIDTH = BAR_COUNT * (BAR_WIDTH + BAR_GAP)
const VISUALIZER_X = (WINDOW_WIDTH - VISUALIZER_WIDTH) / 2

// ============================================================================
// MODEL - Player State (initial values, updated at runtime via C++ app)
// ============================================================================

// Note: All dynamic values (song info, progress, volume, visualizer bars)
// are updated at runtime by C++ MusicPlayerView class.
// DSL defines the initial layout with default values.

// ============================================================================
// VIEW - UI Scene
// ============================================================================

scene player {
    width: WINDOW_WIDTH
    height: WINDOW_HEIGHT

    // Background gradient layer
    rect background {
        width: WINDOW_WIDTH
        height: WINDOW_HEIGHT
        fill: #0d0d1a
        position: absolute
    }

    // Main content
    group main {
        width: WINDOW_WIDTH
        height: WINDOW_HEIGHT
        layout: flex
        flexDirection: column
        alignItems: center
        padding: 40

        // ----------------------------------------
        // Album Art Section
        // ----------------------------------------
        group album_section {
            width: ALBUM_SIZE
            height: ALBUM_SIZE + 20

            // Glow effect
            circle album_glow {
                x: ALBUM_SIZE / 2
                y: ALBUM_SIZE / 2
                radius: ALBUM_SIZE / 2 + 10
                fill: #8338ec
                opacity: 0.15
            }

            // Album cover (square with rounded corners)
            rect album_cover {
                width: ALBUM_SIZE
                height: ALBUM_SIZE
                fill: #1a1a2e
                cornerRadius: 20
            }

            // Album art gradient overlay
            rect album_gradient {
                x: 10
                y: 10
                width: ALBUM_SIZE - 20
                height: ALBUM_SIZE - 20
                fill: #8338ec
                cornerRadius: 15
                opacity: 0.8
            }

            // Center disc
            group disc {
                x: ALBUM_SIZE / 2
                y: ALBUM_SIZE / 2

                circle disc_outer {
                    radius: 50
                    fill: #1a1a2e
                }
                circle disc_ring {
                    radius: 40
                    stroke: #ff006e
                    strokeWidth: 2
                }
                circle disc_center {
                    radius: 8
                    fill: #00d9ff
                }
            }
        }

        // ----------------------------------------
        // Song Info (static text, updated by C++ MusicPlayerView)
        // ----------------------------------------
        group song_info {
            layout: flex
            flexDirection: column
            alignItems: center
            gap: 8
            height: 80

            text song_title {
                content: "Midnight Dreams"
                fontSize: 24
                color: #ffffff
            }
            text song_artist {
                content: "Synthwave Collective"
                fontSize: 16
                color: #888888
            }
            text song_album {
                content: "Neon Horizons"
                fontSize: 14
                color: #555555
            }
        }

        // ----------------------------------------
        // Audio Visualizer (heights updated dynamically by C++ MusicPlayerView)
        // ----------------------------------------
        group visualizer_section {
            width: VISUALIZER_WIDTH
            height: VISUALIZER_HEIGHT

            // Bars - fixed x positions, initial heights (updated at runtime)
            rect vbar0  { x: 0,   y: 33, width: BAR_WIDTH, height: 27, fill: #00d9ff }
            rect vbar1  { x: 14,  y: 21, width: BAR_WIDTH, height: 39, fill: #00d9ff }
            rect vbar2  { x: 28,  y: 9,  width: BAR_WIDTH, height: 51, fill: #00e5ff }
            rect vbar3  { x: 42,  y: 18, width: BAR_WIDTH, height: 42, fill: #00e5ff }
            rect vbar4  { x: 56,  y: 3,  width: BAR_WIDTH, height: 57, fill: #8338ec }
            rect vbar5  { x: 70,  y: 12, width: BAR_WIDTH, height: 48, fill: #8338ec }
            rect vbar6  { x: 84,  y: 24, width: BAR_WIDTH, height: 36, fill: #9c4dff }
            rect vbar7  { x: 98,  y: 6,  width: BAR_WIDTH, height: 54, fill: #9c4dff }
            rect vbar8  { x: 112, y: 15, width: BAR_WIDTH, height: 45, fill: #ff006e }
            rect vbar9  { x: 126, y: 27, width: BAR_WIDTH, height: 33, fill: #ff006e }
            rect vbar10 { x: 140, y: 18, width: BAR_WIDTH, height: 42, fill: #ff006e }
            rect vbar11 { x: 154, y: 9,  width: BAR_WIDTH, height: 51, fill: #9c4dff }
            rect vbar12 { x: 168, y: 21, width: BAR_WIDTH, height: 39, fill: #9c4dff }
            rect vbar13 { x: 182, y: 3,  width: BAR_WIDTH, height: 57, fill: #8338ec }
            rect vbar14 { x: 196, y: 30, width: BAR_WIDTH, height: 30, fill: #8338ec }
            rect vbar15 { x: 210, y: 12, width: BAR_WIDTH, height: 48, fill: #00e5ff }
            rect vbar16 { x: 224, y: 18, width: BAR_WIDTH, height: 42, fill: #00e5ff }
            rect vbar17 { x: 238, y: 24, width: BAR_WIDTH, height: 36, fill: #00d9ff }
            rect vbar18 { x: 252, y: 33, width: BAR_WIDTH, height: 27, fill: #00d9ff }
            rect vbar19 { x: 266, y: 39, width: BAR_WIDTH, height: 21, fill: #00d9ff }
        }

        // ----------------------------------------
        // Progress Bar
        // ----------------------------------------
        group progress_section {
            width: PROGRESS_WIDTH
            height: 50
            layout: flex
            flexDirection: column
            gap: 8

            // Time labels row
            group time_row {
                layout: flex
                flexDirection: row
                justifyContent: spaceBetween
                width: PROGRESS_WIDTH
                height: 16

                text time_current { content: "1:13", fontSize: 12, color: #888888 }
                text time_total { content: "4:05", fontSize: 12, color: #888888 }
            }

            // Progress track
            group progress_track {
                width: PROGRESS_WIDTH
                height: 6

                rect track_bg {
                    width: PROGRESS_WIDTH
                    height: 6
                    fill: #2c2c54
                    cornerRadius: 3
                    position: absolute
                }

                rect track_fill {
                    width: 102
                    height: 6
                    fill: #00d9ff
                    cornerRadius: 3
                    position: absolute
                }

                circle playhead {
                    x: 102
                    y: 3
                    radius: 8
                    fill: #ffffff
                }
            }
        }

        // ----------------------------------------
        // Control Buttons
        // ----------------------------------------
        group controls_section {
            layout: flex
            flexDirection: row
            alignItems: center
            justifyContent: center
            gap: 20
            height: 80
            width: 340

            // Shuffle button (40x40) - crossed lines with arrows
            group btn_shuffle {
                width: 40, height: 40
                circle btn_shuffle_bg { x: 20, y: 20, radius: 20, fill: #1a1a2e }
                // Simple X cross pattern
                path shuffle_line1 { x: 13, y: 15, d: "M 0 0 L 12 10 L 14 8 L 2 -2 Z", fill: #666666 }
                path shuffle_line2 { x: 13, y: 15, d: "M 0 10 L 12 0 L 14 2 L 2 12 Z", fill: #666666 }
                // Right arrows
                path shuffle_arr1 { x: 24, y: 14, d: "M 0 0 L 4 2 L 0 4 Z", fill: #666666 }
                path shuffle_arr2 { x: 24, y: 22, d: "M 0 0 L 4 2 L 0 4 Z", fill: #666666 }
            }

            // Previous button (50x50) - |◀◀ skip back
            group btn_prev {
                width: 50, height: 50
                circle btn_prev_bg { x: 25, y: 25, radius: 25, fill: #2c2c54 }
                // Bar on left
                rect prev_bar { x: 14, y: 18, width: 3, height: 14, fill: #ffffff }
                // Two triangles pointing left (SVG path)
                path prev_tri1 { x: 21, y: 18, d: "M 0 7 L 8 0 L 8 14 Z", fill: #ffffff }
                path prev_tri2 { x: 29, y: 18, d: "M 0 7 L 8 0 L 8 14 Z", fill: #ffffff }
            }

            // Play/Pause button (70x70)
            group btn_play {
                width: 70, height: 70
                circle btn_play_bg { x: 35, y: 35, radius: 35, fill: #00d9ff }
                // Pause icon (two vertical bars)
                rect pause_bar1 { x: 26, y: 23, width: 7, height: 24, fill: #0d0d1a }
                rect pause_bar2 { x: 37, y: 23, width: 7, height: 24, fill: #0d0d1a }
            }

            // Next button (50x50) - ▶▶| skip forward
            group btn_next {
                width: 50, height: 50
                circle btn_next_bg { x: 25, y: 25, radius: 25, fill: #2c2c54 }
                // Two triangles pointing right (SVG path)
                path next_tri1 { x: 13, y: 18, d: "M 0 0 L 8 7 L 0 14 Z", fill: #ffffff }
                path next_tri2 { x: 21, y: 18, d: "M 0 0 L 8 7 L 0 14 Z", fill: #ffffff }
                // Bar on right
                rect next_bar { x: 33, y: 18, width: 3, height: 14, fill: #ffffff }
            }

            // Repeat button (40x40) - loop/cycle icon
            group btn_repeat {
                width: 40, height: 40
                circle btn_repeat_bg { x: 20, y: 20, radius: 20, fill: #1a1a2e }
                // Loop rectangle outline
                path repeat_rect { x: 12, y: 14, d: "M 2 0 L 14 0 L 14 2 L 2 2 Z M 14 0 L 16 0 L 16 12 L 14 12 Z M 2 10 L 14 10 L 14 12 L 2 12 Z M 0 0 L 2 0 L 2 12 L 0 12 Z", fill: #666666 }
                // Arrow on right pointing right
                path repeat_arr_r { x: 25, y: 14, d: "M 0 0 L 4 2 L 0 4 Z", fill: #666666 }
                // Arrow on left pointing left
                path repeat_arr_l { x: 11, y: 22, d: "M 4 0 L 0 2 L 4 4 Z", fill: #666666 }
            }
        }

        // ----------------------------------------
        // Volume Control
        // ----------------------------------------
        group volume_section {
            layout: flex
            flexDirection: row
            alignItems: center
            justifyContent: center
            gap: 12
            width: 200
            height: 30

            // Volume icon
            rect volume_icon { width: 16, height: 12, fill: #666666 }

            // Volume slider track
            group volume_track {
                width: 120
                height: 4

                rect vol_bg {
                    width: 120
                    height: 4
                    fill: #2c2c54
                    cornerRadius: 2
                    position: absolute
                }

                rect vol_fill {
                    width: 90
                    height: 4
                    fill: #ffffff
                    cornerRadius: 2
                    position: absolute
                }

                circle vol_thumb {
                    x: 90
                    y: 2
                    radius: 6
                    fill: #ffffff
                }
            }

            // Volume percentage
            text volume_value { content: "75%", fontSize: 12, color: #666666 }
        }
    }
}

// ============================================================================
// ANIMATIONS
// ============================================================================

// Album disc rotation
anim "DiscSpin" {
    duration: 8
    loop: loop

    track "#disc/rotation" {
        keyframe 0 -> 0
        keyframe 8 -> 360
    }
}

// Visualizer bar pulse animations
anim "VisualizerPulse" {
    duration: 0.5
    loop: pingpong

    track "#vbar4/height" {
        keyframe 0 -> 40
        keyframe 0.5 -> 60
    }
    track "#vbar9/height" {
        keyframe 0 -> 35
        keyframe 0.5 -> 55
    }
}

// Glow pulse
anim "GlowPulse" {
    duration: 2
    loop: pingpong

    track "#album_glow/opacity" {
        keyframe 0 -> 0.1
        keyframe 2 -> 0.25
    }
}
