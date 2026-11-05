#include "serializer.hpp"
#include "redscore/platform/texture_export_session.hpp"

#include <ranges>
#include <stdexcept>
#include <utility>

#include <glm/gtc/matrix_inverse.hpp>

namespace ISR::cast_detail {
std::optional<TRS> Serializer::instance_for(const glm::mat4 &placement, const AssetVariant &variant) {
    try {
        if (auto instance = try_decompose(placement * variant.inverse);
            instance && matches(matrix(*instance) * variant.model, placement))
            return instance;
    } catch (const std::invalid_argument &) {
        // A different model-space basis may make the quotient singular.
    }
    return std::nullopt;
}

void Serializer::place(const NodePtr &node, const glm::mat4 &parent, bool prefab, const glm::mat4 &local_parent) {
    if (!node)
        throw std::invalid_argument("Null CAST scene node");
    if (visiting.contains(node.get()))
        throw std::invalid_argument("CAST scene/prefab cycle");
    if (!prefab && !traversed.insert(node.get()).second)
        throw std::invalid_argument("CAST node has multiple scene parents");
    visiting.insert(node.get());
    if (node->light)
        throw std::invalid_argument("CAST cannot represent scene light: " + node->name);
    if (!node->extras.is_null() && !node->extras.empty())
        throw std::invalid_argument("CAST cannot represent node extras: " + node->name);
    const glm::mat4 global = parent * node->transform.matrix();
    const glm::mat4 local = prefab ? local_parent * node->transform.matrix() : glm::mat4(1.f);
    active_globals.emplace(node.get(), global);
    if (prefab)
        active_prefab_locals.emplace(node.get(), local);
    if (node->model) {
        const auto binding = node->skin.lock();
        glm::mat4 placement = global;
        std::optional<glm::mat4> model_local;
        if (prefab)
            model_local = local;
        if (binding) {
            if (!binding->root)
                throw std::invalid_argument("CAST skin has no skeleton root");
            // The rig root can be a sibling of the mesh in the prefab tree.
            // Resolve its transform from the nearest active ancestor, not
            // from the mesh's bone attachment (skinning already applies that).
            auto rig_global = [&](auto &&self, const NodePtr &rig_node) -> glm::mat4 {
                if (auto active = active_globals.find(rig_node.get()); active != active_globals.end())
                    return active->second;
                if (auto parent_node = rig_node->parent.lock())
                    return self(self, parent_node) * rig_node->transform.matrix();
                if (prefab || std::ranges::find(scene.roots, rig_node) == scene.roots.end())
                    throw std::invalid_argument("CAST skin root is outside the scene/prefab");
                return y_up_to_z_up * rig_node->transform.matrix();
            };
            placement = rig_global(rig_global, binding->root);
            if (prefab) {
                auto rig_local = [&](auto &&self, const NodePtr &rig_node) -> std::optional<glm::mat4> {
                    if (auto active = active_prefab_locals.find(rig_node.get()); active != active_prefab_locals.end())
                        return active->second;
                    if (auto parent_node = rig_node->parent.lock())
                        if (auto parent_local = self(self, parent_node))
                            return *parent_local * rig_node->transform.matrix();
                    return std::nullopt;
                };
                model_local = rig_local(rig_local, binding->root);
            }
        }
        TRS t;
        std::string filename;
        try {
            if (auto plain = try_decompose(placement)) {
                t = *plain;
                filename = asset(node->model, binding);
            } else {
                bool reused = false;
                if (prefab) {
                    auto &variants = prefab_assets[node.get()];
                    auto &shared = shared_prefab_assets[{node->model.get(), binding.get()}];
                    auto use = [&](const std::optional<AssetVariant> &variant) {
                        if (variant)
                            if (auto instance = instance_for(placement, *variant)) {
                                t = *instance;
                                filename = variant->filename;
                                return true;
                            }
                        return false;
                    };
                    // A fixed prefab-local Model TRS lets different world placements
                    // share geometry. A cached basis is valid only if its remaining
                    // Instance transform is TRS and reconstructs this placement.
                    reused = use(variants.local) || use(variants.fallback) || use(shared.local) || use(shared.fallback);
                    if (!reused && model_local)
                        if (auto fixed = try_decompose(*model_local)) {
                            AssetVariant variant{*model_local, glm::inverse(*model_local), {}};
                            if (auto instance = instance_for(placement, variant)) {
                                t = *instance;
                                filename = asset(node->model, binding, &*fixed);
                                variant.filename = filename;
                                if (!shared.local)
                                    shared.local = variant;
                                variants.local = std::move(variant);
                                reused = true;
                            }
                        }
                }
                if (!reused) {
                    const auto factors = factor_shear(placement);
                    t = factors.instance;
                    filename = asset(node->model, binding, &factors.model);
                    if (prefab) {
                        auto &variants = prefab_assets[node.get()];
                        auto &shared = shared_prefab_assets[{node->model.get(), binding.get()}];
                        if (!variants.fallback || !shared.fallback) {
                            const auto model_matrix = matrix(factors.model);
                            AssetVariant variant{model_matrix, glm::inverse(model_matrix), filename};
                            if (!variants.fallback)
                                variants.fallback = variant;
                            if (!shared.fallback)
                                shared.fallback = std::move(variant);
                        }
                    }
                }
            }
        } catch (const std::invalid_argument &e) {
            throw std::invalid_argument("CAST node '" + node->name + "': " + e.what());
        }
        main.begin(instance_id);
        main.text("n", node->name);
        main.link("rf", main.next());
        position(main, t);
        main.begin(file_id);
        main.text("p", filename);
        main.end();
        main.end();
    }
    if (node->prefab)
        place(node->prefab, global, true, glm::mat4(1.f));
    for (const auto &child : node->children)
        place(child, global, prefab, local);
    if (prefab)
        active_prefab_locals.erase(node.get());
    active_globals.erase(node.get());
    visiting.erase(node.get());
}

Serializer::Serializer(const Scene &s, const std::filesystem::path &path, const std::filesystem::path &dir)
    : scene(s), folder(dir), names(s, path, dir), main(path) {}

void Serializer::run() {
    main.begin(root_id);
    main.begin(metadata_id);
    main.text("s", "RedsCore virtual model");
    main.text("sr", folder.filename().generic_string());
    main.text("vm_scene_name", scene.name);
    main.end();
    for (const auto &root : scene.roots)
        place(root, y_up_to_z_up, false, glm::mat4(1.f));
    for (const auto &skin : scene.skeletons) {
        if (!skin || !skin->root)
            throw std::invalid_argument("Invalid CAST skeleton instance");
        if (traversed.contains(skin->root.get()) && !used_skins.contains(skin.get()))
            throw std::invalid_argument("CAST cannot represent a standalone scene skeleton without a model");
    }
    main.end();
}

} // namespace ISR::cast_detail

namespace ISR {
bool save_cast(const Scene &scene, const std::filesystem::path &destination) {
    if (destination.empty() || destination.filename().empty())
        throw std::invalid_argument("CAST output path needs a filename");
    std::filesystem::path path = destination;
    if (path.extension().empty())
        path += ".cast";
    if (!path.parent_path().empty())
        std::filesystem::create_directories(path.parent_path());
    const auto folder = path.parent_path() / (path.stem().string() + "_data");
    std::filesystem::create_directories(folder);

    detail::TextureExportSession textures(scene);
    cast_detail::Serializer(scene, path, folder).run();

    for (const auto &[name, data] : scene.extra_files) {
        const auto target = path.parent_path() / name;
        if (!target.parent_path().empty())
            std::filesystem::create_directories(target.parent_path());
        cast_detail::write_bytes(target, data);
    }
    return true;
}
} // namespace ISR
