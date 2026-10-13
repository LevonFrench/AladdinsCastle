// SPDX-License-Identifier: MIT
#pragma once
#include <array>
#include <cmath>
namespace acvr {
// Call only after unit-quaternion validation. All live pose consumers use the
// same normalization so tolerance in provider samples cannot shear gun/eye space.
inline std::array<double,4> normalized_quaternion(const float *input) noexcept {
    std::array<double,4> q{};double squared=0;
    for(unsigned i=0;i<4;++i) {q[i]=input[i];squared+=q[i]*q[i];}
    const double norm=std::sqrt(squared);
    for(auto &n:q) n/=norm;
    return q;
}
}
