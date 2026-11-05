#include "redscore/platform/buffer/buffer.h"
#include "serializer.hpp"

#include <algorithm>
#include <stdexcept>

namespace ISR::cast_detail {
    size_t width(const ElementFormat f) {
        switch (f) {
            case ElementFormat::U8:
            case ElementFormat::I8:
                return 1;
            case ElementFormat::U16:
            case ElementFormat::I16:
            case ElementFormat::F16:
                return 2;
            case ElementFormat::U32:
            case ElementFormat::I32:
            case ElementFormat::F32:
                return 4;
        }
        throw std::invalid_argument("Unknown vertex component format");
    }

    size_t components(const ElementType t) {
        switch (t) {
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
        throw std::invalid_argument("Unknown vertex shape");
    }

    uint32_t unsigned_value(const uint8_t *data, const size_t size) {
        const IO::ConstByteBufferView bytes(data, size);
        switch (size) {
            case 1:
                return bytes.view_as<uint8_t>()[0];
            case 2:
                return bytes.view_as<uint16_t>()[0];
            case 4:
                return bytes.view_as<uint32_t>()[0];
            default:
                throw std::invalid_argument("Invalid CAST component width");
        }
    }

    float float_value(const uint8_t *data) {
        const IO::ConstByteBufferView bytes(data, sizeof(float));
        return bytes.view_as<float>()[0];
    }

    template<class T>
    void validate_indices(const uint8_t *data, size_t count, uint32_t maximum) {
        if (!count)
            return;
        const IO::ConstByteBufferView bytes(data, count * sizeof(T));
        for (const T index: bytes.view_as<T>())
            if (index >= maximum)
                throw std::invalid_argument("CAST vertex/bone index out of range");
    }

    void check_attribute(const VertexAttribute &a, const size_t count) {
        const auto stride = width(a.format) * components(a.type);
        if (!count || a.count != count || a.data.size() / stride != count || a.data.size() % stride)
            throw std::invalid_argument("Invalid CAST vertex attribute size/count");
    }

    void indices(Writer &w, const std::string_view name, const uint8_t *data, const size_t count, const size_t stride,
                 const uint32_t maximum) {
        if (stride != 1 && stride != 2 && stride != 4)
            throw std::invalid_argument("Invalid CAST index width");
        switch (stride) {
            case 1:
                validate_indices<uint8_t>(data, count, maximum);
                break;
            case 2:
                validate_indices<uint16_t>(data, count, maximum);
                break;
            case 4:
                validate_indices<uint32_t>(data, count, maximum);
                break;
            default:
                throw std::invalid_argument("Invalid CAST index width");
        }
        w.property(name, stride == 1 ? "b" : stride == 2 ? "h" : "i", count, [&] { w.bytes(data, count * stride); });
    }

    static float component(const VertexAttribute &a, const size_t index) {
        const uint8_t *p = a.data.data() + index * width(a.format);
        if (a.format == ElementFormat::F32)
            return float_value(p);
        if (a.format == ElementFormat::F16)
            throw std::invalid_argument("CAST half-float attribute requires conversion");
        const uint32_t v = unsigned_value(p, width(a.format));
        switch (a.format) {
            case ElementFormat::U8:
                return a.normalized ? v / 255.f : static_cast<float>(v); // NOLINT(*-narrowing-conversions)
            case ElementFormat::U16:
                return a.normalized ? v / 65535.f : static_cast<float>(v); // NOLINT(*-narrowing-conversions)
            case ElementFormat::U32:
                return a.normalized ? v / 4294967295.f : static_cast<float>(v); // NOLINT(*-narrowing-conversions)
            case ElementFormat::I8: {
                const auto s = static_cast<int8_t>(v);
                return a.normalized ? std::max(-1.f, s / 127.f) : static_cast<float>(s); // NOLINT(*-narrowing-conversions)
            }
            case ElementFormat::I16: {
                const auto s = static_cast<int16_t>(v);
                return a.normalized ? std::max(-1.f, s / 32767.f) : static_cast<float>(s); // NOLINT(*-narrowing-conversions)
            }
            case ElementFormat::I32: {
                const auto s = static_cast<int32_t>(v);
                return a.normalized ? std::max(-1.f, s / 2147483647.f) : static_cast<float>(s); // NOLINT(*-narrowing-conversions)
            }
            case ElementFormat::F16:
            case ElementFormat::F32:
                break; // handled before integer decoding
        }
        throw std::invalid_argument("Unknown CAST vertex component");
    }

    static void vectors(Writer &w, const std::string_view name, const VertexAttribute &a, const int lanes) {
        if (components(a.type) != static_cast<size_t>(lanes))
            throw std::invalid_argument("Unsupported CAST attribute shape: " + std::string(name));
        w.property(name,
                   lanes == 2
                       ? "2v"
                       : lanes == 3
                             ? "3v"
                             : "4v",
                   a.count, [&] {
                       if (a.format == ElementFormat::F32 && !a.normalized) {
                           w.bytes(a.data.data(), a.data.size());
                       }
                       else {
                           for (size_t i = 0; i < a.count * lanes; ++i)
                               w.f32(component(a, i));
                       }
                   });
    }

    static void tangent(Writer &w, const VertexAttribute &a) {
        const size_t lanes = components(a.type);
        if (lanes != 3 && lanes != 4)
            throw std::invalid_argument("CAST tangent requires Vec3 or Vec4");
        w.property("vt", "3v", a.count, [&] {
            for (size_t i = 0; i < a.count; ++i)
                for (size_t c = 0; c < 3; ++c)
                    w.f32(component(a, i * lanes + c));
        });
        if (lanes == 4)
            w.property("vm_tangent_w", "f", a.count, [&] {
                for (size_t i = 0; i < a.count; ++i)
                    w.f32(component(a, i * 4 + 3));
            });
    }

    void Serializer::mesh(Writer &w, const Primitive &p, const std::string_view name, const uint64_t material_hash,
                          const size_t bone_count,
                          std::vector<std::tuple<uint64_t, const Primitive *, size_t, std::string> > &shapes,
                          const std::vector<std::string> &target_names) {
        if (p.attributes.empty())
            throw std::invalid_argument("CAST primitive has no vertices");
        const size_t count = p.attributes.front().count;
        const VertexAttribute *joints = nullptr, *weights = nullptr;
        std::unordered_set<std::string> present;
        for (const auto &a: p.attributes) {
            check_attribute(a, count);
            std::string key = std::to_string(static_cast<int>(a.usage)) + ":" + std::to_string(a.set);
            if (!present.insert(key).second)
                throw std::invalid_argument("Duplicate CAST vertex semantic");
            if (a.set != 0 &&
                (a.usage == ElementUsage::Position || a.usage == ElementUsage::Normal || a.usage ==
                 ElementUsage::Tangent))
                throw std::invalid_argument("CAST supports only set 0 of position/normal/tangent");
            if (a.usage == ElementUsage::Joints)
                joints = &a;
            if (a.usage == ElementUsage::Weights)
                weights = &a;
            if ((a.usage == ElementUsage::Joints || a.usage == ElementUsage::Weights) && a.set != 0)
                throw std::invalid_argument("CAST cannot represent multiple joint/weight sets");
        }
        if (!present.contains(std::to_string(static_cast<int>(ElementUsage::Position)) + ":0"))
            throw std::invalid_argument("CAST mesh has no POSITION_0");
        if (bool(joints) != bool(weights) || (joints && !bone_count))
            throw std::invalid_argument("CAST weighted mesh requires matching joints, weights and skeleton");
        const uint64_t mesh_hash = w.begin(mesh_id);
        w.text("n", name);
        if (material_hash)
            w.link("m", material_hash);
        size_t uv_layers = 0, color_layers = 0;
        for (const auto &a: p.attributes) {
            switch (a.usage) {
                case ElementUsage::Position:
                    vectors(w, "vp", a, 3);
                    break;
                case ElementUsage::Normal:
                    vectors(w, "vn", a, 3);
                    break;
                case ElementUsage::Tangent:
                    tangent(w, a);
                    break;
                case ElementUsage::TexCoord:
                    uv_layers = std::max(uv_layers, size_t(a.set) + 1);
                    vectors(w, "u" + std::to_string(a.set), a, 2);
                    break;
                case ElementUsage::Color:
                    color_layers = std::max(color_layers, size_t(a.set) + 1);
                    if (a.format == ElementFormat::U8 && a.type == ElementType::Vec4) {
                        w.property("c" + std::to_string(a.set), "i", count, [&] {
                            for (size_t i = 0; i < count; ++i)
                                w.u32(unsigned_value(a.data.data() + i * 4, 4));
                        });
                    }
                    else
                        vectors(w, "c" + std::to_string(a.set), a, 4);
                    break;
                case ElementUsage::Joints:
                case ElementUsage::Weights:
                    break;
                case ElementUsage::Custom:
                    throw std::invalid_argument("CAST cannot represent custom vertex attribute: " + a.custom_name);
            }
        }
        if (uv_layers > 255 || color_layers > 255)
            throw std::invalid_argument("Too many CAST UV/color layers");
        for (size_t i = 0; i < uv_layers; ++i)
            if (!present.contains(std::to_string(static_cast<int>(ElementUsage::TexCoord)) + ":" + std::to_string(i)))
                throw std::invalid_argument("Sparse CAST UV layers");
        for (size_t i = 0; i < color_layers; ++i)
            if (!present.contains(std::to_string(static_cast<int>(ElementUsage::Color)) + ":" + std::to_string(i)))
                throw std::invalid_argument("Sparse CAST color layers");
        if (uv_layers)
            w.byte("ul", uv_layers);
        if (color_layers)
            w.byte("cl", color_layers);
        if (joints) {
            const size_t lanes = components(joints->type);
            if (lanes != components(weights->type) || lanes > 255 || joints->normalized ||
                (joints->format != ElementFormat::U8 && joints->format != ElementFormat::U16 &&
                 joints->format != ElementFormat::U32))
                throw std::invalid_argument("Invalid CAST skin attribute formats");
            w.byte("mi", lanes);
            indices(w, "wb", joints->data.data(), count * lanes, width(joints->format),
                    narrow(bone_count, "bone count"));
            w.property("wv", "f", count * lanes, [&] {
                if (weights->format == ElementFormat::F32 && !weights->normalized) {
                    for (size_t i = 0; i < count * lanes; ++i)
                        float_value(weights->data.data() + i * 4);
                    w.bytes(weights->data.data(), weights->data.size());
                }
                else {
                    for (size_t i = 0; i < count * lanes; ++i)
                        w.f32(component(*weights, i));
                }
            });
        }
        const size_t stride = p.index_type == IndexType::U8 ? 1 : p.index_type == IndexType::U16 ? 2 : 4;
        if (!p.index_count && p.indices.empty()) {
            if (count % 3)
                throw std::invalid_argument("CAST unindexed triangle count must be divisible by three");
            const size_t index_width = count <= 256 ? 1 : count <= 65536 ? 2 : 4;
            w.property("f",
                       index_width == 1
                           ? "b"
                           : index_width == 2
                                 ? "h"
                                 : "i",
                       count, [&] {
                           for (size_t i = 0; i < count; ++i) {
                               if (index_width == 1)
                                   w.u8(i);
                               else if (index_width == 2)
                                   w.u16(i);
                               else
                                   w.u32(narrow(i, "face index"));
                           }
                       });
        }
        else {
            if (!p.index_count || p.index_count % 3 || p.indices.size() / stride != p.index_count ||
                p.indices.size() % stride)
                throw std::invalid_argument("CAST requires a valid triangle index buffer");
            indices(w, "f", p.indices.data(), p.index_count, stride, narrow(count, "vertex count"));
        }
        for (size_t i = 0; i < p.morph_targets.size(); ++i)
            shapes.emplace_back(mesh_hash, &p, i,
                                i < target_names.size()
                                    ? target_names[i]
                                    : std::string(name) + "_target_" + std::to_string(i));
        w.end();
    }

    void Serializer::shape(Writer &w, const uint64_t base, const Primitive &p, const size_t target,
                           const std::string_view name) {
        const VertexAttribute *positions = nullptr, *delta = nullptr;
        for (const auto &a: p.attributes)
            if (a.usage == ElementUsage::Position)
                positions = &a;
        for (const auto &a: p.morph_targets[target]) {
            if (a.usage != ElementUsage::Position || delta)
                throw std::invalid_argument("CAST blend shapes cannot represent normal/tangent morph deltas");
            delta = &a;
        }
        if (!positions || !delta || positions->format != ElementFormat::F32 || delta->format != ElementFormat::F32 ||
            delta->type != ElementType::Vec3 || positions->type != ElementType::Vec3 || delta->count != positions->count
            ||
            delta->data.size() != positions->data.size())
            throw std::invalid_argument("Invalid CAST morph position target");
        w.begin(shape_id);
        w.text("n", name);
        w.link("b", base);
        const size_t count = positions->count;
        size_t changed = 0;
        for (size_t i = 0; i < count; ++i) {
            bool nonzero = false;
            for (size_t c = 0; c < 3; ++c)
                nonzero |= float_value(delta->data.data() + (3 * i + c) * 4) != 0.f;
            changed += nonzero;
        }
        const auto index_width = count <= 256 ? 1 : count <= 65536 ? 2 : 4;
        w.property("vi",
                   index_width == 1
                       ? "b"
                       : index_width == 2
                             ? "h"
                             : "i",
                   changed, [&] {
                       for (size_t i = 0; i < count; ++i) {
                           bool nonzero = false;
                           for (size_t c = 0; c < 3; ++c)
                               nonzero |= float_value(delta->data.data() + (3 * i + c) * 4) != 0.f;
                           if (nonzero) {
                               if (index_width == 1)
                                   w.u8(i);
                               else if (index_width == 2)
                                   w.u16(i);
                               else
                                   w.u32(narrow(i, "morph index"));
                           }
                       }
                   });
        w.property("vp", "3v", changed, [&] {
            for (size_t i = 0; i < count; ++i) {
                float d[3], v[3];
                bool nonzero = false;
                for (size_t c = 0; c < 3; ++c) {
                    d[c] = float_value(delta->data.data() + (3 * i + c) * 4);
                    nonzero |= d[c] != 0.f;
                    v[c] = float_value(positions->data.data() + (3 * i + c) * 4);
                }
                if (nonzero)
                    for (size_t c = 0; c < 3; ++c)
                        w.f32(v[c] + d[c]);
            }
        });
        w.end();
    }
} // namespace ISR::cast_detail
