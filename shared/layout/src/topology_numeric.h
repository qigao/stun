#pragma once

#include <array>
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

// Dot2 with TwoProductFMA and TwoSum (Ogita/Rump/Oishi, Algorithms 3.1,
// 3.5, 5.3): https://www.tuhh.de/ti3/paper/rump/OgRuOi05.pdf
// O(1) time/space. Retain product and addition residuals across all four
// terms; separately rounded determinants can lose a small linear coefficient.
// Requires round-to-nearest arithmetic without reassociation. This does not
// compensate coordinate subtraction, underflow, or subsequent root arithmetic.
inline long double compensated_dot4(const std::array<long double,4>& a,
                                    const std::array<long double,4>& b) {
    long double sum=a[0]*b[0];
    long double correction=std::fma(a[0],b[0],-sum);
    for(std::size_t i=1;i<a.size();++i) {
        const long double product=a[i]*b[i];
        const long double product_residual=std::fma(a[i],b[i],-product);
        const long double next=sum+product;
        const long double virtual_product=next-sum;
        const long double sum_residual=(sum-(next-virtual_product))+(product-virtual_product);
        correction+=sum_residual+product_residual;
        sum=next;
    }
    return sum+correction;
}

} // namespace stun::graphlayout::detail
