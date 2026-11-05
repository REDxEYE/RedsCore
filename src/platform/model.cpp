#include "redscore/platform/model/model.hpp"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>

namespace ISR {

    TexturePtr png_texture(const std::string &name, DataBuffer data) {
        return std::make_shared<TextureReference>(name, std::move(data), nullptr);
    }

    TexturePtr lazy_png_texture(const std::string &name, const std::function<DataBuffer()> &data) {
        return std::make_shared<TextureReference>(name, DataBuffer{}, data);
    }

    void Primitive::set_attribute(const VertexAttribute &attribute) {
        attributes.push_back(attribute);
    }

    void Primitive::set_indices(const void *data, size_t bytes, IndexType type, size_t count) {
        if (bytes && !data) throw std::invalid_argument("Null model buffer");
        if (bytes) {
            const IO::ConstByteBufferView view(static_cast<const uint8_t *>(data), bytes);
            indices.assign(view.begin(), view.end());
        } else {
            indices.clear();
        }
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

}
