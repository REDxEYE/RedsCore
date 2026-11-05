#include "serializer.hpp"

#include <array>
#include <cmath>
#include <ranges>
#include <stdexcept>

namespace ISR::cast_detail {
std::string Serializer::texture(const TexturePtr &ref) {
    if (auto it = textures.find(ref.get()); it != textures.end())
        return it->second;
    DataBuffer loaded;
    const DataBuffer *bytes = &ref->png;
    if (ref->load_png) {
        loaded = ref->load_png();
        bytes = &loaded;
    }
    if (bytes->empty()) {
        textures.emplace(ref.get(), std::string{});
        return {};
    }
    const auto name = names.allocate(ref->name, "texture", ".png", next_texture_id++);
    write_bytes(folder / name, *bytes);
    textures.emplace(ref.get(), name);
    return name;
}

void Serializer::material_properties(Writer &w, const Material &m) {
    // Nonstandard vm_* properties preserve VM PBR controls where CAST has no standard slot.
    w.scalar("vm_metallic", m.metallic_factor);
    w.scalar("vm_roughness", m.roughness_factor);
    w.scalar("vm_normal_scale", m.normal_scale);
    w.scalar("vm_alpha_cutoff", m.alpha_cutoff);
    w.scalar("vm_specular_factor", m.specularFactor);
    w.vec3("vm_specular_color", m.specularColorFactor);
    w.vec3("vm_emissive_factor", m.emissive_factor);
    w.text("vm_alpha_mode", m.alpha_mode == AlphaMode::Mask    ? "MASK"
                            : m.alpha_mode == AlphaMode::Blend ? "BLEND"
                                                               : "OPAQUE");
    w.byte("vm_double_sided", m.double_sided);
    w.byte("vm_specular_used", m.specularUsed);
}

void Serializer::write_material(Writer &w, const Material &m) {
    w.begin(material_id);
    w.text("n", m.name);
    w.text("t", "pbr");
    material_properties(w, m);
    w.vec4("vm_base_color", m.base_color);
    struct Slot {
        std::string_view name;
        TexturePtr ref;
        std::string path;
    };
    std::array<Slot, 7> slots{{{"albedo", m.albedo, {}},
                               {"normal", m.normal, {}},
                               {"specular", m.specular, {}},
                               {"emissive", m.emissive, {}},
                               {"ao", m.occlusion, {}},
                               {"extra0", m.metallic_roughness, {}},
                               {"extra1", m.specularColor, {}}}};
    uint64_t hash = w.next();
    for (auto &slot : slots) {
        if (!slot.ref)
            continue;
        slot.path = texture(slot.ref);
        if (!slot.path.empty())
            w.link(slot.name, hash++);
    }
    const bool missing_albedo = slots.front().path.empty();
    if (missing_albedo)
        w.link("albedo", hash);
    for (const auto &slot : slots) {
        if (slot.path.empty())
            continue;
        w.begin(file_id);
        w.text("p", slot.path);
        w.end();
    }
    if (missing_albedo) {
        w.begin(color_id);
        w.text("cs", "linear");
        w.vec4("rgba", m.base_color);
        w.end();
    }
    w.end();
}

void Serializer::skeleton(Writer &w, const Skeleton &s) {
    if (s.bones.empty())
        throw std::invalid_argument("CAST skeleton has no bones");
    narrow(s.bones.size(), "bone count");
    std::vector<uint8_t> state(s.bones.size());
    auto check = [&](auto &&self, const size_t i) -> void {
        if (state[i] == 1)
            throw std::invalid_argument("CAST skeleton cycle");
        if (state[i] == 2)
            return;
        state[i] = 1;
        int32_t parent = s.bones[i].parent;
        if (parent < -1 || (parent >= 0 && size_t(parent) >= s.bones.size()))
            throw std::invalid_argument("CAST invalid bone parent");
        if (parent >= 0)
            self(self, parent);
        state[i] = 2;
    };
    for (size_t i = 0; i < s.bones.size(); ++i)
        check(check, i);
    w.begin(skeleton_id);
    std::unordered_set<std::string> names;
    for (const auto &b : s.bones) {
        if (b.name.empty() || !names.insert(b.name).second)
            throw std::invalid_argument("CAST bone names must be unique and nonempty");
        TRS t;
        try {
            t = decompose(b.transform.matrix());
        } catch (const std::invalid_argument &e) {
            throw std::invalid_argument("CAST bone '" + b.name + "': " + e.what());
        }
        w.begin(bone_id);
        w.text("n", b.name);
        w.integer("p", static_cast<uint32_t>(b.parent));
        w.vec3("lp", t.p);
        w.vec4("lr", {t.r.x, t.r.y, t.r.z, t.r.w});
        w.vec3("s", t.s);
        w.byte("ssc", 0);
        w.end();
    }
    w.end();
}

uint32_t Serializer::framerate(const std::vector<Animation> &animations) {
    for (uint32_t fps : {1u, 24u, 30u, 60u, 120u, 240u}) {
        bool valid = true;
        for (const auto &a : animations)
            for (const auto &c : a.channels)
                for (float time : c.times)
                    if (!std::isfinite(time) || time < 0.f || double(time) * fps > UINT32_MAX ||
                        std::abs(double(time) * fps - std::round(double(time) * fps)) > 1e-4)
                        valid = false;
        if (valid)
            return fps;
    }
    throw std::invalid_argument(
        "CAST animation seconds cannot be represented by integer frames at 1/24/30/60/120/240 fps");
}

void Serializer::animations(Writer &w, const std::vector<Animation> &animations, const Skeleton *s) {
    if (animations.empty())
        return;
    if (!s)
        throw std::invalid_argument("CAST animation requires a skeleton");
    const uint32_t fps = framerate(animations);
    for (const auto &a : animations) {
        w.begin(animation_id);
        w.text("n", a.name);
        w.scalar("fr", float(fps));
        for (const auto &c : a.channels) {
            if (c.interpolation != Interpolation::Linear)
                throw std::invalid_argument("CAST supports only linear animation interpolation");
            if (c.path == AnimationPath::Weights)
                throw std::invalid_argument("CAST morph-weight animation requires explicit blend shape target curves");
            if (std::ranges::none_of(s->bones, [&](const Bone &b) { return b.name == c.bone; }))
                throw std::invalid_argument("CAST animation bone not found: " + c.bone);
            const size_t lanes = c.path == AnimationPath::Rotation ? 4 : 3;
            if (c.times.empty() || c.values.size() / lanes != c.times.size() || c.values.size() % lanes)
                throw std::invalid_argument("CAST invalid animation sample count");
            uint32_t previous = 0;
            for (size_t i = 0; i < c.times.size(); ++i) {
                const uint32_t frame =
                    narrow(static_cast<size_t>(std::round(double(c.times[i]) * fps)), "animation frame");
                if (i && frame <= previous)
                    throw std::invalid_argument("CAST animation frames must increase");
                previous = frame;
            }
            const std::array<std::string_view, 3> axes = c.path == AnimationPath::Translation
                                                             ? std::array<std::string_view, 3>{"tx", "ty", "tz"}
                                                             : std::array<std::string_view, 3>{"sx", "sy", "sz"};
            for (size_t axis = 0; axis < (lanes == 4 ? 1 : 3); ++axis) {
                w.begin(curve_id);
                w.text("nn", c.bone);
                w.text("kp", lanes == 4 ? "rq" : axes[axis]);
                w.text("m", "absolute");
                w.property("kb", "i", c.times.size(), [&] {
                    for (float time : c.times)
                        w.u32(static_cast<uint32_t>(std::round(double(time) * fps)));
                });
                w.property("kv", lanes == 4 ? "4v" : "f", c.times.size(), [&] {
                    for (size_t i = 0; i < c.times.size(); ++i) {
                        if (lanes == 4)
                            for (size_t k = 0; k < 4; ++k)
                                w.f32(c.values[i * 4 + k]);
                        else
                            w.f32(c.values[i * 3 + axis]);
                    }
                });
                w.end();
            }
        }
        w.end();
    }
}

std::string Serializer::asset(const std::shared_ptr<Model> &model, const SkinPtr &skin, const TRS *model_transform) {
    if (!model)
        throw std::invalid_argument("Null CAST model");
    const AssetKey key{model.get(), skin.get()};
    if (!model_transform)
        if (auto it = assets.find(key); it != assets.end())
            return it->second;
    if (skin)
        used_skins.insert(skin.get());
    const Skeleton *skel = skin ? skin->skeleton.get() : model->skeleton.get();
    if (skin && (!skel || (model->skeleton && model->skeleton != skin->skeleton)))
        throw std::invalid_argument("CAST mesh skin does not match model skeleton");
    if (skin && (skin->joints.size() != skel->bones.size() || !skin->root))
        throw std::invalid_argument("CAST skeleton binding has invalid joints/root");
    if (skin)
        for (size_t i = 0; i < skin->joints.size(); ++i) {
            const auto &joint = skin->joints[i];
            if (!joint || joint->name != skel->bones[i].name)
                throw std::invalid_argument("CAST skeleton instance joint order/name differs from definition");
            const auto actual = joint->transform.matrix();
            const auto expected = skel->bones[i].transform.matrix();
            for (int col = 0; col < 4; ++col)
                for (int row = 0; row < 4; ++row)
                    if (!std::isfinite(actual[col][row]) || std::abs(actual[col][row] - expected[col][row]) > 1e-4f)
                        throw std::invalid_argument("CAST skeleton instance rest transform differs from definition");
        }
    const auto filename = names.allocate(model->name, "model", ".cast", next_asset_id++);
    Writer w(folder / filename);
    w.begin(root_id);
    w.begin(model_id);
    w.text("n", model->name);
    if (model_transform)
        position(w, *model_transform);
    if (skel)
        skeleton(w, *skel);
    std::unordered_map<const Material *, uint64_t> material_hashes;
    for (const auto &sub : model->submodels)
        for (const auto &m : sub.meshes)
            for (const auto &p : m.primitives)
                if (p.material && !material_hashes.contains(p.material.get())) {
                    const uint64_t hash = w.next();
                    write_material(w, *p.material);
                    material_hashes.emplace(p.material.get(), hash);
                }
    std::vector<std::tuple<uint64_t, const Primitive *, size_t, std::string>> shapes;
    for (const auto &sub : model->submodels)
        for (const auto &m : sub.meshes) {
            for (const auto &p : m.primitives)
                if (!m.target_names.empty() && m.target_names.size() != p.morph_targets.size())
                    throw std::invalid_argument("CAST morph target name count differs from targets");
            for (size_t i = 0; i < m.primitives.size(); ++i) {
                const auto &p = m.primitives[i];
                const auto name =
                    !p.name.empty() ? p.name : m.name + (m.primitives.size() == 1 ? "" : "_" + std::to_string(i));
                mesh(w, p, name, p.material ? material_hashes.at(p.material.get()) : 0, skel ? skel->bones.size() : 0,
                     shapes, m.target_names);
            }
        }
    for (const auto &[hash, p, target, name] : shapes)
        shape(w, hash, *p, target, name);
    w.end();
    animations(w, model->animations, skel);
    if (skin)
        animations(w, skin->animations, skel);
    w.end();
    if (!model_transform)
        assets.emplace(key, filename);
    return filename;
}

} // namespace ISR::cast_detail
