//
// Created by red_eye on 5/12/26.
//

#pragma once
#include <vector>

#include "buffer/buffer.h"
#include "redscore/int_def.h"

enum class AttrType {
    float32,
    float16,
    uint32,
    uint16,
    uint8,
    int32,
    int16,
    int8,
    invalid
};

enum class AttrUsage {
    POSITION,
    NORMAL,
    UV0,
    UV1,
    UV2,
    UV3,
    UV4,
    COLOR,
    JOINTS,
    WEIGHTS,
    UNK0,
    PAD,
};


struct Attribute {
    AttrUsage usage;
    AttrType type;
    u32 offset;
    u32 component_count;
    bool normalized;

    Attribute() = delete;

    Attribute(const AttrUsage usage, const AttrType type, const u32 offset, const u32 component_count,
              const bool normalized = false)
        : usage(usage),
          type(type),
          offset(offset),
          normalized(normalized),
          component_count(component_count) {
    }


    [[nodiscard]] u32 size() const;

    [[nodiscard]] std::string gltf_name() const;
};

struct VertexLayout {
    std::vector<Attribute> attributes;
    u32 stride{0};

    [[nodiscard]] u32 attribute_offset(AttrUsage usage) const;

    [[nodiscard]] AttrType attribute_type(AttrUsage usage) const;

    [[nodiscard]] bool has_attribute(AttrUsage usage) const;

    [[nodiscard]] bool valid() const;

    [[nodiscard]] bool gltf_valid() const;

    [[nodiscard]] i32 gltf_component_type(AttrUsage usage) const;

    [[nodiscard]] i32 gltf_type(AttrUsage usage) const;

    [[nodiscard]] const Attribute &get_attribute(AttrUsage usage) const;
};

struct BufferRebuildInput {
    IO::ConstByteBufferView buffer;
    const VertexLayout &layout;
};

struct BufferRebuildResult {
    IO::Buffer buffer;
    VertexLayout layout;
};


using convert_attribute_fn = std::function<void(IO::MutableByteBufferView, u32, const VertexLayout &layout)>;

static void convert_attribute_do_nothing(IO::MutableByteBufferView vertex_start, u32 count,
                                         const VertexLayout &layout) {
    (void) vertex_start;
    (void) count;
    (void) layout;
}

BufferRebuildResult convert_buffer(const BufferRebuildInput &input,
                                   const convert_attribute_fn &convert_attribute = convert_attribute_do_nothing);
