#pragma once

#include <cstdint>

namespace stun::graphlayout {

using NodeId = std::uint64_t;
using EdgeId = std::uint64_t;
using ClusterId = std::uint64_t;

struct Point {
    double x{};
    double y{};
};

struct Size {
    double width{};
    double height{};
};

struct Rect {
    double x{};
    double y{};
    double width{};
    double height{};
};

}
