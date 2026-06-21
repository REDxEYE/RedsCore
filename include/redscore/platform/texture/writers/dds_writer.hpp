//
// Created by red_eye on 4/19/26.
//

#pragma once

#include <span>

#include "redscore/int_def.h"
#include "redscore/platform/buffer/buffer.h"
#include "redscore/platform/file/file.h"
#include "redscore/platform/file/memory_file.h"
#include "redscore/platform/texture/shared/dds.hpp"

namespace DDS {
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
