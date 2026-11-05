#include "redscore/platform/buffer/buffer.h"
#include "serializer.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace ISR::gltf_writer {
namespace {
int component(ElementFormat format) {
    switch (format) {
    case ElementFormat::U8:
        return TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
    case ElementFormat::U16:
        return TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
    case ElementFormat::U32:
        return TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
    case ElementFormat::I8:
        return TINYGLTF_COMPONENT_TYPE_BYTE;
    case ElementFormat::I16:
        return TINYGLTF_COMPONENT_TYPE_SHORT;
    case ElementFormat::F32:
        return TINYGLTF_COMPONENT_TYPE_FLOAT;
    default:
        throw std::invalid_argument("Convert F16/I32 attributes before glTF serialization");
    }
}

size_t component_size(ElementFormat format) {
    switch (format) {
    case ElementFormat::U8:
    case ElementFormat::I8:
        return 1;
    case ElementFormat::U16:
    case ElementFormat::I16:
    case ElementFormat::F16:
        return 2;
    default:
        return 4;
    }
}

int shape(ElementType type) {
    switch (type) {
    case ElementType::Scalar:
        return TINYGLTF_TYPE_SCALAR;
    case ElementType::Vec2:
        return TINYGLTF_TYPE_VEC2;
    case ElementType::Vec3:
        return TINYGLTF_TYPE_VEC3;
    case ElementType::Vec4:
        return TINYGLTF_TYPE_VEC4;
    case ElementType::Mat3:
        return TINYGLTF_TYPE_MAT3;
    case ElementType::Mat4:
        return TINYGLTF_TYPE_MAT4;
    }
    throw std::invalid_argument("Invalid element shape");
}

size_t lanes(ElementType type) {
    switch (type) {
    case ElementType::Scalar:
        return 1;
    case ElementType::Vec2:
        return 2;
    case ElementType::Vec3:
        return 3;
    case ElementType::Vec4:
        return 4;
    case ElementType::Mat3:
        return 9;
    case ElementType::Mat4:
        return 16;
    }
    throw std::invalid_argument("Invalid element shape");
}

std::string semantic(const VertexAttribute &a) {
    switch (a.usage) {
    case ElementUsage::Position:
        return "POSITION";
    case ElementUsage::Normal:
        return "NORMAL";
    case ElementUsage::Tangent:
        return "TANGENT";
    case ElementUsage::Color:
        return "COLOR_" + std::to_string(a.set);
    case ElementUsage::TexCoord:
        return "TEXCOORD_" + std::to_string(a.set);
    case ElementUsage::Joints:
        return "JOINTS_" + std::to_string(a.set);
    case ElementUsage::Weights:
        return "WEIGHTS_" + std::to_string(a.set);
    case ElementUsage::Custom:
        if (a.custom_name.empty() || a.custom_name[0] != '_')
            throw std::invalid_argument("Custom attribute must start with underscore");
        return a.custom_name;
    }
    throw std::invalid_argument("Invalid attribute semantic");
}
template <class T> void check_indices(const IO::ConstByteBufferView bytes, const size_t vertex_count) {
    for (const T index : bytes.view_as<T>())
        if (index >= vertex_count)
            throw std::invalid_argument("Index exceeds vertex count");
}

} // namespace

int Serializer::mesh(const Mesh &mesh) {
    if (auto i = meshes.find(&mesh); i != meshes.end())
        return i->second;
    auto g = out.make<tinygltf::Mesh>();
    g->name = mesh.name;
    meshes[&mesh] = g.index();
    for (const auto &p : mesh.primitives) {
        tinygltf::Primitive gp;
        gp.mode = TINYGLTF_MODE_TRIANGLES;
        gp.material = material(p.material);
        if (p.attributes.empty())
            throw std::invalid_argument("Primitive has no attributes");
        const auto count = p.attributes.front().count;
        for (const auto &a : p.attributes) {
            const auto width = component_size(a.format) * lanes(a.type);
            if (!count || a.count != count || a.data.size() / width != count || a.data.size() % width)
                throw std::invalid_argument("Invalid vertex buffer size/count");
            // Packed matrices need column padding for 8/16-bit components; reject rather than misserialize.
            if ((a.type == ElementType::Mat3 || a.type == ElementType::Mat4) && a.format != ElementFormat::F32)
                throw std::invalid_argument("Matrix attributes require F32");
            auto name = semantic(a);
            if (gp.attributes.contains(name))
                throw std::invalid_argument("Duplicate vertex semantic");
            const auto accessor = out.set_primitive_attribute(gp, name, a.data.data(), a.data.size(),
                                                              component(a.format), shape(a.type), count, a.normalized);
            if (a.usage == ElementUsage::Position) {
                if (a.format != ElementFormat::F32 || a.type != ElementType::Vec3)
                    throw std::invalid_argument("Positions require F32 Vec3");
                std::vector<double> low(3, std::numeric_limits<double>::infinity()),
                    high(3, -std::numeric_limits<double>::infinity());
                const IO::ConstByteBufferView bytes(a.data.data(), a.data.size());
                const auto values = bytes.view_as<float>();
                for (size_t i = 0; i < count; ++i)
                    for (size_t c = 0; c < 3; ++c) {
                        const float v = values[i * 3 + c];
                        if (!std::isfinite(v))
                            throw std::invalid_argument("Non-finite position");
                        low[c] = std::min(low[c], static_cast<double>(v));
                        high[c] = std::max(high[c], static_cast<double>(v));
                    }
                accessor->minValues = low;
                accessor->maxValues = high;
            }
        }
        for (const auto &target : p.morph_targets) {
            if (target.empty())
                throw std::invalid_argument("Morph target has no attributes");
            tinygltf::Primitive target_primitive;
            for (const auto &a : target) {
                if ((a.usage != ElementUsage::Position && a.usage != ElementUsage::Normal &&
                     a.usage != ElementUsage::Tangent) ||
                    a.format != ElementFormat::F32 || a.type != ElementType::Vec3 || a.normalized || a.count != count ||
                    a.data.size() != count * 3 * sizeof(float))
                    throw std::invalid_argument("Invalid morph target attribute");
                const IO::ConstByteBufferView bytes(a.data.data(), a.data.size());
                for (const float value : bytes.view_as<float>())
                    if (!std::isfinite(value))
                        throw std::invalid_argument("Non-finite morph target delta");
                const auto name = semantic(a);
                if (target_primitive.attributes.contains(name))
                    throw std::invalid_argument("Duplicate morph target semantic");
                out.set_primitive_attribute(target_primitive, name, a.data.data(), a.data.size(),
                                            TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_VEC3, count, false);
            }
            gp.targets.push_back(std::move(target_primitive.attributes));
        }
        if (g->primitives.empty()) {
            if (!mesh.target_names.empty() && mesh.target_names.size() != gp.targets.size())
                throw std::invalid_argument("Morph target name count differs from target count");
            g->weights.resize(gp.targets.size(), 0.0);
        } else if (gp.targets.size() != g->weights.size()) {
            throw std::invalid_argument("Mesh primitives have different morph target counts");
        }
        if (p.index_count) {
            size_t width = p.index_type == IndexType::U8 ? 1 : p.index_type == IndexType::U16 ? 2 : 4;
            if (p.indices.size() / width != p.index_count || p.indices.size() % width)
                throw std::invalid_argument("Invalid index buffer size");
            const IO::ConstByteBufferView bytes(p.indices.data(), p.indices.size());
            switch (p.index_type) {
            case IndexType::U8:
                check_indices<uint8_t>(bytes, count);
                break;
            case IndexType::U16:
                check_indices<uint16_t>(bytes, count);
                break;
            case IndexType::U32:
                check_indices<uint32_t>(bytes, count);
                break;
            }
            out.set_primitive_indices(gp, p.indices.data(), p.indices.size(),
                                      width == 1   ? TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE
                                      : width == 2 ? TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT
                                                   : TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT,
                                      p.index_count);
        }
        g->primitives.push_back(std::move(gp));
    }
    if (!mesh.target_names.empty()) {
        tinygltf::Value::Array names;
        names.reserve(mesh.target_names.size());
        for (const auto &name : mesh.target_names)
            names.emplace_back(name);
        tinygltf::Value::Object extras;
        extras["targetNames"] = tinygltf::Value(std::move(names));
        g->extras = tinygltf::Value(std::move(extras));
    }
    return g.index();
}
} // namespace ISR::gltf_writer
