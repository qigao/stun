#pragma once

#include <cmath>

namespace stun::graphlayout::detail {

// Preserve the second product's residual when orientations nearly cancel.
// This compensates multiplication, not coordinate subtraction or underflow.
inline long double compensated_determinant(long double ax, long double ay,
                                          long double bx, long double by) {
    const long double product=ay*bx;
    const long double residual=std::fma(-ay,bx,product);
    return std::fma(ax,by,-product)+residual;
}

} // namespace stun::graphlayout::detail
