// Created by RED on 14.01.2026.

#pragma once
#include <memory>
#include "glm/glm.hpp"
#include "redscore/platform/texture/texture.h"

struct TextureChannels {
    static constexpr uint32 R=0;
    static constexpr uint32 G=1;
    static constexpr uint32 B=2;
    static constexpr uint32 A=3;
};

namespace TextureOps {
    std::unique_ptr<Texture> multiply(const Texture::Ptr &texture_a, const Texture::Ptr &texture_b);

    bool multiply_scalar(const Texture::Ptr &texture, const glm::vec4 &scalars);

    bool invert_channel(const Texture::Ptr &texture, u32 channel);

    bool swap_channels(const Texture::Ptr &texture, u32 channel_a, u32 channel_b);

    bool fill_channel(const Texture::Ptr &texture, u32 channel, float value);

    Texture::Ptr extract_channel(const Texture::Ptr &texture, u32 channel);

    bool insert_channel(const Texture::Ptr &texture, u32 channel, const Texture::Ptr &channel_texture);
};
