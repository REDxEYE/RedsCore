#include "serializer.hpp"
#include "redscore/platform/texture_export_session.hpp"
#include <fstream>

namespace ISR {
namespace gltf_writer {

void serialize_scene(const Scene &scene, GltfHelper &helper, const std::filesystem::path *texture_folder) {
    Serializer serializer{helper};
    serializer.texture_folder = texture_folder;
    for (const auto &root : scene.roots)
        helper.add_to_scene(serializer.node(root));
    for (const auto &s : scene.skeletons) {
        if (!s || !s->root)
            throw std::invalid_argument("Invalid skeleton instance");
        if (serializer.nodes.contains(s->root.get()))
            serializer.skin(s);
    }
    for (const auto &[s, meshes] : serializer.pending_skins) {
        auto index = serializer.skin(s);
        for (auto m : meshes)
            m->skin = index;
    }
    auto &model = helper.model();
    model.asset.version = "2.0";
    model.asset.generator = "RedsCore virtual model";
    if (!model.scenes.empty()) {
        model.defaultScene = 0;
        model.scenes[0].name = scene.name;
    }
}
} // namespace gltf_writer

void to_gltf(const Scene &scene, GltfHelper &helper) { gltf_writer::serialize_scene(scene, helper, nullptr); }

bool save_gltf(const Scene &scene, const std::filesystem::path &path, bool embed_buffers, bool external_textures) {
    GltfHelper helper;
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path());
    std::filesystem::path texture_folder;
    if (external_textures) {
        texture_folder = path.parent_path() / (path.stem().string() + "_data");
        std::filesystem::create_directories(texture_folder);
    }
    detail::TextureExportSession textures(scene);
    gltf_writer::serialize_scene(scene, helper, external_textures ? &texture_folder : nullptr);
    tinygltf::TinyGLTF writer;
    if (!writer.WriteGltfSceneToFile(&helper.model(), path.string(), false, embed_buffers, true, false))
        return false;
    for (const auto &extra : scene.extra_files) {
        auto destination = path.parent_path() / extra.name;
        std::ofstream file(destination, std::ios::binary);
        file.write(reinterpret_cast<const char *>(extra.data.data()), extra.data.size());
        if (!file)
            throw std::runtime_error("Failed to write model sidecar: " + destination.string());
    }
    return true;
}
} // namespace ISR
