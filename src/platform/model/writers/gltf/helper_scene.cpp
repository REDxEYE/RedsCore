// Created by RED on 08.03.2026.

#include "redscore/platform/gltf_helper.h"

#include <algorithm>
#include <ranges>

#include "glm/gtx/matrix_decompose.hpp"

GltfHelper::GltfHelper() {
    m_model.asset.version = "2.0";
    m_model.asset.generator = "ApexPredator tinygltf helper";
    m_model.scenes.emplace_back();
    m_model.defaultScene = 0;
}

tinygltf::Model &GltfHelper::model() { return m_model; }

const tinygltf::Model &GltfHelper::model() const { return m_model; }

GltfHelper::Handle<tinygltf::Mesh> GltfHelper::create_mesh(const std::string &name, const int mode) {
    const auto mesh = make<tinygltf::Mesh>();
    mesh->name = name;
    mesh->primitives.emplace_back();
    mesh->primitives.back().mode = mode;
    return mesh;
}

GltfHelper::Handle<tinygltf::Skin> GltfHelper::current_skin() const {
    if (m_skin_stack.empty()) {
        return {};
    }
    return m_skin_stack.back();
}

void GltfHelper::push_skin(const Handle<tinygltf::Skin> skin) { m_skin_stack.push_back(skin); }

void GltfHelper::pop_skin() { m_skin_stack.pop_back(); }

void GltfHelper::reset() { m_model = tinygltf::Model(); }

void GltfHelper::add_extension(const std::string &name, const bool required) {
    if (std::ranges::contains(m_model.extensionsUsed, name)) {
        return;
    }

    m_model.extensionsUsed.push_back(name);
    if (required) {
        m_model.extensionsRequired.push_back(name);
    }
}

bool is_all_zero(const glm::vec3 &v) { return glm::length(v) <= glm::epsilon<float>(); }

bool is_all_one(const glm::vec3 &v) { return glm::length(v - glm::vec3(1.0f)) <= glm::epsilon<float>(); }

bool is_identity_quat(const glm::quat &q) {
    constexpr auto eps = glm::epsilon<float>();
    constexpr auto id = glm::identity<glm::quat>();
    return glm::length(q - id) <= eps || glm::length(q + id) <= eps;
}

void GltfHelper::set_node_matrix(const Handle<tinygltf::Node> &node, const glm::mat4 &mat) {
    glm::vec3 translation, scale, skew;
    glm::vec4 perspective;
    glm::quat rotation;
    glm::decompose(mat, scale, rotation, translation, skew, perspective);

    if (glm::any(glm::isnan(translation))) {
        throw std::runtime_error("Translation is NaN");
    }
    if (glm::any(glm::isinf(translation))) {
        throw std::runtime_error("Translation is Inf");
    }
    if (glm::any(glm::isnan(scale))) {
        throw std::runtime_error("Scale is NaN");
    }
    if (glm::any(glm::isinf(scale))) {
        throw std::runtime_error("Scale is Inf");
    }
    if (glm::any(glm::isnan(rotation))) {
        throw std::runtime_error("Rotation is NaN");
    }
    if (glm::any(glm::isinf(rotation))) {
        throw std::runtime_error("Rotation is Inf");
    }

    if (!is_all_zero(translation)) {
        node->translation = {translation.x, translation.y, translation.z};
    }
    if (glm::abs(glm::length(rotation - glm::identity<glm::quat>())) > glm::epsilon<float>()) {
        node->rotation = {rotation.x, rotation.y, rotation.z, rotation.w};
    }
    if (!is_all_one(scale)) {
        node->scale = {scale.x, scale.y, scale.z};
    }
}

void GltfHelper::set_node_transform(const Handle<tinygltf::Node> &node, const glm::vec3 &translation,
                                    const glm::vec3 &scale, const glm::quat &rotation) {
    if (glm::any(glm::isnan(translation))) {
        throw std::runtime_error("Translation is NaN");
    }
    if (glm::any(glm::isinf(translation))) {
        throw std::runtime_error("Translation is Inf");
    }
    if (glm::any(glm::isnan(scale))) {
        throw std::runtime_error("Scale is NaN");
    }
    if (glm::any(glm::isinf(scale))) {
        throw std::runtime_error("Scale is Inf");
    }
    if (glm::any(glm::isnan(rotation))) {
        throw std::runtime_error("Rotation is NaN");
    }
    if (glm::any(glm::isinf(rotation))) {
        throw std::runtime_error("Rotation is Inf");
    }

    if (!is_all_zero(translation)) {
        node->translation = {translation.x, translation.y, translation.z};
    }
    if (!is_identity_quat(rotation)) {
        node->rotation = {rotation.x, rotation.y, rotation.z, rotation.w};
    }
    if (!is_all_one(scale)) {
        node->scale = {scale.x, scale.y, scale.z};
    }
}

void GltfHelper::add_extra_save_data(const std::string &name, const std::vector<uint8> &&data) {
    m_extra_save_data.emplace_back(name, data);
}

void GltfHelper::add_to_scene(const Handle<tinygltf::Node> &node) {
    if (node.index() < 0 || static_cast<size_t>(node.index()) >= m_model.nodes.size()) {
        throw std::runtime_error("Node id is out of range");
    }
    if (m_model.scenes.empty()) {
        m_model.scenes.emplace_back();
    }

    if (std::ranges::contains(m_model.scenes[0].nodes, node.index())) {
        return;
    }

    m_model.scenes[0].nodes.push_back(node.index());
}

GltfHelper::Handle<tinygltf::Node> GltfHelper::get_parent(const Handle<tinygltf::Node> &child) {
    const auto child_index = child.index();
    for (const auto [index, value] : m_model.nodes | std::views::enumerate) {
        if (std::ranges::contains(value.children, child_index)) {
            return {&m_model.nodes, static_cast<int32>(index)};
        }
    }
    return {};
}

void GltfHelper::set_parent(const Handle<tinygltf::Node> &parent, const Handle<tinygltf::Node> &children) {
    if (children.index() < 0 || static_cast<size_t>(children.index()) >= m_model.nodes.size()) {
        throw std::runtime_error("Node id is out of range");
    }
    if (parent.index() < 0 || static_cast<size_t>(parent.index()) >= m_model.nodes.size()) {
        throw std::runtime_error("Parent id is out of range");
    }

    if (children == parent) {
        GLog_Error("Cannot set node to itself as parent");
        return;
    }

    if (std::ranges::contains(m_model.nodes[parent.index()].children, children.index())) {
        GLog_Warning("Node() is already a child of parent()", children.index(), parent.index());
        return;
    }

    m_model.nodes[parent.index()].children.push_back(children.index());
}

GltfHelper::Handle<tinygltf::Node> GltfHelper::find_node_in_skin(Handle<tinygltf::Skin> skin, std::string_view name) {
    for (const auto node_id : skin->joints) {
        const auto &node = model().nodes[node_id];
        if (node.name == name) {
            return {&model().nodes, node_id};
        }
    }
    return {};
}
