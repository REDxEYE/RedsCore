#pragma once

#include "writer.hpp"

namespace ISR::cast_detail {
struct TRS {
    glm::vec3 p, s;
    glm::quat r;
};

struct AffineFactors {
    TRS instance, model;
};

extern const glm::mat4 y_up_to_z_up;
glm::mat4 matrix(const TRS &t);
bool matches(const glm::mat4 &actual, const glm::mat4 &expected);
std::optional<TRS> try_decompose(const glm::mat4 &m);
TRS decompose(const glm::mat4 &m);
AffineFactors factor_shear(const glm::mat4 &m);
void position(Writer &w, const TRS &t);
} // namespace ISR::cast_detail
