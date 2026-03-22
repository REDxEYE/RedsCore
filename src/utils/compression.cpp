//
// Created by red_eye on 3/22/26.
//

#include "zlib-ng.h"
#include "zutil.h"

#include "zstd.h"

#include "redscore/utils/compression.hpp"

#include <format>


std::optional<IO::Buffer> decompress_zlib(const IO::Buffer &in, const u64 decompressed_size, int32 wbits) {
    const auto in_len = in.size();
    const auto expected_out_len = decompressed_size;

    IO::Buffer out(expected_out_len);

    zng_stream s{};
    int rc = zng_inflateInit2(&s, wbits);
    if (rc != Z_OK)
        throw std::runtime_error(std::format("Decompression failed: {}", zng_z_errmsg[rc]));

    if (in_len > UINT_MAX || expected_out_len > UINT_MAX) {
        zng_inflateEnd(&s);
        throw std::runtime_error("Decompression failed, input buffer too large");
    }

    s.next_in = in.data();
    s.avail_in = in_len;
    s.next_out = out.data();
    s.avail_out = expected_out_len;

    rc = zng_inflate(&s, Z_FINISH);

    const auto in_consumed = static_cast<size_t>(s.next_in - in.data());

    zng_inflateEnd(&s);

    if (in_consumed != in_len) {
        throw std::runtime_error("Decompression failed, input buffer was not fully consumed");
    }
    if (rc != Z_STREAM_END)
        throw std::runtime_error(std::format("Decompression failed: {}", zng_z_errmsg[rc]));

    if (s.total_out != expected_out_len)
        throw std::runtime_error("Decompression failed, decompressed size did not match expected size");

    return out;
}

std::optional<IO::Buffer> decompress_zlib(const IO::Buffer &in, const u64 decompressed_size) {
    return decompress_zlib(in, decompressed_size, 15);
}

std::optional<IO::Buffer> decompress_zstd(const IO::Buffer &in, const u64 decompressed_size) {
    IO::Buffer out(decompressed_size);
    if (const auto rc = ZSTD_decompress(out.data(), decompressed_size, in.data(), in.size()); ZSTD_isError(rc)) {
        throw std::runtime_error(std::format("Decompression failed: {}", ZSTD_getErrorName(rc)));
    }
    return out;
}
