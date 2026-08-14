// Dashboard MVC Demo
// Model-View-Controller pattern using Flex DSL
// Demonstrates: data as Model, scene as View, machine as Controller
//
// Architecture:
//   Model      = data blocks (dashboard_model, metrics, notifications)
//   View       = scene with components bound to model via ${}
//   Controller = state machine handling user actions and state transitions

// ============================================================================
// MODEL - Data that drives the UI
// ============================================================================

// Core dashboard model - single source of truth
data dashboard_model {
    config: {
        title: "Analytics Dashboard",
        refreshRate: 5,
        theme: "dark"
    }
    user: {
        name: "Admin",
        role: "Administrator",
        avatar: "#4cc9f0"
    }
}

// Metrics model - business data
data metrics {
    revenue:   { label: "Revenue",     value: "$45,231", delta: "+20%", trend: "up",   color: "#00d9ff" }
    users:     { label: "Active Users", value: "2,540",   delta: "+12%", trend: "up",   color: "#00ff88" }
    bounce:    { label: "Bounce Rate",  value: "42.3%",   delta: "-5%",  trend: "down", color: "#ff006e" }
    sessions:  { label: "Sessions",     value: "8,420",   delta: "+8%",  trend: "up",   color: "#8338ec" }
}

// Chart data model
data chart_data {
    mon: { day: "Mon", value: 120, percent: 48 }
    tue: { day: "Tue", value: 180, percent: 72 }
    wed: { day: "Wed", value: 240, percent: 96 }
    thu: { day: "Thu", value: 160, percent: 64 }
    fri: { day: "Fri", value: 250, percent: 100 }
    sat: { day: "Sat", value: 200, percent: 80 }
    sun: { day: "Sun", value: 140, percent: 56 }
}

// Notifications model
data notifications {
    n1: { type: "success", message: "Data synced successfully", time: "2m ago" }
    n2: { type: "warning", message: "High memory usage detected", time: "15m ago" }
    n3: { type: "info",    message: "New user registered", time: "1h ago" }
}

// Navigation model
data nav_model {
    dashboard:  { label: "Dashboard",  icon: "#00d9ff", active: true }
    analytics:  { label: "Analytics",  icon: "#4895ef", active: false }
    reports:    { label: "Reports",    icon: "#4361ee", active: false }
    settings:   { label: "Settings",   icon: "#3f37c9", active: false }
}

// ============================================================================
// LAYOUT CONSTANTS
// ============================================================================

const WINDOW_WIDTH = 1280
const WINDOW_HEIGHT = 800
const SIDEBAR_WIDTH = 260
const CONTENT_PADDING = 32
const CONTENT_GAP = 24

const CONTENT_WIDTH = WINDOW_WIDTH - SIDEBAR_WIDTH
const CONTENT_INNER = CONTENT_WIDTH - (CONTENT_PADDING * 2)
const METRIC_CARD_WIDTH = (CONTENT_INNER - 60) / 4
const CHART_PANEL_WIDTH = (CONTENT_INNER - CONTENT_GAP) * 2 / 3
const NOTIF_PANEL_WIDTH = (CONTENT_INNER - CONTENT_GAP) / 3
const NOTIF_ITEM_WIDTH = NOTIF_PANEL_WIDTH - 48
const CHART_BAR_MAX_HEIGHT = 150

// ============================================================================
// VIEW - UI rendered from model data (Light Theme)
// ============================================================================

scene dashboard_view {
    width: WINDOW_WIDTH
    height: WINDOW_HEIGHT

    // Background layer - light gray
    rect background {
        width: WINDOW_WIDTH, height: WINDOW_HEIGHT, fill: #f0f2f5
    }

    group app {
        width: WINDOW_WIDTH, height: WINDOW_HEIGHT
        layout: flex
        flexDirection: row

        // ----------------------------------------
        // Sidebar (Navigation View)
        // ----------------------------------------
        group sidebar {
            width: SIDEBAR_WIDTH, height: WINDOW_HEIGHT
            layout: flex
            flexDirection: column
            padding: 24
            gap: 32

            rect sidebar_bg { width: SIDEBAR_WIDTH, height: WINDOW_HEIGHT, fill: #ffffff, position: absolute }

            // Logo
            group logo {
                layout: flex
                flexDirection: row
                alignItems: center
                gap: 12

                circle logo_dot { radius: 8, fill: #3b82f6 }
                text logo_text { content: "FLEX.MVC", fontSize: 24, color: #1f2937 }
            }

            // Navigation items from model
            group nav {
                layout: flex
                flexDirection: column
                gap: 8
                height: 200

                for item in nav_model {
                    group nav_item {
                        width: 212, height: 44
                        layout: flex
                        flexDirection: row
                        alignItems: center
                        gap: 12
                        padding: 12

                        rect nav_bg {
                            width: 212, height: 44,
                            fill: #f3f4f6,
                            opacity: 0.5,
                            position: absolute
                        }

                        circle nav_icon { radius: 4, fill: ${item.icon} }
                        text nav_label {
                            content: ${item.label},
                            fontSize: 15,
                            color: #4b5563
                        }
                    }
                }
            }

            // User profile from model
            group user_profile {
                y: 650
                layout: flex
                flexDirection: row
                alignItems: center
                gap: 12

                circle avatar { radius: 18, fill: #3b82f6 }
                group user_info {
                    layout: flex
                    flexDirection: column
                    gap: 2

                    text user_name { content: "Admin", fontSize: 14, color: #1f2937 }
                    text user_role { content: "Administrator", fontSize: 12, color: #6b7280 }
                }
            }
        }

        // ----------------------------------------
        // Main Content Area
        // ----------------------------------------
        group main_content {
            width: CONTENT_WIDTH, height: WINDOW_HEIGHT
            layout: flex
            flexDirection: column
            padding: CONTENT_PADDING
            gap: CONTENT_GAP

            // Header bar
            group header {
                layout: flex
                flexDirection: row
                justifyContent: spaceBetween
                alignItems: center
                width: CONTENT_INNER, height: 48

                group header_left {
                    layout: flex
                    flexDirection: column
                    gap: 4

                    text page_title {
                        content: "Analytics Dashboard",
                        fontSize: 28,
                        color: #1f2937
                    }
                    text page_subtitle {
                        content: "Real-time metrics and insights",
                        fontSize: 14,
                        color: #6b7280
                    }
                }

                group header_right {
                    layout: flex
                    flexDirection: row
                    alignItems: center
                    gap: 16

                    // Refresh indicator
                    group refresh_indicator {
                        opacity: 0
                        circle spinner { radius: 8, stroke: #3b82f6, strokeWidth: 2 }
                    }

                    // Notification badge
                    group notification_badge {
                        circle badge_bg { radius: 16, fill: #e5e7eb }
                        text badge_count { content: "3", fontSize: 12, color: #ef4444 }
                    }
                }
            }

            // ----------------------------------------
            // Metrics Cards (View bound to metrics model)
            // ----------------------------------------
            group metrics_row {
                layout: flex
                flexDirection: row
                gap: 20
                width: CONTENT_INNER
                height: 140

                for metric in metrics {
                    group metric_card {
                        width: METRIC_CARD_WIDTH, height: 140
                        layout: flex
                        flexDirection: column
                        justifyContent: spaceBetween
                        padding: 20
                        opacity: 0

                        rect card_bg {
                            width: METRIC_CARD_WIDTH, height: 140,
                            fill: #ffffff,
                            position: absolute
                        }

                        // Metric header
                        group metric_header {
                            layout: flex
                            flexDirection: row
                            justifyContent: spaceBetween
                            alignItems: center

                            text metric_label {
                                content: ${metric.label},
                                fontSize: 14,
                                color: #6b7280
                            }
                            circle trend_dot { radius: 4, fill: ${metric.color} }
                        }

                        // Metric value
                        text metric_value {
                            content: ${metric.value},
                            fontSize: 32,
                            color: #1f2937
                        }

                        // Metric delta
                        text metric_delta {
                            content: ${metric.delta},
                            fontSize: 14,
                            color: ${metric.color}
                        }
                    }
                }
            }

            // ----------------------------------------
            // Chart Section (View bound to chart_data model)
            // ----------------------------------------
            group chart_section {
                layout: flex
                flexDirection: row
                gap: CONTENT_GAP
                width: CONTENT_INNER
                height: 320

                // Bar chart
                group chart_panel {
                    width: CHART_PANEL_WIDTH, height: 320
                    layout: flex
                    flexDirection: column
                    padding: 24
                    gap: 16

                    rect chart_bg {
                        width: CHART_PANEL_WIDTH, height: 320,
                        fill: #ffffff,
                        position: absolute
                    }

                    text chart_title {
                        content: "Weekly Traffic",
                        fontSize: 18,
                        color: #1f2937
                    }

                    // Chart component - separates bar drawing from labels
                    // Bars at fixed x, y = baseline - height (so bar grows upward from baseline)
                    group chart_area {
                        width: 500
                        height: CHART_BAR_MAX_HEIGHT

                        // y = baseline - (percent/100 * maxHeight) = maxHeight * (1 - percent/100)
                        rect bar0 { x: 20,  y: CHART_BAR_MAX_HEIGHT * (100 - ${chart_data.mon.percent}) / 100, width: 48, height: ${chart_data.mon.percent} / 100 * CHART_BAR_MAX_HEIGHT, fill: #3b82f6 }
                        rect bar1 { x: 84,  y: CHART_BAR_MAX_HEIGHT * (100 - ${chart_data.tue.percent}) / 100, width: 48, height: ${chart_data.tue.percent} / 100 * CHART_BAR_MAX_HEIGHT, fill: #3b82f6 }
                        rect bar2 { x: 148, y: CHART_BAR_MAX_HEIGHT * (100 - ${chart_data.wed.percent}) / 100, width: 48, height: ${chart_data.wed.percent} / 100 * CHART_BAR_MAX_HEIGHT, fill: #3b82f6 }
                        rect bar3 { x: 212, y: CHART_BAR_MAX_HEIGHT * (100 - ${chart_data.thu.percent}) / 100, width: 48, height: ${chart_data.thu.percent} / 100 * CHART_BAR_MAX_HEIGHT, fill: #3b82f6 }
                        rect bar4 { x: 276, y: CHART_BAR_MAX_HEIGHT * (100 - ${chart_data.fri.percent}) / 100, width: 48, height: ${chart_data.fri.percent} / 100 * CHART_BAR_MAX_HEIGHT, fill: #3b82f6 }
                        rect bar5 { x: 340, y: CHART_BAR_MAX_HEIGHT * (100 - ${chart_data.sat.percent}) / 100, width: 48, height: ${chart_data.sat.percent} / 100 * CHART_BAR_MAX_HEIGHT, fill: #3b82f6 }
                        rect bar6 { x: 404, y: CHART_BAR_MAX_HEIGHT * (100 - ${chart_data.sun.percent}) / 100, width: 48, height: ${chart_data.sun.percent} / 100 * CHART_BAR_MAX_HEIGHT, fill: #3b82f6 }
                    }

                    // X-axis labels - fixed row layout, independent of bar heights
                    group chart_labels {
                        layout: flex
                        flexDirection: row
                        justifyContent: spaceAround
                        width: 500
                        height: 24

                        text label0 { content: "Mon", fontSize: 12, color: #6b7280 }
                        text label1 { content: "Tue", fontSize: 12, color: #6b7280 }
                        text label2 { content: "Wed", fontSize: 12, color: #6b7280 }
                        text label3 { content: "Thu", fontSize: 12, color: #6b7280 }
                        text label4 { content: "Fri", fontSize: 12, color: #6b7280 }
                        text label5 { content: "Sat", fontSize: 12, color: #6b7280 }
                        text label6 { content: "Sun", fontSize: 12, color: #6b7280 }
                    }
                }

                // Notifications panel (View bound to notifications model)
                group notifications_panel {
                    width: NOTIF_PANEL_WIDTH, height: 320
                    layout: flex
                    flexDirection: column
                    padding: 24
                    gap: 16

                    rect notif_bg {
                        width: NOTIF_PANEL_WIDTH, height: 320,
                        fill: #ffffff,
                        position: absolute
                    }

                    text notif_title {
                        content: "Recent Activity",
                        fontSize: 18,
                        color: #1f2937
                    }

                    group notif_list {
                        layout: flex
                        flexDirection: column
                        gap: 12
                        height: 216

                        for notif in notifications {
                            group notif_item {
                                layout: flex
                                flexDirection: column
                                gap: 4
                                padding: 12
                                width: NOTIF_ITEM_WIDTH
                                height: 60

                                rect notif_item_bg {
                                    width: NOTIF_ITEM_WIDTH, height: 60
                                    fill: #f9fafb
                                    position: absolute
                                }

                                text notif_msg {
                                    content: ${notif.message},
                                    fontSize: 13,
                                    color: #374151
                                }
                                text notif_time {
                                    content: ${notif.time},
                                    fontSize: 11,
                                    color: #9ca3af
                                }
                            }
                        }
                    }
                }
            }

            // Status bar
            group status_bar {
                layout: flex
                flexDirection: row
                justifyContent: spaceBetween
                alignItems: center
                width: CONTENT_INNER, height: 32

                group status_left {
                    layout: flex
                    flexDirection: row
                    alignItems: center
                    gap: 8

                    circle status_dot { radius: 4, fill: #22c55e }
                    text status_text {
                        content: "All systems operational",
                        fontSize: 12,
                        color: #6b7280
                    }
                }

                text last_update {
                    content: "Last updated: just now",
                    fontSize: 12,
                    color: #9ca3af
                }
            }
        }
    }

    // Toast notification overlay
    group toast {
        x: WINDOW_WIDTH - 320, y: WINDOW_HEIGHT - 80
        opacity: 0

        rect toast_bg { width: 280, height: 56, fill: #22c55e }
        text toast_msg {
            x: 16, y: 18,
            content: "Data refreshed successfully",
            fontSize: 14,
            color: #ffffff
        }
    }
}

// ============================================================================
// CONTROLLER - State machine managing UI states and transitions
// ============================================================================

machine dashboard_controller {
    // Data loading layer
    layer data_layer {
        state idle {
            initial: true
        }

        state loading {
            animation: "loadingSpinner"
        }

        state loaded {
            animation: "showContent"
        }

        state error {
            animation: "showError"
        }

        transition idle -> loading when fetchData > 0
        transition loading -> loaded when dataReady > 0
        transition loading -> error when dataError > 0
        transition loaded -> loading when refresh > 0
        transition error -> loading when retry > 0
    }

    // UI interaction layer
    layer ui_layer {
        state default {
            initial: true
        }

        state nav_hover {
            animation: "navHighlight"
        }

        state card_hover {
            animation: "cardHighlight"
        }

        transition default -> nav_hover when navHover > 0
        transition nav_hover -> default when navHover < 1
        transition default -> card_hover when cardHover > 0
        transition card_hover -> default when cardHover < 1
    }

    // Notification layer
    layer notification_layer {
        state hidden {
            initial: true
        }

        state visible {
            animation: "showToast"
        }

        transition hidden -> visible when showNotification > 0
        transition visible -> hidden when showNotification < 1
    }

    // Theme layer
    layer theme_layer {
        state dark {
            initial: true
        }

        state light {
            animation: "switchToLight"
        }

        transition dark -> light when themeLight > 0
        transition light -> dark when themeLight < 1
    }
}

// ============================================================================
// ANIMATIONS - Visual feedback for state changes
// ============================================================================

// Loading spinner rotation
anim "loadingSpinner" {
    duration: 1.0
    loop: loop
    track "#refresh_indicator/opacity" {
        keyframe 0 -> 1
    }
    track "#spinner/rotation" {
        keyframe 0 -> 0
        keyframe 1.0 -> 360
    }
}

// Show content with staggered animation
anim "showContent" {
    duration: 0.8

    // Hide loading
    track "#refresh_indicator/opacity" {
        keyframe 0 -> 1
        keyframe 0.2 -> 0
    }

    // Staggered card reveal
    track "#metric_card0/opacity" {
        keyframe 0 -> 0
        keyframe 0.2 -> 1
    }
    track "#metric_card1/opacity" {
        keyframe 0.1 -> 0
        keyframe 0.3 -> 1
    }
    track "#metric_card2/opacity" {
        keyframe 0.2 -> 0
        keyframe 0.4 -> 1
    }
    track "#metric_card3/opacity" {
        keyframe 0.3 -> 0
        keyframe 0.5 -> 1
    }

    // Staggered bar reveal
    track "#bar0/opacity" {
        keyframe 0.4 -> 0
        keyframe 0.5 -> 1
    }
    track "#bar1/opacity" {
        keyframe 0.45 -> 0
        keyframe 0.55 -> 1
    }
    track "#bar2/opacity" {
        keyframe 0.5 -> 0
        keyframe 0.6 -> 1
    }
    track "#bar3/opacity" {
        keyframe 0.55 -> 0
        keyframe 0.65 -> 1
    }
    track "#bar4/opacity" {
        keyframe 0.6 -> 0
        keyframe 0.7 -> 1
    }
    track "#bar5/opacity" {
        keyframe 0.65 -> 0
        keyframe 0.75 -> 1
    }
    track "#bar6/opacity" {
        keyframe 0.7 -> 0
        keyframe 0.8 -> 1
    }
}

// Error state animation
anim "showError" {
    duration: 0.5
    track "#status_dot/fill" {
        keyframe 0 -> #da3633
    }
    track "#status_text/content" {
        keyframe 0 -> "Connection error"
    }
    track "#status_text/color" {
        keyframe 0 -> #da3633
    }
}

// Navigation highlight
anim "navHighlight" {
    duration: 0.15
    track "opacity" {
        keyframe 0 -> 0.5
        keyframe 0.15 -> 1.0
    }
}

// Card highlight
anim "cardHighlight" {
    duration: 0.2
    track "scale" {
        keyframe 0 -> 1.0
        keyframe 0.2 -> 1.02
    }
}

// Toast notification
anim "showToast" {
    duration: 3.0
    track "#toast/opacity" {
        keyframe 0 -> 0
        keyframe 0.2 -> 1
        keyframe 2.5 -> 1
        keyframe 3.0 -> 0
    }
    track "#toast/y" {
        keyframe 0 -> WINDOW_HEIGHT
        keyframe 0.2 -> WINDOW_HEIGHT - 80
        keyframe 2.5 -> WINDOW_HEIGHT - 80
        keyframe 3.0 -> WINDOW_HEIGHT
    }
}

// Theme switch animation (light to dark)
anim "switchToDark" {
    duration: 0.3
    track "#background/fill" {
        keyframe 0 -> #f0f2f5
        keyframe 0.3 -> #0d1117
    }
    track "#sidebar_bg/fill" {
        keyframe 0 -> #ffffff
        keyframe 0.3 -> #161b22
    }
}
