// Created by RED on 14.01.2026.

#pragma once
#include <memory>
#include "redscore/platform/texture/texture.h"

namespace TextureOps {
    std::unique_ptr<Texture> multiply(const Texture *texture_a, const Texture *texture_b);

    bool invert_channel(Texture *texture, u32 channel);

    bool swap_channels(Texture *texture, u32 channel_a, u32 channel_b);
};


