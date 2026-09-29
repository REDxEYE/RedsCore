#include "redscore/platform/model/model.hpp"
#include "redscore/platform/gltf_helper.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <glm/gtc/matrix_transform.hpp>

namespace ISR {
    namespace {
        DataBuffer copy_bytes(const void *data, size_t bytes) {
            if (bytes && !data) throw std::invalid_argument("Null model buffer");
            DataBuffer result(bytes);
            if (bytes) std::memcpy(result.data(), data, bytes);
            return result;
        }
    }

    TexturePtr png_texture(std::string name, DataBuffer data) {
        return std::make_shared<TextureReference>(std::move(name), std::move(data), nullptr);
    }

    void Primitive::set_attribute(const VertexAttribute &attribute) {
        attributes.push_back({
            attribute.usage, attribute.set, attribute.custom_name, attribute.format, attribute.type,
            attribute.normalized, attribute.count, copy_bytes(attribute.data.data(), attribute.data.size())
        });
    }

    void Primitive::set_indices(const void *data, size_t bytes, IndexType type, size_t count) {
        indices = copy_bytes(data, bytes);
        index_type = type;
        index_count = count;
    }

    glm::mat4 Transform::matrix() const {
        if (matrix_override) return *matrix_override;
        return glm::translate(glm::mat4(1.f), translation) * glm::mat4_cast(rotation) * glm::scale(
                   glm::mat4(1.f), scale);
    }

    NodePtr SceneBuilder::create_node(std::string name) {
        auto node = std::make_shared<Node>();
        node->name = std::move(name);
        return node;
    }

    std::shared_ptr<Material> SceneBuilder::find_material(std::string_view name) const {
        for (const auto &m: materials) if (m->name == name) return m;
        return {};
    }

    std::shared_ptr<Material> SceneBuilder::material(std::string_view name) {
        if (auto existing = find_material(name)) return existing;
        auto m = std::make_shared<Material>();
        m->name = name;
        materials.push_back(m);
        return m;
    }

    void SceneBuilder::add_to_scene(const NodePtr &node) {
        if (!node || !node->parent.expired()) return;
        if (std::find(scene.roots.begin(), scene.roots.end(), node) == scene.roots.end()) scene.roots.push_back(node);
    }

    void SceneBuilder::set_parent(const NodePtr &parent, const NodePtr &child) {
        if (!parent || !child) throw std::invalid_argument("Null scene parent/child");
        for (auto p = parent; p; p = p->parent.lock()) if (p == child) throw std::invalid_argument("Scene cycle");
        if (auto previous = child->parent.lock()) std::erase(previous->children, child);
        std::erase(scene.roots, child);
        child->parent = parent;
        if (std::ranges::find(parent->children, child) == parent->children.end())
            parent->children.push_back(child);
    }

    glm::mat4 SceneBuilder::global_matrix(const NodePtr &node) {
        if (!node) return {1.f};
        return global_matrix(node->parent.lock()) * node->transform.matrix();
    }

    SkinPtr SceneBuilder::add_skeleton(std::shared_ptr<Skeleton> skeleton) {
        if (!skeleton || skeleton->bones.empty()) throw std::invalid_argument("Empty skeleton");
        auto skin = std::make_shared<SkeletonInstance>();
        skin->skeleton = std::move(skeleton);
        skin->root = create_node(skin->skeleton->name + "_Skeleton");
        for (const auto &bone: skin->skeleton->bones) {
            auto node = create_node(bone.name);
            node->transform = bone.transform;
            skin->joints.push_back(node);
        }
        for (size_t i = 0; i < skin->joints.size(); ++i) {
            const auto parent = skin->skeleton->bones[i].parent;
            if (parent < -1 || parent >= static_cast<int64_t>(skin->joints.size()))
                throw std::invalid_argument("Invalid bone parent");
            set_parent(parent < 0 ? skin->root : skin->joints[parent], skin->joints[i]);
        }
        add_to_scene(skin->root);
        scene.skeletons.push_back(skin);
        skin_stack.push_back(skin);
        return skin;
    }

    SkinPtr SceneBuilder::current_skin() const { return skin_stack.empty() ? nullptr : skin_stack.back(); }

    void SceneBuilder::pop_skin() {
        if (skin_stack.empty()) throw std::logic_error("Empty skeleton stack");
        skin_stack.pop_back();
    }

    NodePtr SceneBuilder::find_node_in_skin(const SkinPtr &skin, std::string_view name) const {
        if (skin) for (const auto &node: skin->joints) if (node->name == name) return node;
        return {};
    }

    void SceneBuilder::reset() {
        scene = {};
        skin_stack.clear();
        materials.clear();
    }

    namespace {
        using NodeHandle = GltfHelper::Handle<tinygltf::Node>;

        int component(ElementFormat format) {
            switch (format) {
                case ElementFormat::U8: return TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
                case ElementFormat::U16: return TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
                case ElementFormat::U32: return TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
                case ElementFormat::I8: return TINYGLTF_COMPONENT_TYPE_BYTE;
                case ElementFormat::I16: return TINYGLTF_COMPONENT_TYPE_SHORT;
                case ElementFormat::F32: return TINYGLTF_COMPONENT_TYPE_FLOAT;
                default: throw std::invalid_argument("Convert F16/I32 attributes before glTF serialization");
            }
        }

        size_t component_size(ElementFormat format) {
            switch (format) {
                case ElementFormat::U8:
                case ElementFormat::I8: return 1;
                case ElementFormat::U16:
                case ElementFormat::I16:
                case ElementFormat::F16: return 2;
                default: return 4;
            }
        }

        int shape(ElementType type) {
            switch (type) {
                case ElementType::Scalar: return TINYGLTF_TYPE_SCALAR;
                case ElementType::Vec2: return TINYGLTF_TYPE_VEC2;
                case ElementType::Vec3: return TINYGLTF_TYPE_VEC3;
                case ElementType::Vec4: return TINYGLTF_TYPE_VEC4;
                case ElementType::Mat3: return TINYGLTF_TYPE_MAT3;
                case ElementType::Mat4: return TINYGLTF_TYPE_MAT4;
            }
            throw std::invalid_argument("Invalid element shape");
        }

        size_t lanes(ElementType type) {
            switch (type) {
                case ElementType::Scalar: return 1;
                case ElementType::Vec2: return 2;
                case ElementType::Vec3: return 3;
                case ElementType::Vec4: return 4;
                case ElementType::Mat3: return 9;
                case ElementType::Mat4: return 16;
            }
            throw std::invalid_argument("Invalid element shape");
        }

        std::string semantic(const VertexAttribute &a) {
            switch (a.usage) {
                case ElementUsage::Position: return "POSITION";
                case ElementUsage::Normal: return "NORMAL";
                case ElementUsage::Tangent: return "TANGENT";
                case ElementUsage::Color: return "COLOR_" + std::to_string(a.set);
                case ElementUsage::TexCoord: return "TEXCOORD_" + std::to_string(a.set);
                case ElementUsage::Joints: return "JOINTS_" + std::to_string(a.set);
                case ElementUsage::Weights: return "WEIGHTS_" + std::to_string(a.set);
                case ElementUsage::Custom:
                    if (a.custom_name.empty() || a.custom_name[0] != '_')
                        throw std::invalid_argument("Custom attribute must start with underscore");
                    return a.custom_name;
            }
            throw std::invalid_argument("Invalid attribute semantic");
        }

        tinygltf::Value json_value(const nlohmann::json &j) {
            if (j.is_null()) return {};
            if (j.is_boolean()) return tinygltf::Value(j.get<bool>());
            if (j.is_number()) return tinygltf::Value(j.get<double>());
            if (j.is_string()) return tinygltf::Value(j.get<std::string>());
            if (j.is_array()) {
                tinygltf::Value::Array a;
                for (const auto &v: j) a.push_back(json_value(v));
                return tinygltf::Value(a);
            }
            tinygltf::Value::Object o;
            for (const auto &[k,v]: j.items()) o.emplace(k, json_value(v));
            return tinygltf::Value(o);
        }

        struct Serializer {
            GltfHelper &out;
            std::unordered_map<const Node *, NodeHandle> nodes;
            std::unordered_map<const Material *, int> materials;
            std::unordered_map<const TextureReference *, int> textures;
            std::unordered_map<const Mesh *, int> meshes;
            std::unordered_map<const SkeletonInstance *, int> skins;
            std::vector<std::pair<SkinPtr, std::vector<NodeHandle> > > pending_skins;
            std::vector<SkinPtr> owned_skins;
            std::unordered_set<const Node *> visiting;

            int texture(const TexturePtr &t) {
                if (!t) return -1;
                if (auto i = textures.find(t.get()); i != textures.end()) return i->second;
                auto data = t->load_png ? t->load_png() : t->png;
                if (data.empty()) return -1;
                const auto result = out.create_texture_png_data(std::move(data), t->name).index();
                textures[t.get()] = result;
                return result;
            }

            int material(const std::shared_ptr<Material> &m) {
                if (!m) return -1;
                if (auto i = materials.find(m.get()); i != materials.end()) return i->second;
                auto g = out.make<tinygltf::Material>();
                materials[m.get()] = g.index();
                g->name = m->name;
                g->pbrMetallicRoughness.baseColorFactor = {
                    m->base_color.x, m->base_color.y, m->base_color.z, m->base_color.w
                };
                g->pbrMetallicRoughness.metallicFactor = m->metallic_factor;
                g->pbrMetallicRoughness.roughnessFactor = m->roughness_factor;
                g->pbrMetallicRoughness.baseColorTexture.index = texture(m->albedo);
                g->pbrMetallicRoughness.metallicRoughnessTexture.index = texture(m->metallic_roughness);
                g->normalTexture.index = texture(m->normal);
                g->normalTexture.scale = m->normal_scale;
                g->occlusionTexture.index = texture(m->occlusion);
                g->emissiveTexture.index = texture(m->emissive);
                g->emissiveFactor = {m->emissive_factor.x, m->emissive_factor.y, m->emissive_factor.z};
                g->alphaMode = m->alpha_mode == AlphaMode::Mask
                                   ? "MASK"
                                   : m->alpha_mode == AlphaMode::Blend
                                         ? "BLEND"
                                         : "OPAQUE";
                g->alphaCutoff = m->alpha_cutoff;
                g->doubleSided = m->double_sided;
                return g.index();
            }

            int mesh(const Mesh &mesh) {
                if (auto i = meshes.find(&mesh); i != meshes.end()) return i->second;
                auto g = out.make<tinygltf::Mesh>();
                g->name = mesh.name;
                meshes[&mesh] = g.index();
                for (const auto &p: mesh.primitives) {
                    tinygltf::Primitive gp;
                    gp.mode = TINYGLTF_MODE_TRIANGLES;
                    gp.material = material(p.material);
                    if (p.attributes.empty()) throw std::invalid_argument("Primitive has no attributes");
                    const auto count = p.attributes.front().count;
                    for (const auto &a: p.attributes) {
                        const auto width = component_size(a.format) * lanes(a.type);
                        if (!count || a.count != count || a.data.size() / width != count || a.data.size() % width)
                            throw std::invalid_argument("Invalid vertex buffer size/count");
                        // Packed matrices need column padding for 8/16-bit components; reject rather than misserialize.
                        if ((a.type == ElementType::Mat3 || a.type == ElementType::Mat4) && a.format !=
                            ElementFormat::F32)
                            throw std::invalid_argument("Matrix attributes require F32");
                        auto name = semantic(a);
                        if (gp.attributes.contains(name))
                            throw std::invalid_argument("Duplicate vertex semantic");
                        const auto accessor = out.set_primitive_attribute(gp, name, a.data.data(), a.data.size(),
                                                                    component(a.format), shape(a.type), count,
                                                                    a.normalized);
                        if (a.usage == ElementUsage::Position) {
                            if (a.format != ElementFormat::F32 || a.type != ElementType::Vec3)
                                throw std::invalid_argument("Positions require F32 Vec3");
                            std::vector<double> low(3, std::numeric_limits<double>::infinity()), high(
                                3, -std::numeric_limits<double>::infinity());
                            for (size_t i = 0; i < count; ++i)
                                for (size_t c = 0; c < 3; ++c) {
                                    float v;
                                    std::memcpy(&v, a.data.data() + (i * 3 + c) * 4, 4);
                                    if (!std::isfinite(v)) throw std::invalid_argument("Non-finite position");
                                    low[c] = std::min(low[c], double(v));
                                    high[c] = std::max(high[c], double(v));
                                }
                            accessor->minValues = low;
                            accessor->maxValues = high;
                        }
                    }
                    if (p.index_count) {
                        size_t width = p.index_type == IndexType::U8 ? 1 : p.index_type == IndexType::U16 ? 2 : 4;
                        if (p.indices.size() / width != p.index_count || p.indices.size() % width)
                            throw std::invalid_argument("Invalid index buffer size");
                        for (size_t i = 0; i < p.index_count; ++i) {
                            uint32_t index = 0;
                            std::memcpy(&index, p.indices.data() + i * width, width);
                            if (index >= count) throw std::invalid_argument("Index exceeds vertex count");
                        }
                        out.set_primitive_indices(gp, p.indices.data(), p.indices.size(),
                                                  width == 1
                                                      ? TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE
                                                      : width == 2
                                                            ? TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT
                                                            : TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT, p.index_count);
                    }
                    g->primitives.push_back(std::move(gp));
                }
                return g.index();
            }

            int light(const Light &light) {
                const auto &c = light.color;
                if (!std::isfinite(c.x) || !std::isfinite(c.y) || !std::isfinite(c.z) ||
                    c.x < 0.f || c.y < 0.f || c.z < 0.f || c.x > 1.f || c.y > 1.f || c.z > 1.f)
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
                        if (light.range != 0.f) throw std::invalid_argument("Directional light cannot have a range");
                        g.type = "directional";
                        break;
                    case LightType::Point: g.type = "point";
                        break;
                    case LightType::Spot:
                        g.type = "spot";
                        g.spot.innerConeAngle = light.inner_cone_angle;
                        g.spot.outerConeAngle = light.outer_cone_angle;
                        break;
                    default: throw std::invalid_argument("Invalid light type");
                }
                g.range = light.range;
                const int index = static_cast<int>(model.lights.size());
                model.lights.push_back(std::move(g));
                out.add_extension("KHR_lights_punctual", false);
                return index;
            }

            NodeHandle node(const NodePtr &n) {
                if (!n) throw std::invalid_argument("Null scene node");
                if (visiting.contains(n.get())) throw std::invalid_argument("Scene cycle");
                if (nodes.contains(n.get()))
                    throw std::invalid_argument("Node has multiple parents; instance the model on separate nodes");
                visiting.insert(n.get());
                auto g = out.make<tinygltf::Node>();
                nodes[n.get()] = g;
                g->name = n->name;
                if (n->light) g->light = light(*n->light);
                if (n->transform.matrix_override) {
                    // Preserve affine matrices exactly, including shear; do not decompose them to TRS.
                    const auto &matrix = *n->transform.matrix_override;
                    for (int col = 0; col < 4; ++col)
                        for (int row = 0; row < 4; ++row) {
                            if (!std::isfinite(matrix[col][row])) throw std::invalid_argument("Non-finite node matrix");
                            g->matrix.push_back(matrix[col][row]);
                        }
                } else {
                    GltfHelper::set_node_transform(g, n->transform.translation, n->transform.scale,
                                                   n->transform.rotation);
                }
                if (!n->extras.is_null()) g->extras = json_value(n->extras);
                SkinPtr binding = n->skin.lock();
                if (n->model && n->model->skeleton && !binding) {
                    SceneBuilder b;
                    binding = b.add_skeleton(n->model->skeleton);
                    binding->animations = n->model->animations;
                    owned_skins.push_back(binding);
                    auto root = node(binding->root);
                    out.set_parent(g, root);
                }
                std::vector<NodeHandle> skinned_meshes;
                if (n->model)
                    for (const auto &sub: n->model->submodels) {
                        auto subnode = out.make<tinygltf::Node>();
                        subnode->name = sub.name;
                        out.set_parent(g, subnode);
                        for (const auto &m: sub.meshes) {
                            auto mn = out.make<tinygltf::Node>();
                            mn->name = m.name;
                            mn->mesh = mesh(m);
                            out.set_parent(subnode, mn);
                            if (binding) skinned_meshes.push_back(mn);
                        }
                    }
                if (binding) pending_skins.emplace_back(binding, std::move(skinned_meshes));
                for (const auto &child: n->children) {
                    auto c = node(child);
                    out.set_parent(g, c);
                }
                visiting.erase(n.get());
                return g;
            }

            void animation(const Animation &a, const SkinPtr &binding) {
                auto g = out.make<tinygltf::Animation>();
                g->name = a.name;
                for (const auto &c: a.channels) {
                    if (c.path == AnimationPath::Weights)
                        throw std::invalid_argument("Morph animation requires morph targets (not implemented)");
                    auto found = std::find_if(binding->joints.begin(), binding->joints.end(),
                                              [&](auto &j) { return j->name == c.bone; });
                    if (found == binding->joints.end())
                        throw std::invalid_argument("Animation bone not found: " + c.bone);
                    auto width = c.path == AnimationPath::Rotation ? 4u : 3u;
                    auto multiplier = c.interpolation == Interpolation::CubicSpline ? 3u : 1u;
                    if (c.times.empty() || c.values.size() != c.times.size() * width * multiplier)
                        throw std::invalid_argument("Invalid animation sample count");
                    float previous = -1.f;
                    for (float t: c.times) {
                        if (!std::isfinite(t) || t < 0 || t <= previous)
                            throw std::invalid_argument("Invalid animation times");
                        previous = t;
                    }
                    for (float v: c.values)
                        if (!std::isfinite(v))
                            throw std::invalid_argument("Non-finite animation sample");
                    auto input = out.create_accessor_chain(reinterpret_cast<const uint8_t *>(c.times.data()),
                                                           c.times.size() * 4, 0, TINYGLTF_COMPONENT_TYPE_FLOAT,
                                                           TINYGLTF_TYPE_SCALAR, c.times.size());
                    input.accessor->minValues = {c.times.front()};
                    input.accessor->maxValues = {c.times.back()};
                    auto output = out.create_accessor_chain(reinterpret_cast<const uint8_t *>(c.values.data()),
                                                            c.values.size() * 4, 0, TINYGLTF_COMPONENT_TYPE_FLOAT,
                                                            width == 4 ? TINYGLTF_TYPE_VEC4 : TINYGLTF_TYPE_VEC3,
                                                            c.values.size() / width);
                    tinygltf::AnimationSampler sampler;
                    sampler.input = input.accessor.index();
                    sampler.output = output.accessor.index();
                    sampler.interpolation = c.interpolation == Interpolation::Step
                                                ? "STEP"
                                                : c.interpolation == Interpolation::CubicSpline
                                                      ? "CUBICSPLINE"
                                                      : "LINEAR";
                    tinygltf::AnimationChannel channel;
                    channel.sampler = static_cast<int>(g->samplers.size());
                    channel.target_node = nodes.at(found->get()).index();
                    channel.target_path = c.path == AnimationPath::Translation
                                              ? "translation"
                                              : c.path == AnimationPath::Rotation
                                                    ? "rotation"
                                                    : "scale";
                    g->samplers.push_back(sampler);
                    g->channels.push_back(channel);
                }
            }

            int skin(const SkinPtr &s) {
                if (auto i = skins.find(s.get()); i != skins.end()) return i->second;
                if (!s || !s->skeleton || s->joints.size() != s->skeleton->bones.size() || s->joints.empty())
                    throw std::invalid_argument("Invalid skeleton instance");
                auto g = out.make<tinygltf::Skin>();
                skins[s.get()] = g.index();
                g->name = s->skeleton->name;
                g->skeleton = nodes.at(s->root.get()).index();
                std::vector<float> inverse;
                // Bind transforms are relative to the skeleton root, independent of scene placement.
                std::vector<glm::mat4> globals(s->joints.size());
                std::vector<int> state(s->joints.size());
                std::function<glm::mat4(size_t)> global = [&](size_t i) {
                    if (state[i] == 1) throw std::invalid_argument("Skeleton cycle");
                    if (state[i] == 2) return globals[i];
                    state[i] = 1;
                    auto parent = s->skeleton->bones[i].parent;
                    if (parent < -1 || parent >= static_cast<int64_t>(globals.size()))
                        throw std::invalid_argument("Invalid bone parent");
                    globals[i] = (parent < 0 ? glm::mat4(1.f) : global(parent)) * s->skeleton->bones[i].transform.
                                 matrix();
                    state[i] = 2;
                    return globals[i];
                };
                for (size_t i = 0; i < s->joints.size(); ++i) {
                    g->joints.push_back(nodes.at(s->joints[i].get()).index());
                    auto matrix = global(i);
                    if (std::abs(glm::determinant(matrix)) < 1e-12f)
                        throw std::invalid_argument("Singular bind transform");
                    auto inv = glm::inverse(matrix);
                    for (int col = 0; col < 4; ++col)
                        for (int row = 0; row < 4; ++row)
                            inverse.push_back(inv[col][row]);
                }
                g->inverseBindMatrices = out.create_accessor_chain(reinterpret_cast<const uint8_t *>(inverse.data()),
                                                                   inverse.size() * 4, 0, TINYGLTF_COMPONENT_TYPE_FLOAT,
                                                                   TINYGLTF_TYPE_MAT4,
                                                                   s->joints.size()).accessor.index();
                for (const auto &a: s->animations) animation(a, s);
                return g.index();
            }
        };
    }

    void to_gltf(const Scene &scene, GltfHelper &helper) {
        Serializer serializer{helper};
        for (const auto &root: scene.roots) helper.add_to_scene(serializer.node(root));
        for (const auto &s: scene.skeletons) serializer.skin(s);
        for (const auto &[s, meshes]: serializer.pending_skins) {
            auto index = serializer.skin(s);
            for (auto m: meshes) m->skin = index;
        }
        auto &model = helper.model();
        model.asset.version = "2.0";
        model.asset.generator = "RedsCore virtual model";
        if (!model.scenes.empty()) {
            model.defaultScene = 0;
            model.scenes[0].name = scene.name;
        }
    }

    bool save_gltf(const Scene &scene, const std::filesystem::path &path, bool embed_buffers) {
        GltfHelper helper;
        to_gltf(scene, helper);
        if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
        tinygltf::TinyGLTF writer;
        if (!writer.WriteGltfSceneToFile(&helper.model(), path.string(), false, embed_buffers, true, false))
            return false;
        for (const auto &extra: scene.extra_files) {
            auto destination = path.parent_path() / extra.name;
            std::ofstream file(destination, std::ios::binary);
            file.write(reinterpret_cast<const char *>(extra.data.data()), extra.data.size());
            if (!file) throw std::runtime_error("Failed to write model sidecar: " + destination.string());
        }
        return true;
    }
}
