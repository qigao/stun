#pragma once

#include "graph_ir.h"

#include <string>

namespace stun::graphlayout {

struct GraphValidationResult {
    bool valid{false};
    std::string message;
};

GraphValidationResult validate_graph(const GraphIR& graph);

}
