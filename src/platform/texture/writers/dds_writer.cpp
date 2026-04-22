//
// Created by red_eye on 4/19/26.
//

#include "redscore/platform/texture/writers//dds_writer.hpp"

DDS::FormatInfo DDS::choose_format(const u32 bytes_per_channel, const u32 channel_count,
                                   const bool is_float) {
    if (channel_count != 1 && channel_count != 2 && channel_count != 4) {
        throw std::runtime_error("DDS writer supports only 1, 2 or 4 channels");
    }

    if (bytes_per_channel != 1 && bytes_per_channel != 2 && bytes_per_channel != 4) {
        throw std::runtime_error("DDS writer supports only 1, 2 or 4 bytes per channel");
    }

    if (is_float && bytes_per_channel != 2 && bytes_per_channel != 4) {
        throw std::runtime_error("Float DDS supports only 16-bit or 32-bit channels");
    }

    FormatInfo out{};

    if (!is_float) {
        if (bytes_per_channel == 1) {
            if (channel_count == 1) {
                out.pf.flags = DDPF_LUMINANCE;
                out.pf.rgbBitCount = 8;
                out.pf.rBitMask = 0x000000FF;
                return out;
            }
            if (channel_count == 2) {
                out.use_dx10 = true;
                out.dxgi = DXGI_FORMAT_R8G8_UNORM;
                out.pf.flags = DDPF_FOURCC;
                out.pf.fourCC = make_fourcc('D', 'X', '1', '0');
                return out;
            }
            if (channel_count == 4) {
                out.pf.flags = DDPF_RGB | DDPF_ALPHAPIXELS;
                out.pf.rgbBitCount = 32;
                out.pf.rBitMask = 0x000000FF;
                out.pf.gBitMask = 0x0000FF00;
                out.pf.bBitMask = 0x00FF0000;
                out.pf.aBitMask = 0xFF000000;
                return out;
            }
        }

        if (bytes_per_channel == 2) {
            out.use_dx10 = true;
            out.pf.flags = DDPF_FOURCC;
            out.pf.fourCC = make_fourcc('D', 'X', '1', '0');

            if (channel_count == 1) {
                out.dxgi = DXGI_FORMAT_R16_UNORM;
                return out;
            }
            if (channel_count == 2) {
                out.dxgi = DXGI_FORMAT_R16G16_UNORM;
                return out;
            }
            if (channel_count == 4) {
                out.dxgi = DXGI_FORMAT_R16G16B16A16_UNORM;
                return out;
            }
        }

        if (bytes_per_channel == 4) {
            out.use_dx10 = true;
            out.pf.flags = DDPF_FOURCC;
            out.pf.fourCC = make_fourcc('D', 'X', '1', '0');

            if (channel_count == 1) {
                out.dxgi = DXGI_FORMAT_R32_UINT;
                return out;
            }
            if (channel_count == 2) {
                out.dxgi = DXGI_FORMAT_R32G32_UINT;
                return out;
            }
            if (channel_count == 4) {
                out.dxgi = DXGI_FORMAT_R32G32B32A32_UINT;
                return out;
            }
        }
    } else {
        out.use_dx10 = true;
        out.pf.flags = DDPF_FOURCC;
        out.pf.fourCC = make_fourcc('D', 'X', '1', '0');

        if (bytes_per_channel == 2) {
            if (channel_count == 1) {
                out.dxgi = DXGI_FORMAT_R16_FLOAT;
                return out;
            }
            if (channel_count == 2) {
                out.dxgi = DXGI_FORMAT_R16G16_FLOAT;
                return out;
            }
            if (channel_count == 4) {
                out.dxgi = DXGI_FORMAT_R16G16B16A16_FLOAT;
                return out;
            }
        }

        if (bytes_per_channel == 4) {
            if (channel_count == 1) {
                out.dxgi = DXGI_FORMAT_R32_FLOAT;
                return out;
            }
            if (channel_count == 2) {
                out.dxgi = DXGI_FORMAT_R32G32_FLOAT;
                return out;
            }
            if (channel_count == 4) {
                out.dxgi = DXGI_FORMAT_R32G32B32A32_FLOAT;
                return out;
            }
        }
    }

    throw std::runtime_error("Unsupported DDS format combination");
}

void DDS::write(const IO::FilePtr &file,
                const std::span<const uint8> data,
                const u32 width,
                const u32 height,
                const u32 depth,
                const u32 bytes_per_channel,
                const u32 channel_count,
                const bool is_float) {
    if (width == 0 || height == 0 || depth == 0) {
        throw std::runtime_error("Invalid texture dimensions");
    }

    const uint64 row_pitch = static_cast<uint64>(width) * bytes_per_channel * channel_count;
    const uint64 slice_pitch = row_pitch * height;
    const uint64 expected_size = slice_pitch * depth;

    if (data.size() != expected_size) {
        throw std::runtime_error("Input data size does not match width/height/depth/format");
    }

    const auto [use_dx10, dxgi, pf] = choose_format(bytes_per_channel, channel_count, is_float);

    DDS_HEADER header{};
    header.flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_PITCH;
    header.height = height;
    header.width = width;
    header.pitchOrLinearSize = static_cast<u32>(row_pitch);
    header.ddspf = pf;
    header.caps = DDSCAPS_TEXTURE;

    if (depth > 1) {
        header.flags |= DDSD_DEPTH;
        header.depth = depth;
        header.caps2 = DDSCAPS2_VOLUME;
    }

    file->write(DDS_MAGIC);
    file->write(header);

    if (use_dx10) {
        DDS_HEADER_DXT10 dx10{};
        dx10.dxgiFormat = dxgi;
        dx10.arraySize = 1;
        dx10.resourceDimension = depth > 1
                                     ? DDS_RESOURCE_DIMENSION_TEXTURE3D
                                     : DDS_RESOURCE_DIMENSION_TEXTURE2D;
        dx10.miscFlag = 0;
        dx10.miscFlags2 = 0;
        file->write(dx10);
    }

    if (!data.empty()) {
        if (const size_t written = file->write(data.data(), static_cast<std::streamsize>(data.size()));
            written != data.size()) {
            throw std::runtime_error("Failed to write DDS pixel data");
        }
    }
}

void DDS::write(const IO::FilePtr &file, const IO::ConstByteBufferView data, const u32 width, const u32 height, const u32 depth,
                const u32 bytes_per_channel, const u32 channel_count, const bool is_float) {
    write(file, data.as_span(), width, height, depth, bytes_per_channel, channel_count, is_float);
}
