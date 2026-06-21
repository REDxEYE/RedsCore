//
// Created by red_eye on 5/12/26.
//

#include "redscore/platform/vertex_buffer.hpp"

#include "redscore/gltf/tiny_gltf.h"
#include "redscore/platform/logger.h"

float half_to_float(const u16 value) {
    const u32 sign = (value & 0x8000u) << 16;
    const u32 exp = (value & 0x7C00u) >> 10;
    const u32 mantissa = value & 0x03FFu;

    u32 out;

    if (exp == 0) {
        if (mantissa == 0) {
            out = sign;
        } else {
            u32 m = mantissa;
            u32 e = 127 - 15 + 1;

            while ((m & 0x0400u) == 0) {
                m <<= 1;
                --e;
            }

            m &= 0x03FFu;
            out = sign | (e << 23) | (m << 13);
        }
    } else if (exp == 0x1F) {
        out = sign | 0x7F800000u | (mantissa << 13);
    } else {
        out = sign | ((exp + 127 - 15) << 23) | (mantissa << 13);
    }

    return std::bit_cast<float>(out);
}

void copy_attribute(const IO::ConstByteBufferView src_vertex, const IO::MutableByteBufferView dst_vertex,
                    const Attribute &src_attribute, const Attribute &dst_attribute) {
    if (src_attribute.type != AttrType::float16) {
        std::memcpy(
            dst_vertex.data() + dst_attribute.offset,
            src_vertex.data() + src_attribute.offset,
            src_attribute.size()
        );
        return;
    }

    const auto src = src_vertex
            .subview(src_attribute.offset, src_attribute.size())
            .readonly_view_as<u16>(0, src_attribute.component_count, true);

    auto dst = dst_vertex
            .subview(dst_attribute.offset, dst_attribute.size())
            .writable_view_as<float>(0, dst_attribute.component_count, true);

    for (u32 i = 0; i < src_attribute.component_count; ++i) {
        dst[i] = half_to_float(src[i]);
    }
}


BufferRebuildResult convert_buffer(const BufferRebuildInput &input, const convert_attribute_fn &convert_attributes) {
    const auto &input_layout = input.layout;
    if (!input_layout.valid()) {
        throw std::runtime_error("Invalid input buffer layout");
    }

    if (input_layout.stride == 0 || input.buffer.size() % input_layout.stride != 0) {
        throw std::runtime_error("Invalid buffer size or stride");
    }

    BufferRebuildResult result{};
    auto &[attributes, stride] = result.layout;

    attributes.reserve(input_layout.attributes.size());
    for (const auto &attribute: input_layout.attributes) {
        if (attribute.usage == AttrUsage::PAD) {
            continue;
        }

        if (stride % 4 != 0) {
            const u32 pad_size = 4 - stride % 4;
            if (pad_size == 2) {
                attributes.emplace_back(AttrUsage::PAD, AttrType::uint16, stride, 1);
            } else {
                throw std::runtime_error("Invalid stride");
            }
            stride += pad_size;
        }

        Attribute new_attribute = attribute;
        if (attribute.type == AttrType::float16) {
            new_attribute.type = AttrType::float32;
        }
        new_attribute.offset = stride;
        stride += new_attribute.size();
        attributes.push_back(new_attribute);
    }

    const u64 vertex_count = input.buffer.size() / input_layout.stride;
    result.buffer.resize(vertex_count * stride);


    for (u64 vertex_index = 0; vertex_index < vertex_count; ++vertex_index) {
        const auto src_vertex = input.buffer.subview(
            vertex_index * input_layout.stride,
            input_layout.stride
        );

        const auto dst_vertex = result.buffer.writable_view(
            vertex_index * stride,
            stride
        );

        for (auto src_attribute: input_layout.attributes) {
            for (const auto &attribute: attributes) {
                if (attribute.usage == src_attribute.usage) {
                    copy_attribute(src_vertex, dst_vertex, src_attribute, attribute);
                    break;
                }
            }
        }
    }
    convert_attributes(result.buffer.writable_view(), vertex_count, result.layout);

    return result;
}

u32 Attribute::size() const {
    u32 size = 0;
    switch (type) {
        case AttrType::float32:
        case AttrType::uint32:
        case AttrType::int32:
            size = sizeof(u32);
            break;
        case AttrType::float16:
        case AttrType::uint16:
        case AttrType::int16:
            size = sizeof(u16);
            break;
        case AttrType::uint8:
        case AttrType::int8:
            size = sizeof(u8);
            break;
        case AttrType::invalid:
            throw std::runtime_error("Invalid attribute type");
    }
    return size * component_count;
}

std::string Attribute::gltf_name() const {
    switch (usage) {
        case AttrUsage::POSITION:
            return "POSITION";
        case AttrUsage::NORMAL:
            return "NORMAL";
        case AttrUsage::UV0:
            return "TEXCOORD_0";
        case AttrUsage::UV1:
            return "TEXCOORD_1";
        case AttrUsage::UV2:
            return "TEXCOORD_2";
        case AttrUsage::UV3:
            return "TEXCOORD_3";
        case AttrUsage::UV4:
            return "TEXCOORD_4";
        case AttrUsage::COLOR:
            return "_COLOR";
        case AttrUsage::JOINTS:
            return "JOINTS_0";
        case AttrUsage::WEIGHTS:
            return "WEIGHTS_0";
        case AttrUsage::UNK0:
        case AttrUsage::PAD:
            throw std::runtime_error("Invalid attribute type");
    }
    throw std::runtime_error("Invalid attribute type");
}

u32 VertexLayout::attribute_offset(const AttrUsage usage) const {
    for (const auto &attribute: attributes) {
        if (attribute.usage == usage) {
            return attribute.offset;
        }
    }
    return -1;
}

AttrType VertexLayout::attribute_type(AttrUsage usage) const {
    for (const auto &attribute: attributes) {
        if (attribute.usage == usage) {
            return attribute.type;
        }
    }
    return AttrType::invalid;
}

bool VertexLayout::has_attribute(const AttrUsage usage) const {
    return std::ranges::any_of(attributes, [&](const auto &attribute) {
        return attribute.usage == usage;
    });
}

bool VertexLayout::valid() const {
    u32 calculated_stride = 0;
    for (const auto &attribute: attributes) {
        if (attribute.offset != calculated_stride) {
            GLog_Error("Offset of attribute does not match calculated {}!={}", attribute.offset, calculated_stride);
            return false;
        }
        calculated_stride += attribute.size();
    }
    if (stride != calculated_stride) {
        GLog_Error("Stride does not match calculated {}!={}", stride, calculated_stride);
        return false;
    }

    return true;
}

bool VertexLayout::gltf_valid() const {
    u32 calculated_stride = 0;
    for (const auto &attribute: attributes) {
        if (attribute.offset != calculated_stride) {
            GLog_Error("Offset of attribute does not match calculated {}!={}", attribute.offset, calculated_stride);
            return false;
        }
        calculated_stride += attribute.size();
    }
    if (stride != calculated_stride) {
        GLog_Error("Stride does not match calculated {}!={}", stride, calculated_stride);
        return false;
    }

    if (calculated_stride % 4 != 0) {
        GLog_Error("Stride is not a multiple of {} % 4 != 0", calculated_stride);
        return false;
    }

    return true;
}

i32 VertexLayout::gltf_component_type(const AttrUsage usage) const {
    if (const auto attribute = std::ranges::find(attributes, usage, &Attribute::usage);
        attribute != attributes.end()) {
        switch (attribute->type) {
            case AttrType::float32:
                return TINYGLTF_COMPONENT_TYPE_FLOAT;
            case AttrType::float16:
                throw std::runtime_error("float16 not supported");
            case AttrType::uint32:
                return TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
            case AttrType::uint16:
                return TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
            case AttrType::uint8:
                return TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
            case AttrType::int32:
                return TINYGLTF_COMPONENT_TYPE_INT;
            case AttrType::int16:
                return TINYGLTF_COMPONENT_TYPE_SHORT;
            case AttrType::int8:
                return TINYGLTF_COMPONENT_TYPE_BYTE;
            case AttrType::invalid:
                throw std::runtime_error("invalid attribute type");
        }
    }
    throw std::runtime_error("unknown attribute type");
}

i32 VertexLayout::gltf_type(const AttrUsage usage) const {
    if (const auto attribute = std::ranges::find(attributes, usage, &Attribute::usage);
        attribute != attributes.end()) {
        switch (attribute->component_count) {
            case 1:
                return TINYGLTF_TYPE_SCALAR;
            case 2:
                return TINYGLTF_TYPE_VEC2;
            case 3:
                return TINYGLTF_TYPE_VEC3;
            case 4:
                return TINYGLTF_TYPE_VEC4;
            default:
                throw std::runtime_error("unsupported component count");
        }
    }
    throw std::runtime_error("unknown attribute type");
}

const Attribute &VertexLayout::get_attribute(const AttrUsage usage) const {
    for (const auto &attribute: attributes) {
        if (attribute.usage == usage) {
            return attribute;
        }
    }
    throw std::runtime_error("Invalid attribute type");
}
