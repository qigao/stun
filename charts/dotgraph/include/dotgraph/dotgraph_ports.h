#pragma once

#include "dotgraph/dotgraph_ast.h"

#include <string>

namespace dotgraph {

// A rendering/measurement stage owns these values. They are coordinates
// *inside the final measured node rectangle*, before graph placement. This
// Chart-specific record binds names to actual pixel measurements; the native
// graph router never parses labels or guesses where named ports should be.
struct MeasuredDotPort {
    std::string node_id;
    std::string name;
    DotGraphCompass outward_side = DG_COMPASS_NONE; // N/E/S/W only.
    double local_x = 0.0;
    double local_y = 0.0;
};

} // namespace dotgraph
