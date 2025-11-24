#pragma once

// SVG assets for VCV-style modules

// SVG for input port (blue/cyan jack)
constexpr const char *kInputPortSvg = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 48">
  <defs>
    <radialGradient id="portGrad" cx="0.3" cy="0.3">
      <stop offset="0%" stop-color="#4A9FD8"/>
      <stop offset="100%" stop-color="#1E5A7D"/>
    </radialGradient>
    <radialGradient id="holeGrad" cx="0.5" cy="0.5">
      <stop offset="0%" stop-color="#0A0C10"/>
      <stop offset="100%" stop-color="#1A1E28"/>
    </radialGradient>
  </defs>
  <circle cx="24" cy="24" r="22" fill="#14161A"/>
  <circle cx="24" cy="24" r="18" fill="url(#portGrad)"/>
  <circle cx="24" cy="24" r="8" fill="url(#holeGrad)"/>
  <circle cx="20" cy="20" r="6" fill="#FFFFFF" opacity="0.3"/>
</svg>
)SVG";

// SVG for output port (orange/amber jack)
constexpr const char *kOutputPortSvg = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 48 48">
  <defs>
    <radialGradient id="portGrad" cx="0.3" cy="0.3">
      <stop offset="0%" stop-color="#E8A040"/>
      <stop offset="100%" stop-color="#8D5A20"/>
    </radialGradient>
    <radialGradient id="holeGrad" cx="0.5" cy="0.5">
      <stop offset="0%" stop-color="#0A0C10"/>
      <stop offset="100%" stop-color="#1A1E28"/>
    </radialGradient>
  </defs>
  <circle cx="24" cy="24" r="22" fill="#14161A"/>
  <circle cx="24" cy="24" r="18" fill="url(#portGrad)"/>
  <circle cx="24" cy="24" r="8" fill="url(#holeGrad)"/>
  <circle cx="20" cy="20" r="6" fill="#FFFFFF" opacity="0.3"/>
</svg>
)SVG";

// SVG for large knob (Davies 1900h style)
constexpr const char *kLargeKnobSvg = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 80 80">
  <defs>
    <radialGradient id="knobBody" cx="0.35" cy="0.35">
      <stop offset="0%" stop-color="#6A6A70"/>
      <stop offset="50%" stop-color="#4A4A50"/>
      <stop offset="100%" stop-color="#2A2A30"/>
    </radialGradient>
    <radialGradient id="knobRim" cx="0.3" cy="0.3">
      <stop offset="0%" stop-color="#5A5A60"/>
      <stop offset="100%" stop-color="#3A3A40"/>
    </radialGradient>
    <linearGradient id="indicatorGrad" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#E8E8EA"/>
      <stop offset="100%" stop-color="#A8A8AA"/>
    </linearGradient>
  </defs>
  <ellipse cx="40" cy="42" rx="36" ry="34" fill="#000000" opacity="0.3"/>
  <circle cx="40" cy="40" r="38" fill="url(#knobRim)"/>
  <circle cx="40" cy="40" r="32" fill="url(#knobBody)"/>
  <circle cx="40" cy="12" r="3" fill="#1A1A20" opacity="0.6"/>
  <circle cx="40" cy="68" r="3" fill="#1A1A20" opacity="0.6"/>
  <circle cx="12" cy="40" r="3" fill="#1A1A20" opacity="0.6"/>
  <circle cx="68" cy="40" r="3" fill="#1A1A20" opacity="0.6"/>
  <circle cx="20" cy="20" r="3" fill="#1A1A20" opacity="0.6"/>
  <circle cx="60" cy="20" r="3" fill="#1A1A20" opacity="0.6"/>
  <circle cx="20" cy="60" r="3" fill="#1A1A20" opacity="0.6"/>
  <circle cx="60" cy="60" r="3" fill="#1A1A20" opacity="0.6"/>
  <circle cx="40" cy="40" r="12" fill="#28282E"/>
  <rect x="38" y="18" width="4" height="18" fill="url(#indicatorGrad)" rx="2"/>
  <circle cx="40" cy="40" r="6" fill="#3A3A40"/>
  <line x1="34" y1="40" x2="46" y2="40" stroke="#1A1A20" stroke-width="1.5"/>
  <circle cx="32" cy="32" r="12" fill="#FFFFFF" opacity="0.15"/>
</svg>
)SVG";

// SVG for small knob (Rogan style)
constexpr const char *kSmallKnobSvg = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 60 60">
  <defs>
    <radialGradient id="smallKnobBody" cx="0.35" cy="0.35">
      <stop offset="0%" stop-color="#5A5A60"/>
      <stop offset="70%" stop-color="#3A3A40"/>
      <stop offset="100%" stop-color="#2A2A30"/>
    </radialGradient>
    <linearGradient id="smallIndicator" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#DCDCE0"/>
      <stop offset="100%" stop-color="#9C9CA0"/>
    </linearGradient>
  </defs>
  <ellipse cx="30" cy="32" rx="26" ry="24" fill="#000000" opacity="0.25"/>
  <circle cx="30" cy="30" r="28" fill="url(#smallKnobBody)"/>
  <circle cx="30" cy="30" r="24" fill="none" stroke="#2A2A30" stroke-width="1" opacity="0.5"/>
  <circle cx="30" cy="30" r="20" fill="none" stroke="#4A4A50" stroke-width="0.5" opacity="0.3"/>
  <rect x="28" y="10" width="4" height="14" fill="url(#smallIndicator)" rx="2"/>
  <circle cx="30" cy="30" r="8" fill="#28282E"/>
  <circle cx="30" cy="30" r="5" fill="#3A3A40"/>
  <circle cx="24" cy="24" r="10" fill="#FFFFFF" opacity="0.12"/>
</svg>
)SVG";

// SVG for oscilloscope bezel
constexpr const char *kOscilloscopeBezelSvg = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100">
  <rect x="0" y="0" width="100" height="100" fill="#282830"/>
</svg>
)SVG";

// SVG for speedometer gauge face with ticks and numbers
constexpr const char *kSpeedometerGaugeSvg = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 200">
  <defs>
    <radialGradient id="rimGrad" cx="0.3" cy="0.3">
      <stop offset="0%" stop-color="#B4B4B9"/>
      <stop offset="100%" stop-color="#646469"/>
    </radialGradient>
    <radialGradient id="faceGrad" cx="0.5" cy="0.5">
      <stop offset="0%" stop-color="#19191E"/>
      <stop offset="100%" stop-color="#0A0A0F"/>
    </radialGradient>
  </defs>
  
  <!-- Chrome rim -->
  <circle cx="100" cy="100" r="98" fill="url(#rimGrad)"/>
  <circle cx="100" cy="100" r="93" stroke="#8C8C91" stroke-width="2" fill="none"/>
  
  <!-- Dark face -->
  <circle cx="100" cy="100" r="90" fill="url(#faceGrad)"/>
</svg>
)SVG";

// Complete speedometer gauge with tick marks and numbers (0-240 km/h)
constexpr const char *kSpeedGaugeCompleteSvg = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 200">
  <defs>
    <radialGradient id="speedRimGrad" cx="0.3" cy="0.3">
      <stop offset="0%" stop-color="#B4B4B9"/>
      <stop offset="100%" stop-color="#646469"/>
    </radialGradient>
    <radialGradient id="speedFaceGrad" cx="0.5" cy="0.5">
      <stop offset="0%" stop-color="#19191E"/>
      <stop offset="100%" stop-color="#0A0A0F"/>
    </radialGradient>
    <linearGradient id="cyanToOrange" x1="0%" y1="0%" x2="100%" y2="0%">
      <stop offset="0%" stop-color="#00FFFF"/>
      <stop offset="70%" stop-color="#00FFFF"/>
      <stop offset="85%" stop-color="#FFAA00"/>
      <stop offset="100%" stop-color="#FF6400"/>
    </linearGradient>
  </defs>
  
  <!-- Chrome rim -->
  <circle cx="100" cy="100" r="98" fill="url(#speedRimGrad)"/>
  <circle cx="100" cy="100" r="93" stroke="#8C8C91" stroke-width="2" fill="none"/>
  
  <!-- Dark face -->
  <circle cx="100" cy="100" r="90" fill="url(#speedFaceGrad)"/>
  
  <!-- Smooth colored arc (cyan to orange gradient) -->
  <path d="M 23.5 146.5 A 88 88 0 1 1 176.5 146.5" 
        fill="none" stroke="url(#cyanToOrange)" stroke-width="3" stroke-linecap="round"/>
  
  <!-- All tick marks (240 small ticks) -->
  <g opacity="0.6">
    <!-- Generate small ticks programmatically would be ideal, but for SVG we'll add key ones -->
    <!-- Cyan zone ticks (0-168 = 70% of 240) -->
    <g stroke="#00FFFF" stroke-width="1" stroke-linecap="round" opacity="0.7">
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(0 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(5 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(10 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(15 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(20 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(25 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(30 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(35 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(40 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(45 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(50 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(55 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(60 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(65 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(70 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(75 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(80 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(85 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(90 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(95 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(100 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(105 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(110 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(115 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(120 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(125 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(130 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(135 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(140 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(145 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(150 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(155 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(160 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(165 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(170 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(175 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(180 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(185 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(190 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(195 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(200 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(205 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(210 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(215 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(220 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(225 100 100)"/>
    </g>
    <!-- Orange/Red zone ticks (168-240) -->
    <g stroke="#FF6400" stroke-width="1" stroke-linecap="round" opacity="0.7">
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(230 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(235 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(240 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(245 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(250 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(255 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(260 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(265 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(270 100 100)"/>
    </g>
  </g>
  
  <!-- Major tick marks (every 20 km/h) - Thicker and more visible -->
  <g stroke-linecap="round">
    <line x1="23.5" y1="146.5" x2="33" y2="153" stroke="#00FFFF" stroke-width="2.5" opacity="0.9"/>
    <line x1="14" y1="120" x2="24" y2="124" stroke="#00FFFF" stroke-width="2.5" opacity="0.9"/>
    <line x1="10" y1="90" x2="20" y2="92" stroke="#00FFFF" stroke-width="2.5" opacity="0.9"/>
    <line x1="14" y1="60" x2="24" y2="64" stroke="#00FFFF" stroke-width="2.5" opacity="0.9"/>
    <line x1="28" y1="34" x2="36" y2="42" stroke="#00FFFF" stroke-width="2.5" opacity="0.9"/>
    <line x1="50" y1="16" x2="56" y2="26" stroke="#00FFFF" stroke-width="2.5" opacity="0.9"/>
    <line x1="78" y1="7" x2="82" y2="17" stroke="#00FFFF" stroke-width="2.5" opacity="0.9"/>
    <line x1="108" y1="6" x2="110" y2="16" stroke="#00FFFF" stroke-width="2.5" opacity="0.9"/>
    <line x1="138" y1="12" x2="138" y2="22" stroke="#00FFFF" stroke-width="2.5" opacity="0.9"/>
    <line x1="162" y1="26" x2="158" y2="36" stroke="#FF7850" stroke-width="2.5" opacity="0.9"/>
    <line x1="180" y1="48" x2="172" y2="56" stroke="#FF7850" stroke-width="2.5" opacity="0.9"/>
    <line x1="190" y1="76" x2="180" y2="82" stroke="#FF6400" stroke-width="2.5" opacity="0.9"/>
    <line x1="192" y1="108" x2="182" y2="110" stroke="#FF6400" stroke-width="2.5" opacity="0.9"/>
  </g>
  
  <!-- Numbers with glow effect -->
  <g font-family="sans-serif" font-weight="bold" font-size="11" text-anchor="middle">
    <text x="30" y="155" fill="#00FFFF" filter="url(#glow)">0</text>
    <text x="20" y="125" fill="#00FFFF">20</text>
    <text x="16" y="92" fill="#00FFFF">40</text>
    <text x="22" y="62" fill="#00FFFF">60</text>
    <text x="36" y="38" fill="#00FFFF">80</text>
    <text x="58" y="22" fill="#00FFFF">100</text>
    <text x="84" y="14" fill="#00FFFF">120</text>
    <text x="112" y="13" fill="#00FFFF">140</text>
    <text x="140" y="18" fill="#00FFFF">160</text>
    <text x="164" y="32" fill="#FF7850">180</text>
    <text x="182" y="54" fill="#FF7850">200</text>
    <text x="190" y="82" fill="#FF6400">220</text>
    <text x="192" y="112" fill="#FF6400">240</text>
  </g>
  
  <!-- Unit label -->
  <text x="100" y="135" font-family="sans-serif" font-size="9" text-anchor="middle" fill="#969699">km/h</text>
</svg>
)SVG";

// Complete RPM gauge with tick marks and numbers (0-8 x1000)
constexpr const char *kRpmGaugeCompleteSvg = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 200">
  <defs>
    <radialGradient id="rpmRimGrad" cx="0.3" cy="0.3">
      <stop offset="0%" stop-color="#B4B4B9"/>
      <stop offset="100%" stop-color="#646469"/>
    </radialGradient>
    <radialGradient id="rpmFaceGrad" cx="0.5" cy="0.5">
      <stop offset="0%" stop-color="#19191E"/>
      <stop offset="100%" stop-color="#0A0A0F"/>
    </radialGradient>
    <linearGradient id="rpmCyanToOrange" x1="0%" y1="0%" x2="100%" y2="0%">
      <stop offset="0%" stop-color="#00C8C8"/>
      <stop offset="70%" stop-color="#00C8C8"/>
      <stop offset="85%" stop-color="#FFAA00"/>
      <stop offset="100%" stop-color="#FF6400"/>
    </linearGradient>
  </defs>
  
  <!-- Chrome rim -->
  <circle cx="100" cy="100" r="98" fill="url(#rpmRimGrad)"/>
  <circle cx="100" cy="100" r="93" stroke="#8C8C91" stroke-width="2" fill="none"/>
  
  <!-- Dark face -->
  <circle cx="100" cy="100" r="90" fill="url(#rpmFaceGrad)"/>
  
  <!-- Smooth colored arc (cyan to orange gradient) -->
  <path d="M 23.5 146.5 A 88 88 0 1 1 176.5 146.5" 
        fill="none" stroke="url(#rpmCyanToOrange)" stroke-width="2.5" stroke-linecap="round"/>
  
  <!-- Small tick marks (80 ticks for RPM) -->
  <g opacity="0.6">
    <!-- Cyan zone ticks (0-5.6 = 70% of 8) -->
    <g stroke="#00C8C8" stroke-width="1" stroke-linecap="round" opacity="0.7">
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(0 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(7 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(14 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(21 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(28 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(35 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(42 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(49 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(56 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(63 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(70 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(77 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(84 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(91 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(98 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(105 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(112 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(119 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(126 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(133 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(140 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(147 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(154 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(161 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(168 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(175 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(182 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(189 100 100)"/>
    </g>
    <!-- Orange/Red zone ticks (5.6-8) -->
    <g stroke="#FF6400" stroke-width="1" stroke-linecap="round" opacity="0.7">
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(196 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(203 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(210 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(217 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(224 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(231 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(238 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(245 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(252 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(259 100 100)"/>
      <line x1="23.5" y1="146.5" x2="28" y2="150" transform="rotate(266 100 100)"/>
    </g>
  </g>
  
  <!-- Major tick marks (every 1000 RPM) - Thicker and more visible -->
  <g stroke-linecap="round">
    <line x1="23.5" y1="146.5" x2="33" y2="153" stroke="#00C8C8" stroke-width="2" opacity="0.9"/>
    <line x1="12" y1="112" x2="22" y2="116" stroke="#00C8C8" stroke-width="2" opacity="0.9"/>
    <line x1="10" y1="76" x2="20" y2="78" stroke="#00C8C8" stroke-width="2" opacity="0.9"/>
    <line x1="22" y1="44" x2="30" y2="50" stroke="#00C8C8" stroke-width="2" opacity="0.9"/>
    <line x1="48" y1="20" x2="54" y2="30" stroke="#00C8C8" stroke-width="2" opacity="0.9"/>
    <line x1="82" y1="8" x2="86" y2="18" stroke="#00C8C8" stroke-width="2" opacity="0.9"/>
    <line x1="118" y1="8" x2="120" y2="18" stroke="#FF7850" stroke-width="2" opacity="0.9"/>
    <line x1="152" y1="20" x2="150" y2="30" stroke="#FF7850" stroke-width="2" opacity="0.9"/>
    <line x1="176" y1="46" x2="168" y2="52" stroke="#FF6400" stroke-width="2" opacity="0.9"/>
  </g>
  
  <!-- Numbers -->
  <g font-family="sans-serif" font-weight="bold" font-size="16" text-anchor="middle">
    <text x="30" y="155" fill="#00C8C8">0</text>
    <text x="18" y="116" fill="#00C8C8">1</text>
    <text x="16" y="78" fill="#00C8C8">2</text>
    <text x="28" y="46" fill="#00C8C8">3</text>
    <text x="54" y="26" fill="#00C8C8">4</text>
    <text x="88" y="16" fill="#00C8C8">5</text>
    <text x="122" y="16" fill="#FF7850">6</text>
    <text x="154" y="26" fill="#FF7850">7</text>
    <text x="178" y="50" fill="#FF6400">8</text>
  </g>
  
  <!-- Unit label -->
  <text x="100" y="135" font-family="sans-serif" font-size="9" text-anchor="middle" fill="#969699">x1000r/m</text>
</svg>
)SVG";

// SVG for speedometer arc segments (cyan zone)
constexpr const char *kSpeedometerArcCyanSvg = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 200">
  <defs>
    <linearGradient id="cyanGrad" x1="0%" y1="0%" x2="100%" y2="0%">
      <stop offset="0%" stop-color="#00FFFF"/>
      <stop offset="100%" stop-color="#00E6E6"/>
    </linearGradient>
  </defs>
  <path d="M 100 100 L 29.3 150 A 88 88 0 1 1 170.7 150 Z" 
        fill="none" stroke="url(#cyanGrad)" stroke-width="3"/>
</svg>
)SVG";

// SVG for speedometer arc segments (red zone)
constexpr const char *kSpeedometerArcRedSvg = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 200">
  <defs>
    <linearGradient id="redGrad" x1="0%" y1="0%" x2="100%" y2="0%">
      <stop offset="0%" stop-color="#FF6400"/>
      <stop offset="100%" stop-color="#FF0000"/>
    </linearGradient>
  </defs>
  <path d="M 100 100 L 170.7 150 A 88 88 0 0 1 185 100 Z" 
        fill="none" stroke="url(#redGrad)" stroke-width="3"/>
</svg>
)SVG";

// SVG for speedometer needle
constexpr const char *kSpeedometerNeedleSvg = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 200 200">
  <defs>
    <linearGradient id="needleGrad" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#FF5050"/>
      <stop offset="100%" stop-color="#B40000"/>
    </linearGradient>
    <radialGradient id="hubGrad" cx="0.3" cy="0.3">
      <stop offset="0%" stop-color="#50505A"/>
      <stop offset="100%" stop-color="#28282E"/>
    </radialGradient>
  </defs>
  
  <!-- Needle shadow -->
  <path d="M 100 100 L 95 108 L 98 35 L 100 30 L 102 35 L 105 108 Z" 
        fill="#000000" opacity="0.3" transform="translate(2, 2)"/>
  
  <!-- Needle body -->
  <path d="M 100 100 L 95 108 L 98 35 L 100 30 L 102 35 L 105 108 Z" 
        fill="url(#needleGrad)" stroke="#800000" stroke-width="1"/>
  
  <!-- Center hub glow -->
  <circle cx="100" cy="100" r="15" fill="#3C3C46" opacity="0.5"/>
  
  <!-- Center hub -->
  <circle cx="100" cy="100" r="12" fill="url(#hubGrad)"/>
  <circle cx="100" cy="100" r="12" stroke="#64646E" stroke-width="1" fill="none"/>
  
  <!-- Center dot -->
  <circle cx="100" cy="100" r="6" fill="#1E1E23"/>
</svg>
)SVG";
