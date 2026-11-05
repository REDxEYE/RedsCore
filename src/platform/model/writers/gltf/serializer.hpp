#pragma once
#include "redscore/platform/model/model.hpp"
#include "redscore/platform/gltf_helper.h"
#include <unordered_map>
#include <unordered_set>

namespace ISR::gltf_writer {
using NodeHandle = GltfHelper::Handle<tinygltf::Node>;

struct Serializer {
    GltfHelper &out;
    const std::filesystem::path *texture_folder = nullptr;

    struct PrefabInstance {
        std::unordered_map<const Node *, NodeHandle> nodes;
        std::unordered_map<const SkeletonInstance *, int> skins;
        std::vector<std::pair<SkinPtr, std::vector<NodeHandle>>> pending_skins;
    };

    std::unordered_map<const Node *, NodeHandle> nodes;
    std::unordered_map<const Material *, int> materials;
    std::unordered_map<const TextureReference *, int> textures;
    std::unordered_map<const Mesh *, int> meshes;
    std::unordered_map<const SkeletonInstance *, int> skins;
    std::unordered_map<const Skeleton *, int> inverse_bind_matrices;
    std::vector<std::pair<SkinPtr, std::vector<NodeHandle>>> pending_skins;
    std::vector<SkinPtr> owned_skins;
    std::unordered_set<const Node *> visiting;

    int texture(const TexturePtr &t);
    int material(const std::shared_ptr<Material> &m);
    int mesh(const Mesh &mesh);
    int light(const Light &light);
    NodeHandle node(const NodePtr &n, PrefabInstance *instance = nullptr);
    void animation(const Animation &a, const SkinPtr &binding,
                   const std::unordered_map<const Node *, NodeHandle> &instance_nodes);
    int skin(const SkinPtr &s, PrefabInstance *instance = nullptr);
};

void serialize_scene(const Scene &scene, GltfHelper &helper, const std::filesystem::path *texture_folder);
} // namespace ISR::gltf_writer
