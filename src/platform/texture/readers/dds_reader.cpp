//
// Created by red_eye on 5/13/26.
//

#include "redscore/platform/texture//readers/dds_reader.hpp"

DDSDXGIFormat DDS::legacy_pf_to_dxgi(const DDS_PIXELFORMAT &pf) {
    if ((pf.flags & DDPF_LUMINANCE) != 0) {
        if (pf.rgbBitCount == 8 && pf.rBitMask == 0x000000ff) {
            return DDSDXGIFormat::DXGI_FORMAT_R8_UNORM;
        }

        if (pf.rgbBitCount == 16 && pf.rBitMask == 0x0000ffff) {
            return DDSDXGIFormat::DXGI_FORMAT_R16_UNORM;
        }
    }

    if ((pf.flags & DDPF_RGB) != 0) {
        if (pf.rgbBitCount == 16 &&
            pf.rBitMask == 0x000000ff &&
            pf.gBitMask == 0x0000ff00) {
            return DDSDXGIFormat::DXGI_FORMAT_R8G8_UNORM;
        }

        if (pf.rgbBitCount == 32) {
            if (pf.rBitMask == 0x000000ff &&
                pf.gBitMask == 0x0000ff00 &&
                pf.bBitMask == 0x00ff0000 &&
                pf.aBitMask == 0xff000000) {
                return DDSDXGIFormat::DXGI_FORMAT_R8G8B8A8_UNORM;
            }
            if (pf.bBitMask == 0x000000ff &&
                pf.gBitMask == 0x0000ff00 &&
                pf.rBitMask == 0x00ff0000 &&
                pf.aBitMask == 0xff000000) {
                return DDSDXGIFormat::DXGI_FORMAT_B8G8R8A8_UNORM;
            }
        }
        throw std::runtime_error(std::format("DDS: unsupported legacy rgbBitCount {}", pf.rgbBitCount));
    }

    throw std::runtime_error("DDS: unsupported legacy pixel format");
}

Texture DDS::read_texture(const IO::FilePtr &file) {
    const auto magic = file->read_u32();
    if (magic != DDS_MAGIC) {
        throw std::runtime_error("DDS: invalid magic");
    }

    const auto header = file->read_pod<DDS_HEADER>();

    if (header.size != sizeof(DDS_HEADER)) {
        throw std::runtime_error("DDS: invalid DDS_HEADER size");
    }

    if (header.ddspf.size != sizeof(DDS_PIXELFORMAT)) {
        throw std::runtime_error("DDS: invalid DDS_PIXELFORMAT size");
    }

    DDSDXGIFormat format{};

    if ((header.ddspf.flags & DDPF_FOURCC) != 0) {
        switch (header.ddspf.fourCC) {
            case make_fourcc('D', 'X', '1', '0'): {
                const auto dx10 = file->read_pod<DDS_HEADER_DXT10>();

                if (dx10.arraySize != 1) {
                    throw std::runtime_error("DDS: texture arrays are not supported");
                }

                format = static_cast<DDSDXGIFormat>(dx10.dxgiFormat);
                break;
            }
            case make_fourcc('D', 'X', 'T', '1'): {
                format = DDSDXGIFormat::DXGI_FORMAT_BC1_UNORM;
                break;
            }
            case make_fourcc('D', 'X', 'T', '3'): {
                format = DDSDXGIFormat::DXGI_FORMAT_BC2_UNORM;
                break;
            }
            case make_fourcc('D', 'X', 'T', '5'): {
                format = DDSDXGIFormat::DXGI_FORMAT_BC3_UNORM;
                break;
            }
            default:
                throw std::runtime_error(std::format("DDS: unsupported FOURCC {}", header.ddspf.fourCC));
        }
    } else {
        format = legacy_pf_to_dxgi(header.ddspf);
    }

    const auto width = static_cast<i32>(header.width);
    const auto height = static_cast<i32>(header.height);
    const auto depth = static_cast<i16>((header.flags & DDSD_DEPTH) != 0 ? header.depth : 1);

    if (width <= 0 || height <= 0 || depth <= 0) {
        throw std::runtime_error("DDS: invalid dimensions");
    }

    const auto pos = static_cast<size_t>(file->get_position());
    const auto size = file->get_size();

    if (pos > size) {
        throw std::runtime_error("DDS: invalid stream position");
    }

    auto payload = file->read_bytes(size - pos);
    return Texture::from_dxgi(format, payload.as_span(), width, height, depth);
}
