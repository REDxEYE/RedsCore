//
// Created by red_eye on 4/19/26.
//

#pragma once

#include <span>

#include "redscore/int_def.h"
#include "redscore/platform/buffer/buffer.h"
#include "redscore/platform/file/file.h"
#include "redscore/platform/file/memory_file.h"

namespace DDS {
    constexpr u32 make_fourcc(const char a, const char b, const char c, const char d) {
        return static_cast<u32>(static_cast<uint8>(a))
               | (static_cast<u32>(static_cast<uint8>(b)) << 8)
               | (static_cast<u32>(static_cast<uint8>(c)) << 16)
               | (static_cast<u32>(static_cast<uint8>(d)) << 24);
    }

    enum : u32 {
        DDS_MAGIC = make_fourcc('D', 'D', 'S', ' '),

        // DDS_HEADER.flags
        DDSD_CAPS = 0x00000001,
        DDSD_HEIGHT = 0x00000002,
        DDSD_WIDTH = 0x00000004,
        DDSD_PITCH = 0x00000008,
        DDSD_PIXELFORMAT = 0x00001000,
        DDSD_MIPMAPCOUNT = 0x00020000,
        DDSD_LINEARSIZE = 0x00080000,
        DDSD_DEPTH = 0x00800000,

        // DDS_PIXELFORMAT.flags
        DDPF_ALPHAPIXELS = 0x00000001,
        DDPF_ALPHA = 0x00000002,
        DDPF_FOURCC = 0x00000004,
        DDPF_RGB = 0x00000040,
        DDPF_YUV = 0x00000200,
        DDPF_LUMINANCE = 0x00020000,

        // DDS_HEADER.caps
        DDSCAPS_COMPLEX = 0x00000008,
        DDSCAPS_TEXTURE = 0x00001000,
        DDSCAPS_MIPMAP = 0x00400000,

        // DDS_HEADER.caps2
        DDSCAPS2_CUBEMAP = 0x00000200,
        DDSCAPS2_CUBEMAP_POSITIVEX = 0x00000400,
        DDSCAPS2_CUBEMAP_NEGATIVEX = 0x00000800,
        DDSCAPS2_CUBEMAP_POSITIVEY = 0x00001000,
        DDSCAPS2_CUBEMAP_NEGATIVEY = 0x00002000,
        DDSCAPS2_CUBEMAP_POSITIVEZ = 0x00004000,
        DDSCAPS2_CUBEMAP_NEGATIVEZ = 0x00008000,
        DDSCAPS2_VOLUME = 0x00200000,

        // DX10 header (DDS_HEADER_DXT10)
        DDS_RESOURCE_DIMENSION_UNKNOWN = 0,
        DDS_RESOURCE_DIMENSION_BUFFER = 1,
        DDS_RESOURCE_DIMENSION_TEXTURE1D = 2,
        DDS_RESOURCE_DIMENSION_TEXTURE2D = 3,
        DDS_RESOURCE_DIMENSION_TEXTURE3D = 4,

        DDS_RESOURCE_MISC_TEXTURECUBE = 0x4,

        DDS_ALPHA_MODE_UNKNOWN = 0,
        DDS_ALPHA_MODE_STRAIGHT = 1,
        DDS_ALPHA_MODE_PREMULTIPLIED = 2,
        DDS_ALPHA_MODE_OPAQUE = 3,
        DDS_ALPHA_MODE_CUSTOM = 4
    };

    enum DXGI_FORMAT : u32 {
        DXGI_FORMAT_UNKNOWN = 0,

        DXGI_FORMAT_R8_UNORM = 61,
        DXGI_FORMAT_R8G8_UNORM = 49,
        DXGI_FORMAT_R8G8B8A8_UNORM = 28,

        DXGI_FORMAT_R16_UNORM = 56,
        DXGI_FORMAT_R16_UINT = 57,
        DXGI_FORMAT_R16_FLOAT = 54,

        DXGI_FORMAT_R16G16_UNORM = 35,
        DXGI_FORMAT_R16G16_UINT = 36,
        DXGI_FORMAT_R16G16_FLOAT = 34,

        DXGI_FORMAT_R16G16B16A16_UNORM = 11,
        DXGI_FORMAT_R16G16B16A16_UINT = 12,
        DXGI_FORMAT_R16G16B16A16_FLOAT = 10,

        DXGI_FORMAT_R32_FLOAT = 41,
        DXGI_FORMAT_R32_UINT = 42,

        DXGI_FORMAT_R32G32_FLOAT = 16,
        DXGI_FORMAT_R32G32_UINT = 17,

        DXGI_FORMAT_R32G32B32A32_FLOAT = 2,
        DXGI_FORMAT_R32G32B32A32_UINT = 3
    };

#pragma pack(push, 1)
    struct DDS_PIXELFORMAT {
        u32 size = 32;
        u32 flags = 0;
        u32 fourCC = 0;
        u32 rgbBitCount = 0;
        u32 rBitMask = 0;
        u32 gBitMask = 0;
        u32 bBitMask = 0;
        u32 aBitMask = 0;
    };

    struct DDS_HEADER {
        u32 size = 124;
        u32 flags = 0;
        u32 height = 0;
        u32 width = 0;
        u32 pitchOrLinearSize = 0;
        u32 depth = 0;
        u32 mipMapCount = 0;
        u32 reserved1[11];
        DDS_PIXELFORMAT ddspf;
        u32 caps = 0;
        u32 caps2 = 0;
        u32 caps3 = 0;
        u32 caps4 = 0;
        u32 reserved2 = 0;
    };

    struct DDS_HEADER_DXT10 {
        u32 dxgiFormat = 0;
        u32 resourceDimension = DDS_RESOURCE_DIMENSION_TEXTURE2D;
        u32 miscFlag = 0;
        u32 arraySize = 1;
        u32 miscFlags2 = DDS_ALPHA_MODE_UNKNOWN;
    };
#pragma pack(pop)

    struct FormatInfo {
        bool use_dx10 = false;
        DXGI_FORMAT dxgi = DXGI_FORMAT_UNKNOWN;
        DDS_PIXELFORMAT pf{};
    };

    FormatInfo choose_format(u32 bytes_per_channel, u32 channel_count, bool is_float);

    void write(const IO::FilePtr &file,
               std::span<const uint8> data,
               u32 width,
               u32 height,
               u32 depth,
               u32 bytes_per_channel,
               u32 channel_count,
               bool is_float);

    void write(
        const IO::FilePtr &file,
        IO::ConstByteBufferView data,
        u32 width,
        u32 height,
        u32 depth,
        u32 bytes_per_channel,
        u32 channel_count,
        bool is_float
    );

    inline void write_to_memory(
        std::vector<u8> &output,
        const std::span<const uint8> data,
        const u32 width,
        const u32 height,
        const u32 depth,
        const u32 bytes_per_channel,
        const u32 channel_count,
        const bool is_float
    ) {
        const auto file = IO::memory_file_from_vector(&output);
        write(file, data, width, height, depth, bytes_per_channel, channel_count, is_float);
    }
}
