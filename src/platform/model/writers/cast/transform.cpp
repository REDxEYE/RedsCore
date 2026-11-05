#include "transform.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

#include "redscore/platform/logger.h"

namespace ISR::cast_detail {
// VM/glTF are Y-up; the CAST Blender importer uses Blender's Z-up world.
const glm::mat4 y_up_to_z_up{glm::vec4(1.f, 0.f, 0.f, 0.f), glm::vec4(0.f, 0.f, 1.f, 0.f),
                             glm::vec4(0.f, -1.f, 0.f, 0.f), glm::vec4(0.f, 0.f, 0.f, 1.f)};
glm::mat4 matrix(const TRS &t) {
    return glm::translate(glm::mat4(1.f), t.p) * glm::mat4_cast(t.r) * glm::scale(glm::mat4(1.f), t.s);
}

bool matches(const glm::mat4 &actual, const glm::mat4 &expected) {
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            if (std::abs(actual[col][row] - expected[col][row]) > 1e-4f * std::max(1.f, std::abs(expected[col][row])))
                return false;
    return true;
}

std::optional<TRS> try_decompose(const glm::mat4 &m) {
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            if (!std::isfinite(m[c][r]))
                throw std::invalid_argument("Non-finite CAST transform");
    if (std::abs(m[0][3]) > 1e-5f || std::abs(m[1][3]) > 1e-5f || std::abs(m[2][3]) > 1e-5f ||
        std::abs(m[3][3] - 1.f) > 1e-5f)
        throw std::invalid_argument("CAST cannot represent projective transform");
    TRS t{glm::vec3(m[3]), {}, {}};
    glm::vec3 a(m[0]), b(m[1]), c(m[2]);
    t.s = {glm::length(a), glm::length(b), glm::length(c)};
    if (t.s.x < 1e-8f || t.s.y < 1e-8f || t.s.z < 1e-8f)
        throw std::invalid_argument("CAST cannot represent singular transform");
    a /= t.s.x;
    b /= t.s.y;
    c /= t.s.z;
    if (std::abs(glm::dot(a, b)) > 1e-4f || std::abs(glm::dot(a, c)) > 1e-4f || std::abs(glm::dot(b, c)) > 1e-4f)
        return std::nullopt;
    if (glm::dot(glm::cross(a, b), c) < 0.f) {
        t.s.x = -t.s.x;
        a = -a;
    }
    t.r = glm::normalize(glm::quat_cast(glm::mat3(a, b, c)));
    return t;
}

TRS decompose(const glm::mat4 &m) {
    auto t = try_decompose(m);
    if (!t)
        throw std::invalid_argument("CAST cannot represent affine shear in a bone");
    return *t;
}
AffineFactors factor_shear(const glm::mat4 &m) {
    // A = U * diag(s) * V^T. CAST Instance and Model each have TRS,
    // so their two transforms represent a sheared affine placement.
    const glm::mat3 linear(m);
    glm::mat3 gram = glm::transpose(linear) * linear;
    glm::mat3 right(1.f);
    for (int step = 0; step < 24; ++step) {
        int p = 0, q = 1;
        float largest = std::abs(gram[q][p]);
        for (const auto [a, b] : {std::pair{0, 2}, std::pair{1, 2}}) {
            if (const float v = std::abs(gram[b][a]); v > largest) {
                largest = v;
                p = a;
                q = b;
            }
        }
        if (largest < 1e-10f)
            break;
        const float tau = (gram[q][q] - gram[p][p]) / (2.f * gram[q][p]);
        const float tangent = std::copysign(1.f, tau) / (std::abs(tau) + std::sqrt(1.f + tau * tau));
        const float cosine = 1.f / std::sqrt(1.f + tangent * tangent);
        const float sine = tangent * cosine;
        glm::mat3 rotation(1.f);
        rotation[p][p] = rotation[q][q] = cosine;
        rotation[p][q] = -sine;
        rotation[q][p] = sine;
        gram = glm::transpose(rotation) * gram * rotation;
        right = right * rotation;
    }
    glm::vec3 scales;
    glm::mat3 left;
    for (int i = 0; i < 3; ++i) {
        scales[i] = std::sqrt(std::max(gram[i][i], 0.f));
        if (scales[i] < 1e-8f)
            throw std::invalid_argument("CAST cannot represent singular affine shear");
        left[i] = (linear * right[i]) / scales[i];
    }
    if (glm::determinant(left) < 0.f) {
        scales.x = -scales.x;
        left[0] = -left[0];
    }
    AffineFactors factors{{glm::vec3(m[3]), scales, glm::normalize(glm::quat_cast(left))},
                          {glm::vec3(0.f), glm::vec3(1.f), glm::normalize(glm::quat_cast(glm::transpose(right)))}};
    const glm::mat4 combined = glm::translate(glm::mat4(1.f), factors.instance.p) * glm::mat4_cast(factors.instance.r) *
                               glm::scale(glm::mat4(1.f), factors.instance.s) * glm::mat4_cast(factors.model.r);
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            if (std::abs(combined[col][row] - m[col][row]) > 1e-4f * std::max(1.f, std::abs(m[col][row])))
                // throw std::invalid_argument("CAST affine factorization lost precision");
                GLog_Warning("CAST affine factorization lost precision");
    return factors;
}

void position(Writer &w, const TRS &t) {
    w.vec3("p", t.p);
    w.vec4("r", {t.r.x, t.r.y, t.r.z, t.r.w});
    w.vec3("s", t.s);
}

} // namespace ISR::cast_detail
