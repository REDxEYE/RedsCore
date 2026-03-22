//
// Created by red_eye on 3/22/26.
//

#pragma once

#include "redscore/platform/buffer/buffer.h"

std::optional<IO::Buffer>  decompress_zlib(const IO::Buffer& in, u64 decompressed_size, int32 wbits);
std::optional<IO::Buffer>  decompress_zlib(const IO::Buffer& in, u64 decompressed_size);
std::optional<IO::Buffer>  decompress_zstd(const IO::Buffer& in, u64 decompressed_size);


