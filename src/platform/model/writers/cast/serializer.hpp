#pragma once

#include "transform.hpp"

#include <tuple>
#include <unordered_map>
#include <unordered_set>

namespace ISR::cast_detail {
struct AssetKey {
    const Model *model;
    const SkeletonInstance *skin;

    bool operator==(const AssetKey &) const = default;
};

struct AssetHash {
    size_t operator()(const AssetKey &k) const {
        return std::hash<const void *>{}(k.model) ^ (std::hash<const void *>{}(k.skin) << 1);
    }
};

struct AssetVariant {
    glm::mat4 model, inverse;
    std::string filename;
};

struct PrefabAssets {
    std::optional<AssetVariant> local, fallback;
};
class Serializer {
    const Scene &scene;
    std::filesystem::path folder;
    ResourceNames names;
    Writer main;
    std::unordered_map<AssetKey, std::string, AssetHash> assets;
    std::unordered_map<const Node *, PrefabAssets> prefab_assets;
    std::unordered_map<AssetKey, PrefabAssets, AssetHash> shared_prefab_assets;
    size_t next_asset_id = 0, next_texture_id = 0;
    std::unordered_map<const TextureReference *, std::string> textures;
    std::unordered_set<const Node *> visiting, traversed;
    std::unordered_map<const Node *, glm::mat4> active_globals;
    std::unordered_map<const Node *, glm::mat4> active_prefab_locals;
    std::unordered_set<const SkeletonInstance *> used_skins;

    std::string texture(const TexturePtr &ref);
    static void material_properties(Writer &w, const Material &m);
    void write_material(Writer &w, const Material &m);
    static void mesh(Writer &w, const Primitive &p, std::string_view name, uint64_t material_hash, size_t bone_count,
                     std::vector<std::tuple<uint64_t, const Primitive *, size_t, std::string>> &shapes,
                     const std::vector<std::string> &target_names);
    static void shape(Writer &w, uint64_t base, const Primitive &p, size_t target, std::string_view name);
    static void skeleton(Writer &w, const Skeleton &s);
    static uint32_t framerate(const std::vector<Animation> &animations);
    static void animations(Writer &w, const std::vector<Animation> &animations, const Skeleton *s);
    std::string asset(const std::shared_ptr<Model> &model, const SkinPtr &skin, const TRS *model_transform = nullptr);
    static std::optional<TRS> instance_for(const glm::mat4 &placement, const AssetVariant &variant);
    void place(const NodePtr &node, const glm::mat4 &parent, bool prefab, const glm::mat4 &local_parent);

  public:
    Serializer(const Scene &s, const std::filesystem::path &path, const std::filesystem::path &dir);
    void run();
};

} // namespace ISR::cast_detail
