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

// SVG for speedometer gauge face
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
