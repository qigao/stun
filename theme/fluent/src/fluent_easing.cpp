#include <nanogui/fluent_easing.h>
#include <cmath>
#include <algorithm>

NAMESPACE_BEGIN(nanogui)

float FluentEasing::ease(Curve curve, float t) {
    t = std::clamp(t, 0.f, 1.f);
    
    switch (curve) {
        case Curve::Standard:
            // Cubic bezier (0.2, 0.0, 0, 1.0) - balanced motion
            return cubic_bezier(t, 0.2f, 0.0f, 0.0f, 1.0f);
        
        case Curve::Emphasized:
            // Cubic bezier (0.05, 0.7, 0.1, 1.0) - expressive motion
            return cubic_bezier(t, 0.05f, 0.7f, 0.1f, 1.0f);
        
        case Curve::Decelerated:
            // Cubic bezier (0.0, 0.0, 0.2, 1.0) - incoming elements
            return cubic_bezier(t, 0.0f, 0.0f, 0.2f, 1.0f);
        
        case Curve::Accelerated:
            // Cubic bezier (0.3, 0.0, 1.0, 1.0) - outgoing elements
            return cubic_bezier(t, 0.3f, 0.0f, 1.0f, 1.0f);
    }
    
    return t;
}

float FluentEasing::cubic_bezier(float t, float p1, float p2, float p3, float p4) {
    // Cubic bezier for 2D curve where x=t, y=result
    // Control points: (0,0), (p1,p2), (p3,p4), (1,1)
    
    float t2 = t * t;
    float t3 = t2 * t;
    float mt = 1.f - t;
    float mt2 = mt * mt;
    float mt3 = mt2 * mt;
    
    // Cubic bezier formula: B(t) = (1-t)³P₀ + 3(1-t)²tP₁ + 3(1-t)t²P₂ + t³P₃
    return 3.f * mt2 * t * p2 + 3.f * mt * t2 * p4 + t3;
}

NAMESPACE_END(nanogui)
