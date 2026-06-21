//
// Created by red_eye on 5/13/26.
//

#pragma once
#include "redscore/platform/file/file.h"
#include "redscore/platform/texture/texture.h"
#include "redscore/platform/texture/shared/dds.hpp"

namespace DDS {
    DDSDXGIFormat legacy_pf_to_dxgi(const DDS_PIXELFORMAT &pf);

    Texture read_texture(const IO::FilePtr &file);
}
