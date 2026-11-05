#pragma once

#include "redscore/platform/buffer/buffer.h"
#include <cstdint>
#include <format>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <glm/glm.hpp>

namespace ISR {
using DataBuffer = std::vector<uint8_t>;

template <typename T> DataBuffer data_buffer_from_(std::span<const T> data) {
    if (data.empty())
        return {};
    // VM attributes own their bytes; the view avoids a temporary buffer.
    const IO::ConstByteBufferView bytes(reinterpret_cast<const uint8_t *>(data.data()), data.size_bytes());
    return DataBuffer(bytes.begin(), bytes.end());
}
template <typename T> DataBuffer data_buffer_from_(std::span<T> data) {
    return data_buffer_from_(std::span<const T>(data.data(), data.size()));
}
template <typename T> DataBuffer data_buffer_from_(const std::vector<T> &data) {
    return data_buffer_from_(std::span<const T>(data.data(), data.size()));
}

enum class ElementFormat { U8, U16, U32, I8, I16, I32, F16, F32 };

enum class ElementType { Scalar, Vec2, Vec3, Vec4, Mat3, Mat4 };

enum class ElementUsage { Position, Normal, Tangent, Color, TexCoord, Joints, Weights, Custom };

enum class IndexType { U8, U16, U32 };

struct VertexAttribute {
    ElementUsage usage{ElementUsage::Position};
    uint32_t set{0};
    std::string custom_name;
    ElementFormat format{ElementFormat::F32};
    ElementType type{ElementType::Vec3};
    bool normalized{false};
    size_t count{0};
    // Each attribute owns tightly packed data; game-specific packing is decoded upstream.
    DataBuffer data;
};

struct TextureReference {
    std::string name;
    DataBuffer png;
    // Optional lazy loader; captures the provider and any game-specific processing.
    std::function<DataBuffer()> load_png;
    // Opt in only when this loader and everything it captures allow concurrent calls.
    // Otherwise serializers invoke it on their own thread as before.
    bool parallel_safe = false;
};

using TexturePtr = std::shared_ptr<TextureReference>;

TexturePtr png_texture(const std::string &name, DataBuffer data);

TexturePtr lazy_png_texture(const std::string &name, const std::function<DataBuffer()> &data);

enum class AlphaMode { Opaque, Mask, Blend };

struct Material {
    std::string name;
    TexturePtr albedo, normal, metallic_roughness, occlusion, emissive, specular, specularColor;
    glm::vec4 base_color{1.f};
    glm::vec3 emissive_factor{0.f};
    float metallic_factor{1.f}, roughness_factor{1.f}, normal_scale{1.f}, alpha_cutoff{0.5f};
    float specularFactor{1.f};
    glm::vec3 specularColorFactor{1.f};
    bool specularUsed{false};
    AlphaMode alpha_mode{AlphaMode::Opaque};
    bool double_sided{false};
};

struct Primitive {
    std::string name;
    std::shared_ptr<Material> material;
    std::vector<VertexAttribute> attributes;
    // Target attributes are position/normal/tangent deltas, in mesh target order.
    std::vector<std::vector<VertexAttribute>> morph_targets;
    IndexType index_type{IndexType::U32};
    size_t index_count{0};
    DataBuffer indices;

    void set_attribute(const VertexAttribute &attribute);

    void set_indices(const void *data, size_t bytes, IndexType type, size_t count);
};

struct Mesh {
    std::string name;
    std::vector<Primitive> primitives;
    std::vector<std::string> target_names;
};

struct SubModel {
    std::string name;
    std::vector<Mesh> meshes;
};
} // namespace ISR
template <> struct std::formatter<ISR::ElementFormat> : std::formatter<std::string_view> {
    auto format(ISR::ElementFormat value, std::format_context &ctx) const {
        using enum ISR::ElementFormat;

        std::string_view name;
        switch (value) {
#define FMT_CASE(x)                                                                                                    \
    case x:                                                                                                            \
        name = #x;                                                                                                     \
        break
            FMT_CASE(U8);
            FMT_CASE(U16);
            FMT_CASE(U32);
            FMT_CASE(I8);
            FMT_CASE(I16);
            FMT_CASE(I32);
            FMT_CASE(F16);
            FMT_CASE(F32);
#undef FMT_CASE
        default:
            return std::format_to(ctx.out(), "ElementFormat({})", static_cast<uint32_t>(value));
        }

        return std::formatter<std::string_view>::format(name, ctx);
    }
};
template <> struct std::formatter<ISR::ElementType> : std::formatter<std::string_view> {
    auto format(ISR::ElementType value, std::format_context &ctx) const {
        using enum ISR::ElementType;

        std::string_view name;
        switch (value) {
#define FMT_CASE(x)                                                                                                    \
    case x:                                                                                                            \
        name = #x;                                                                                                     \
        break
            FMT_CASE(Scalar);
            FMT_CASE(Vec2);
            FMT_CASE(Vec3);
            FMT_CASE(Vec4);
            FMT_CASE(Mat3);
            FMT_CASE(Mat4);
#undef FMT_CASE
        default:
            return std::format_to(ctx.out(), "ElementType({})", static_cast<uint32_t>(value));
        }

        return std::formatter<std::string_view>::format(name, ctx);
    }
};
