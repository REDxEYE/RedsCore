#pragma once

// Engine-independent scene data, ported from redscore's Rust virtual_model.
#include "redscore/platform/model/data.hpp"
#include "redscore/platform/model/scene.hpp"
#include <filesystem>

class GltfHelper;

namespace ISR {
    // Serialization boundaries consume the neutral VM types above; app adapters never construct format objects.
    void to_gltf(const Scene &scene, GltfHelper &helper);

    bool save_gltf(const Scene &scene, const std::filesystem::path &path, bool embed_buffers = true,
                   bool external_textures = false);
    // Serialize Y-up VM scenes as Z-up CAST instances with reusable model and PNG sidecars.
    bool save_cast(const Scene &scene, const std::filesystem::path &path);
}

namespace VM = ISR;

