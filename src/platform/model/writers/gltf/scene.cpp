#include "serializer.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <glm/gtc/matrix_transform.hpp>

namespace ISR::gltf_writer {
namespace {
tinygltf::Value json_value(const nlohmann::json &j) {
    if (j.is_null())
        return {};
    if (j.is_boolean())
        return tinygltf::Value(j.get<bool>());
    if (j.is_number())
        return tinygltf::Value(j.get<double>());
    if (j.is_string())
        return tinygltf::Value(j.get<std::string>());
    if (j.is_array()) {
        tinygltf::Value::Array a;
        for (const auto &v : j)
            a.push_back(json_value(v));
        return tinygltf::Value(a);
    }
    tinygltf::Value::Object o;
    for (const auto &[k, v] : j.items())
        o.emplace(k, json_value(v));
    return tinygltf::Value(o);
}
} // namespace
int Serializer::light(const Light &light) {
    const auto &c = light.color;
    if (!std::isfinite(c.x) || !std::isfinite(c.y) || !std::isfinite(c.z) || c.x < 0.f || c.y < 0.f || c.z < 0.f ||
        c.x > 1.f || c.y > 1.f || c.z > 1.f)
        throw std::invalid_argument("Invalid light color");
    if (!std::isfinite(light.intensity) || light.intensity < 0.f)
        throw std::invalid_argument("Invalid light intensity");
    if (!std::isfinite(light.range) || light.range < 0.f)
        throw std::invalid_argument("Invalid light range");
    if (light.type == LightType::Spot &&
        (!std::isfinite(light.inner_cone_angle) || !std::isfinite(light.outer_cone_angle) ||
         light.inner_cone_angle < 0.f || light.inner_cone_angle >= light.outer_cone_angle ||
         light.outer_cone_angle > 1.5707963267948966f))
        throw std::invalid_argument("Invalid spot cone angles");
    auto &model = out.model();
    if (model.lights.size() >= static_cast<size_t>(std::numeric_limits<int32_t>::max()))
        throw std::invalid_argument("Light count exceeds int32 limit");
    tinygltf::Light g;
    g.name = light.name;
    g.color = {c.x, c.y, c.z};
    g.intensity = light.intensity;
    switch (light.type) {
    case LightType::Directional:
        if (light.range != 0.f)
            throw std::invalid_argument("Directional light cannot have a range");
        g.type = "directional";
        break;
    case LightType::Point:
        g.type = "point";
        break;
    case LightType::Spot:
        g.type = "spot";
        g.spot.innerConeAngle = light.inner_cone_angle;
        g.spot.outerConeAngle = light.outer_cone_angle;
        break;
    default:
        throw std::invalid_argument("Invalid light type");
    }
    g.range = light.range;
    const int index = static_cast<int>(model.lights.size());
    model.lights.push_back(std::move(g));
    out.add_extension("KHR_lights_punctual", false);
    return index;
}
NodeHandle Serializer::node(const NodePtr &n, PrefabInstance *instance) {
    if (!n)
        throw std::invalid_argument("Null scene node");
    if (visiting.contains(n.get()))
        throw std::invalid_argument("Scene/prefab cycle");
    auto &instance_nodes = instance ? instance->nodes : nodes;
    if (instance_nodes.contains(n.get()))
        throw std::invalid_argument("Node has multiple parents; instance the model on separate nodes");
    visiting.insert(n.get());
    auto g = out.make<tinygltf::Node>();
    instance_nodes[n.get()] = g;
    g->name = n->name;
    if (n->light)
        g->light = light(*n->light);
    if (n->transform.matrix_override) {
        // Preserve affine matrices exactly, including shear; do not decompose them to TRS.
        const auto &matrix = *n->transform.matrix_override;
        for (int col = 0; col < 4; ++col)
            for (int row = 0; row < 4; ++row) {
                if (!std::isfinite(matrix[col][row]))
                    throw std::invalid_argument("Non-finite node matrix");
                g->matrix.push_back(matrix[col][row]);
            }
    } else {
        GltfHelper::set_node_transform(g, n->transform.translation, n->transform.scale, n->transform.rotation);
    }
    if (!n->extras.is_null())
        g->extras = json_value(n->extras);
    SkinPtr binding = n->skin.lock();
    if (n->model && n->model->skeleton && !binding) {
        SceneBuilder b;
        binding = b.add_skeleton(n->model->skeleton);
        binding->animations = n->model->animations;
        owned_skins.push_back(binding);
        auto root = node(binding->root, instance);
        out.set_parent(g, root);
    }
    std::vector<NodeHandle> skinned_meshes;
    if (n->model) {
        const auto &submodels = n->model->submodels;
        if (submodels.size() == 1 && submodels.front().meshes.size() == 1) {
            g->mesh = mesh(submodels.front().meshes.front());
            if (binding)
                skinned_meshes.push_back(g);
        } else {
            for (const auto &sub : submodels) {
                auto subnode = out.make<tinygltf::Node>();
                subnode->name = sub.name;
                out.set_parent(g, subnode);
                for (const auto &m : sub.meshes) {
                    auto mn = out.make<tinygltf::Node>();
                    mn->name = m.name;
                    mn->mesh = mesh(m);
                    out.set_parent(subnode, mn);
                    if (binding)
                        skinned_meshes.push_back(mn);
                }
            }
        }
    }
    if (binding)
        (instance ? instance->pending_skins : pending_skins).emplace_back(binding, std::move(skinned_meshes));
    if (n->prefab) {
        PrefabInstance prefab;
        auto copy = node(n->prefab, &prefab);
        out.set_parent(g, copy);
        for (const auto &[skin_binding, meshes] : prefab.pending_skins) {
            const int index = skin(skin_binding, &prefab);
            for (auto mesh_node : meshes)
                mesh_node->skin = index;
        }
    }
    for (const auto &child : n->children) {
        auto c = node(child, instance);
        out.set_parent(g, c);
    }
    visiting.erase(n.get());
    return g;
}
void Serializer::animation(const Animation &a, const SkinPtr &binding,
                           const std::unordered_map<const Node *, NodeHandle> &instance_nodes) {
    auto g = out.make<tinygltf::Animation>();
    g->name = a.name;
    for (const auto &c : a.channels) {
        if (c.path == AnimationPath::Weights)
            throw std::invalid_argument("Morph animation requires morph targets (not implemented)");
        auto found =
            std::find_if(binding->joints.begin(), binding->joints.end(), [&](auto &j) { return j->name == c.bone; });
        if (found == binding->joints.end())
            throw std::invalid_argument("Animation bone not found: " + c.bone);
        auto width = c.path == AnimationPath::Rotation ? 4u : 3u;
        auto multiplier = c.interpolation == Interpolation::CubicSpline ? 3u : 1u;
        if (c.times.empty() || c.values.size() != c.times.size() * width * multiplier)
            throw std::invalid_argument("Invalid animation sample count");
        float previous = -1.f;
        for (float t : c.times) {
            if (!std::isfinite(t) || t < 0 || t <= previous)
                throw std::invalid_argument("Invalid animation times");
            previous = t;
        }
        for (float v : c.values)
            if (!std::isfinite(v))
                throw std::invalid_argument("Non-finite animation sample");
        auto input = out.create_accessor_chain(reinterpret_cast<const uint8_t *>(c.times.data()), c.times.size() * 4, 0,
                                               TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_SCALAR, c.times.size());
        input.accessor->minValues = {c.times.front()};
        input.accessor->maxValues = {c.times.back()};
        auto output = out.create_accessor_chain(
            reinterpret_cast<const uint8_t *>(c.values.data()), c.values.size() * 4, 0, TINYGLTF_COMPONENT_TYPE_FLOAT,
            width == 4 ? TINYGLTF_TYPE_VEC4 : TINYGLTF_TYPE_VEC3, c.values.size() / width);
        tinygltf::AnimationSampler sampler;
        sampler.input = input.accessor.index();
        sampler.output = output.accessor.index();
        sampler.interpolation = c.interpolation == Interpolation::Step          ? "STEP"
                                : c.interpolation == Interpolation::CubicSpline ? "CUBICSPLINE"
                                                                                : "LINEAR";
        tinygltf::AnimationChannel channel;
        channel.sampler = static_cast<int>(g->samplers.size());
        channel.target_node = instance_nodes.at(found->get()).index();
        channel.target_path = c.path == AnimationPath::Translation ? "translation"
                              : c.path == AnimationPath::Rotation  ? "rotation"
                                                                   : "scale";
        g->samplers.push_back(sampler);
        g->channels.push_back(channel);
    }
}
int Serializer::skin(const SkinPtr &s, PrefabInstance *instance) {
    if (!s || !s->skeleton || s->joints.size() != s->skeleton->bones.size() || s->joints.empty())
        throw std::invalid_argument("Invalid skeleton instance");
    auto &instance_skins = instance ? instance->skins : skins;
    if (auto i = instance_skins.find(s.get()); i != instance_skins.end())
        return i->second;
    const auto &instance_nodes = instance ? instance->nodes : nodes;
    auto g = out.make<tinygltf::Skin>();
    instance_skins[s.get()] = g.index();
    g->name = s->skeleton->name;
    g->skeleton = instance_nodes.at(s->root.get()).index();
    g->joints.reserve(s->joints.size());
    for (const auto &joint : s->joints)
        g->joints.push_back(instance_nodes.at(joint.get()).index());

    if (auto existing = inverse_bind_matrices.find(s->skeleton.get()); existing != inverse_bind_matrices.end()) {
        g->inverseBindMatrices = existing->second;
    } else {
        // Bind transforms depend on the shared skeleton definition, not placement.
        std::vector<float> inverse;
        inverse.reserve(s->joints.size() * 16);
        std::vector<glm::mat4> globals(s->joints.size());
        std::vector<int> state(s->joints.size());
        std::function<glm::mat4(size_t)> global = [&](size_t i) {
            if (state[i] == 1)
                throw std::invalid_argument("Skeleton cycle");
            if (state[i] == 2)
                return globals[i];
            state[i] = 1;
            auto parent = s->skeleton->bones[i].parent;
            if (parent < -1 || parent >= static_cast<int64_t>(globals.size()))
                throw std::invalid_argument("Invalid bone parent");
            globals[i] = (parent < 0 ? glm::mat4(1.f) : global(parent)) * s->skeleton->bones[i].transform.matrix();
            state[i] = 2;
            return globals[i];
        };
        for (size_t i = 0; i < s->joints.size(); ++i) {
            auto matrix = global(i);
            if (std::abs(glm::determinant(matrix)) < 1e-12f)
                throw std::invalid_argument("Singular bind transform");
            auto inv = glm::inverse(matrix);
            for (int col = 0; col < 4; ++col)
                for (int row = 0; row < 4; ++row)
                    inverse.push_back(inv[col][row]);
        }
        g->inverseBindMatrices =
            out.create_accessor_chain(reinterpret_cast<const uint8_t *>(inverse.data()), inverse.size() * 4, 0,
                                      TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_MAT4, s->joints.size())
                .accessor.index();
        inverse_bind_matrices.emplace(s->skeleton.get(), g->inverseBindMatrices);
    }
    for (const auto &a : s->animations)
        animation(a, s, instance_nodes);
    return g.index();
}
} // namespace ISR::gltf_writer
