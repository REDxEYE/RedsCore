#include "redscore/platform/buffer/buffer.h"
#include "redscore/platform/gltf_helper.h"

namespace {
size_t component_size_bytes(const int component_type) {
    switch (component_type) {
    case TINYGLTF_COMPONENT_TYPE_BYTE:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        return 1;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        return 2;
    case TINYGLTF_COMPONENT_TYPE_INT:
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        return 4;
    case TINYGLTF_COMPONENT_TYPE_DOUBLE:
        return 8;
    default:
        throw std::runtime_error("Unsupported glTF component type");
    }
}

size_t component_count(const int gltf_type) {
    switch (gltf_type) {
    case TINYGLTF_TYPE_SCALAR:
        return 1;
    case TINYGLTF_TYPE_VEC2:
        return 2;
    case TINYGLTF_TYPE_VEC3:
        return 3;
    case TINYGLTF_TYPE_VEC4:
    case TINYGLTF_TYPE_MAT2:
        return 4;
    case TINYGLTF_TYPE_MAT3:
        return 9;
    case TINYGLTF_TYPE_MAT4:
        return 16;
    default:
        throw std::runtime_error("Unsupported glTF accessor type");
    }
}

size_t checked_required_bytes(const size_t element_size, const size_t count, const size_t byte_stride) {
    if (count == 0) {
        throw std::runtime_error("Accessor count must be > 0");
    }
    if (byte_stride == 0) {
        return element_size * count;
    }
    return byte_stride * (count - 1) + element_size;
}

void ensure_fits_int32(const size_t value, const char *what) {
    if (value > static_cast<size_t>(std::numeric_limits<int32>::max())) {
        throw std::runtime_error(std::string(what) + " exceeds int32 limit");
    }
}
} // namespace

int32 GltfHelper::create_primitive(const int32 mesh_id, const int mode) {
    if (mesh_id < 0 || static_cast<size_t>(mesh_id) >= m_model.meshes.size()) {
        throw std::runtime_error("Mesh id is out of range");
    }
    auto &mesh = m_model.meshes[static_cast<size_t>(mesh_id)];
    mesh.primitives.emplace_back();
    mesh.primitives.back().mode = mode;
    ensure_fits_int32(mesh.primitives.size() - 1, "Primitive index");
    return static_cast<int32>(mesh.primitives.size() - 1);
}

GltfHelper::Handle<tinygltf::Buffer> GltfHelper::create_buffer(const std::span<const u8> &data,
                                                               const std::string_view name) {
    const auto buffer = make<tinygltf::Buffer>();
    if (!name.empty()) {
        buffer->name = name;
    }
    buffer->data = std::vector(data.begin(), data.end());
    return buffer;
}

GltfHelper::Handle<tinygltf::Accessor>
GltfHelper::create_accessor(const uint8 *data, const size_t byte_length, const int buffer_view_target,
                            const int component_type, const int gltf_type, const size_t count, const bool normalized,
                            const size_t byte_stride, const size_t accessor_byte_offset, const std::string &name) {
    return create_accessor_chain(data, byte_length, buffer_view_target, component_type, gltf_type, count, normalized,
                                 byte_stride, accessor_byte_offset, name)
        .accessor;
}

GltfHelper::AccessorChainIds GltfHelper::create_accessor_chain(const uint8 *data, const size_t byte_length,
                                                               const int buffer_view_target, const int component_type,
                                                               const int gltf_type, const size_t count,
                                                               const bool normalized, const size_t byte_stride,
                                                               const size_t accessor_byte_offset,
                                                               const std::string &name) {
    if (data == nullptr && byte_length > 0) {
        throw std::runtime_error("Data pointer is null");
    }

    const size_t comp_size = component_size_bytes(component_type);
    const size_t elem_size = comp_size * component_count(gltf_type);

    if (byte_stride != 0 && byte_stride < elem_size) {
        throw std::runtime_error("Byte stride is smaller than element size");
    }
    if (accessor_byte_offset > byte_length) {
        throw std::runtime_error("Accessor byte offset is out of range");
    }

    const size_t available = byte_length - accessor_byte_offset;
    const size_t required = checked_required_bytes(elem_size, count, byte_stride);
    if (available < required) {
        throw std::runtime_error("Provided data is too small for accessor layout");
    }

    const auto buffer = make<tinygltf::Buffer>();
    if (byte_length) {
        // tinygltf owns the bytes after the caller's buffer goes out of scope.
        const IO::ConstByteBufferView source(data, byte_length);
        buffer->data.assign(source.begin(), source.end());
    }

    const auto buffer_view = make<tinygltf::BufferView>();
    buffer_view->buffer = buffer.index();
    buffer_view->byteOffset = 0;
    ensure_fits_int32(byte_length, "BufferView byteLength");
    buffer_view->byteLength = static_cast<int32>(byte_length);
    if (byte_stride > 0) {
        ensure_fits_int32(byte_stride, "BufferView byteStride");
        buffer_view->byteStride = static_cast<int32>(byte_stride);
    }
    buffer_view->target = buffer_view_target;

    const auto accessor = make<tinygltf::Accessor>();
    accessor->bufferView = buffer_view.index();
    ensure_fits_int32(accessor_byte_offset, "Accessor byteOffset");
    accessor->byteOffset = static_cast<int32>(accessor_byte_offset);
    accessor->componentType = component_type;
    accessor->type = gltf_type;
    ensure_fits_int32(count, "Accessor count");
    accessor->count = static_cast<int32>(count);
    accessor->normalized = normalized;
    accessor->name = name;

    return AccessorChainIds{.accessor = accessor, .buffer_view = buffer_view, .buffer = buffer};
}
GltfHelper::Handle<tinygltf::Image> GltfHelper::create_image_png_data(std::vector<uint8> &&png_data,
                                                                      const std::string &name) {
    if (png_data.empty()) {
        throw std::runtime_error("PNG data size must be > 0");
    }

    const auto buffer = make<tinygltf::Buffer>();
    buffer->data = png_data;

    const auto buffer_view = make<tinygltf::BufferView>();
    buffer_view->buffer = buffer.index();
    buffer_view->byteOffset = 0;
    ensure_fits_int32(png_data.size(), "Image BufferView byteLength");
    buffer_view->byteLength = static_cast<int32>(png_data.size());
    buffer_view->target = 0;

    const auto image = make<tinygltf::Image>();
    image->name = name;
    image->mimeType = "image/png";
    image->bufferView = buffer_view.index();

    return image;
}
GltfHelper::Handle<tinygltf::Texture> GltfHelper::create_texture_png_data(std::vector<uint8> &&png_data,
                                                                          const std::string &name,
                                                                          const Handle<tinygltf::Sampler> sampler) {
    if (png_data.empty()) {
        throw std::runtime_error("PNG data size must be > 0");
    }
    const auto image = create_image_png_data(std::move(png_data), name);
    const auto texture = make<tinygltf::Texture>();
    texture->source = image.index();
    texture->name = name;
    if (sampler.is_valid()) {
        texture->sampler = sampler.index();
    }
    return texture;
}
GltfHelper::Handle<tinygltf::Accessor>
GltfHelper::set_primitive_attribute(tinygltf::Primitive &prim, const std::string &attribute_name, const uint8 *data,
                                    const size_t byte_length, const int component_type, const int gltf_type,
                                    const size_t count, const bool normalized, const size_t byte_stride,
                                    const size_t accessor_byte_offset, const std::string &accessor_name) {
    const auto accessor = create_accessor(data, byte_length, TINYGLTF_TARGET_ARRAY_BUFFER, component_type, gltf_type,
                                          count, normalized, byte_stride, accessor_byte_offset, accessor_name);
    prim.attributes[attribute_name] = accessor.index();
    return accessor;
}
GltfHelper::Handle<tinygltf::Accessor>
GltfHelper::set_primitive_indices(tinygltf::Primitive &prim, const uint8 *data, const size_t byte_length,
                                  const int component_type, const size_t count, const size_t byte_stride,
                                  const size_t accessor_byte_offset, const std::string &accessor_name) {
    const auto accessor =
        create_accessor(data, byte_length, TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER, component_type, TINYGLTF_TYPE_SCALAR,
                        count, false, byte_stride, accessor_byte_offset, accessor_name);

    prim.indices = accessor.index();
    return accessor;
}
