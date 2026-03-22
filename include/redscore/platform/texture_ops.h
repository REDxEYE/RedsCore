// Created by RED on 14.01.2026.

#pragma once
#include <memory>
#include "texture.h"

namespace TextureOps {
    std::unique_ptr<Texture> multiply(const Texture *texture_a, const Texture *texture_b);
};


