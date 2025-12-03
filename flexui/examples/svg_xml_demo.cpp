#include <flexui/screen.h>

int main() {
    flexui::Screen screen(800, 600, "SVG XML Demo - RFC 7991/7996");

    // Load CSS for styling SVG elements
    std::string css = R"(
        :root {
            --primary: #3498db;
            --secondary: #e74c3c;
            --success: #2ecc71;
            --warning: #f39c12;
        }

        #root {
            display: flex;
            flex-direction: column;
            width: 800px;
            height: 600px;
            background: #f5f5f5;
            padding: 20px;
            gap: 20px;
        }

        #title {
            font-size: 24px;
            font-weight: 600;
            color: #2c3e50;
            height: 32px;
        }

        #svg-container {
            display: flex;
            flex-direction: row;
            gap: 20px;
            flex-grow: 1;
        }

        .svg-card {
            display: flex;
            flex-direction: column;
            background: white;
            border-radius: 8px;
            padding: 16px;
            gap: 12px;
            flex-grow: 1;
        }

        .card-title {
            font-size: 14px;
            font-weight: 600;
            color: #7f8c8d;
            height: 20px;
        }

        /* SVG elements need explicit display */
        .svg-element {
            display: block;
        }

        /* SVG element styling via CSS */
        .blue-circle {
            fill: var(--primary);
            stroke: #2980b9;
            stroke-width: 3px;
        }

        .blue-circle:hover {
            fill: #5dade2;
        }

        .red-rect {
            fill: var(--secondary);
            stroke: #c0392b;
            stroke-width: 2px;
        }

        .green-path {
            stroke: var(--success);
            stroke-width: 3px;
            fill: none;
            stroke-linecap: round;
            stroke-linejoin: round;
        }

        .dashed-line {
            stroke: var(--warning);
            stroke-width: 2px;
            stroke-dasharray: 8 4;
        }
    )";

    screen.loadCSS(css);

    // XML with SVG elements - RFC 7991/7996 compliant
    screen.loadXML(R"XML(
        <div id="root">
            <label id="title">SVG in XML Demo (RFC 7991/7996)</label>

            <div id="svg-container">
                <!-- Basic Shapes Card -->
                <div class="svg-card">
                    <label class="card-title">Basic Shapes</label>
                    <svg id="shapes-svg" width="200" height="200" viewBox="0 0 200 200">
                        <circle class="blue-circle" cx="50" cy="50" r="30"/>
                        <rect class="red-rect" x="100" y="20" width="60" height="60"/>
                        <ellipse cx="50" cy="140" rx="40" ry="25" fill="#9b59b6" stroke="#8e44ad" stroke-width="2"/>
                        <line x1="110" y1="100" x2="180" y2="180" stroke="#34495e" stroke-width="3"/>
                    </svg>
                </div>

                <!-- Paths Card -->
                <div class="svg-card">
                    <label class="card-title">SVG Paths</label>
                    <svg id="paths-svg" width="200" height="200" viewBox="0 0 200 200">
                        <!-- Bezier curve -->
                        <path class="green-path" d="M 20 100 Q 100 20 180 100"/>

                        <!-- Star shape -->
                        <path fill="#f1c40f" stroke="#f39c12" stroke-width="2"
                              d="M 100 140 L 110 165 L 138 165 L 115 180 L 125 205 L 100 190 L 75 205 L 85 180 L 62 165 L 90 165 Z"/>

                        <!-- Dashed line -->
                        <line class="dashed-line" x1="20" y1="40" x2="180" y2="40"/>
                    </svg>
                </div>

                <!-- Polygon & Polyline Card -->
                <div class="svg-card">
                    <label class="card-title">Polygon & Polyline</label>
                    <svg id="poly-svg" width="200" height="200" viewBox="0 0 200 200">
                        <!-- Triangle -->
                        <polygon points="100,20 150,100 50,100"
                                 fill="#1abc9c" stroke="#16a085" stroke-width="2"/>

                        <!-- Pentagon -->
                        <polygon points="100,120 130,140 120,175 80,175 70,140"
                                 fill="#e74c3c" stroke="#c0392b" stroke-width="2"/>

                        <!-- Zigzag polyline -->
                        <polyline points="20,180 40,160 60,180 80,160 100,180 120,160 140,180 160,160 180,180"
                                  fill="none" stroke="#3498db" stroke-width="2" stroke-linecap="round"/>
                    </svg>
                </div>
            </div>

            <!-- Advanced Features Row -->
            <div id="svg-container">
                <!-- Text on Path Card -->
                <div class="svg-card">
                    <label class="card-title">Text & Groups</label>
                    <svg id="text-svg" width="200" height="150" viewBox="0 0 200 150">
                        <defs>
                            <path id="textCurve" d="M 10 80 Q 100 20 190 80"/>
                        </defs>

                        <!-- Text element -->
                        <text x="100" y="120" text-anchor="middle" fill="#2c3e50" font-size="16" font-weight="bold">
                            SVG Text
                        </text>

                        <!-- Group with transform -->
                        <g transform="translate(50, 30)">
                            <circle cx="20" cy="20" r="15" fill="#3498db"/>
                            <circle cx="50" cy="20" r="15" fill="#e74c3c"/>
                            <circle cx="80" cy="20" r="15" fill="#2ecc71"/>
                        </g>
                    </svg>
                </div>

                <!-- Gradients Card -->
                <div class="svg-card">
                    <label class="card-title">Gradients</label>
                    <svg id="gradient-svg" width="200" height="150" viewBox="0 0 200 150">
                        <defs>
                            <linearGradient id="grad1" x1="0%" y1="0%" x2="100%" y2="0%">
                                <stop offset="0%" stop-color="#3498db"/>
                                <stop offset="100%" stop-color="#9b59b6"/>
                            </linearGradient>
                            <radialGradient id="grad2" cx="50%" cy="50%" r="50%">
                                <stop offset="0%" stop-color="#f1c40f"/>
                                <stop offset="100%" stop-color="#e74c3c"/>
                            </radialGradient>
                        </defs>

                        <rect x="10" y="20" width="80" height="50" fill="url(#grad1)"/>
                        <circle cx="150" cy="45" r="35" fill="url(#grad2)"/>

                        <rect x="10" y="90" width="180" height="40" rx="8" ry="8"
                              fill="url(#grad1)" opacity="0.8"/>
                    </svg>
                </div>

                <!-- Markers Card -->
                <div class="svg-card">
                    <label class="card-title">Markers & Clipping</label>
                    <svg id="marker-svg" width="200" height="150" viewBox="0 0 200 150">
                        <defs>
                            <!-- Arrow marker -->
                            <marker id="arrow" markerWidth="10" markerHeight="10" refX="9" refY="3" orient="auto">
                                <path d="M0,0 L0,6 L9,3 z" fill="#e74c3c"/>
                            </marker>

                            <!-- Dot marker -->
                            <marker id="dot" markerWidth="8" markerHeight="8" refX="4" refY="4">
                                <circle cx="4" cy="4" r="3" fill="#3498db"/>
                            </marker>

                            <!-- Clip path -->
                            <clipPath id="circleClip">
                                <circle cx="150" cy="100" r="40"/>
                            </clipPath>
                        </defs>

                        <!-- Line with arrow -->
                        <line x1="20" y1="30" x2="100" y2="30" stroke="#e74c3c" stroke-width="2" marker-end="url(#arrow)"/>

                        <!-- Path with markers -->
                        <path d="M 20 70 L 60 50 L 100 70 L 140 50"
                              fill="none" stroke="#3498db" stroke-width="2"
                              marker-start="url(#dot)" marker-mid="url(#dot)" marker-end="url(#arrow)"/>

                        <!-- Clipped rectangle -->
                        <rect x="100" y="50" width="100" height="100" fill="#2ecc71" clip-path="url(#circleClip)"/>
                    </svg>
                </div>
            </div>
        </div>
    )XML");

    while (screen.pollEvents()) {
        screen.draw();
    }

    return 0;
}
