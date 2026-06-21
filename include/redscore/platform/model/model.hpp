//
// Created by red_eye on 5/26/26.
//

#pragma once
#include <utility>
#include <vector>
#include "glm/glm.hpp"
#include "glm/gtc/quaternion.hpp"

#include "redscore/platform/vertex_buffer.hpp"
#include "redscore/platform/texture/texture.h"
class GltfHelper;

namespace ISR {
    class TextureSource {
    public:
        virtual ~TextureSource() = default;

        virtual const Texture &get() = 0;
    };

    class TextureReference : public TextureSource {
    public:
        const Texture &get() override = 0;
    };

    class TextureData : public TextureSource {
    public:
        ~TextureData() override = default;

        explicit TextureData(Texture &&m_texture)
            : m_texture(std::move(m_texture)) {
        }

        const Texture &get() override {
            return m_texture;
        }

    private:
        Texture m_texture;
    };

    class Texture {
    public:
        [[nodiscard]] const std::string &identifier() const {
            return m_identifier;
        }

        [[nodiscard]] const glm::vec2 &tiling() const {
            return m_tiling;
        }

        void set_tiling(const glm::vec2 tiling) {
            this->m_tiling = tiling;
        }

    private:
        std::string m_identifier;
        glm::vec2 m_tiling{1.f, 1.f};
    };


    enum class AlphaMode {
        OPAQUE,
        MASK,
    };

    class Material {
    public:
        explicit Material(std::string m_name)
            : m_name(std::move(m_name)) {
        }

        void set_albedo(const std::shared_ptr<TextureSource> &reference) {
            m_albedo = reference;
        }

        void set_normal(const std::shared_ptr<TextureSource> &reference) {
            m_normal = reference;
        }

        void set_mrao(const std::shared_ptr<TextureSource> &reference) {
            m_mrao = reference;
        }

        void set_alpha_mode(const AlphaMode alpha_mode) {
            this->m_alpha_mode = alpha_mode;
        }

        void set_metalic_factor(const f32 metalic_factor) {
            this->m_metalic_factor = metalic_factor;
        }

        void set_roughness_factor(const f32 roughness_factor) {
            this->m_roughness_factor = roughness_factor;
        }

        [[nodiscard]] const std::string &name() const {
            return m_name;
        }

        [[nodiscard]] const std::shared_ptr<TextureSource>& albedo() const {
            return m_albedo;
        }

        [[nodiscard]] const std::shared_ptr<TextureSource>& normal() const {
            return m_normal;
        }

        [[nodiscard]] const std::shared_ptr<TextureSource>& mrao() const {
            return m_mrao;
        }

        [[nodiscard]] f32 metalic_factor() const {
            return m_metalic_factor;
        }

        [[nodiscard]] f32 roughness_factor() const {
            return m_roughness_factor;
        }

        [[nodiscard]] AlphaMode alpha_mode() const {
            return m_alpha_mode;
        }

    private:
        std::string m_name;
        AlphaMode m_alpha_mode{AlphaMode::OPAQUE};
        std::shared_ptr<TextureSource> m_albedo;
        std::shared_ptr<TextureSource> m_normal;
        std::shared_ptr<TextureSource> m_mrao;

        f32 m_metalic_factor = 1.0f;
        f32 m_roughness_factor = 1.0f;
    };

    class Transform {
    public:
        static Transform Identity() {
            return Transform{{}, glm::identity<glm::quat>(), glm::one<glm::vec3>()};
        }

        Transform(const glm::vec3 &position, const glm::quat &rotation, const glm::vec3 &scale)
            : m_position(position),
              m_rotation(rotation),
              m_scale(scale) {
        }

        [[nodiscard]] glm::mat4 matrix() const {
            return glm::translate(glm::mat4{1.0f}, m_position)
                   * glm::mat4_cast(glm::quat(m_rotation))
                   * glm::scale(glm::mat4{1.0f}, m_scale);
        }

        [[nodiscard]] const glm::vec3 &position() const {
            return m_position;
        }

        [[nodiscard]] const glm::quat &rotation() const {
            return m_rotation;
        }

        [[nodiscard]] const glm::vec3 &scale() const {
            return m_scale;
        }

    private:
        glm::vec3 m_position;
        glm::quat m_rotation;
        glm::vec3 m_scale;
    };

    class Bone {
    public:
        Bone(const std::string &name, i32 parent_id, const Transform &transform)
            : m_name(name),
              m_parent_id(parent_id),
              m_transform(transform) {
        }

        Bone(const std::string &name, i32 parent_id, const glm::vec3 &position, const glm::quat &rotation,
             const glm::vec3 &scale)
            : m_name(name),
              m_parent_id(parent_id),
              m_transform(position, rotation, scale) {
        }

        [[nodiscard]] const std::string &name() const {
            return m_name;
        }

        [[nodiscard]] i32 parent_id() const {
            return m_parent_id;
        }

        [[nodiscard]] const Transform &transform() const {
            return m_transform;
        }

    private:
        std::string m_name;
        i32 m_parent_id{-1};
        Transform m_transform;
    };

    class Skeleton {
    public:
        Skeleton(std::string m_name, std::vector<Bone> &&m_bones)
            : m_name(std::move(m_name)),
              m_bones(std::move(m_bones)) {
        }

        [[nodiscard]] const std::string &name() const {
            return m_name;
        }

        [[nodiscard]] const std::vector<Bone> &bones() const {
            return m_bones;
        }

    private:
        std::string m_name;
        std::vector<Bone> m_bones;
    };

    enum class IndexType {
        uint8,
        uint16,
        uint32,
    };

    class Primitive {
    public:
        Primitive(std::string name, const std::shared_ptr<Material> &material_id, VertexLayout layout,
                  IO::Buffer &&vertex_buffer, const u32 vertex_count, const IndexType index_type,
                  IO::Buffer &&index_buffer,
                  const u32 index_count)
            : m_name(std::move(name)),
              m_material_id(material_id),
              m_layout(std::move(layout)),
              m_vertex_buffer(std::move(vertex_buffer)),
              m_vertex_count(vertex_count),
              m_index_type(index_type),
              m_index_buffer(std::move(index_buffer)),
              m_index_count(index_count) {
        }

        [[nodiscard]] const std::string &name() const {
            return m_name;
        }

        [[nodiscard]] const std::shared_ptr<Material> &material_id() const {
            return m_material_id;
        }

        [[nodiscard]] const VertexLayout &layout() const {
            return m_layout;
        }

        [[nodiscard]] const IO::Buffer &vertex_buffer() const {
            return m_vertex_buffer;
        }

        [[nodiscard]] IndexType index_type() const {
            return m_index_type;
        }

        [[nodiscard]] const IO::Buffer &index_buffer() const {
            return m_index_buffer;
        }

        [[nodiscard]] u32 vertex_count() const {
            return m_vertex_count;
        }

        [[nodiscard]] u32 index_count() const {
            return m_index_count;
        }

    private:
        std::string m_name;

        std::shared_ptr<Material> m_material_id{nullptr};

        VertexLayout m_layout;
        IO::Buffer m_vertex_buffer;
        u32 m_vertex_count;

        IndexType m_index_type{IndexType::uint32};
        IO::Buffer m_index_buffer;
        u32 m_index_count;
    };

    class SubModel {
    public:
        SubModel(std::string m_name)
            : m_name(std::move(m_name)) {
        }

        [[nodiscard]] const std::string &name() const {
            return m_name;
        }

        [[nodiscard]] const std::vector<Primitive> &primitives() const {
            return m_primitives;
        }

        [[nodiscard]] std::vector<Primitive> &primitives() {
            return m_primitives;
        }

    private:
        std::string m_name;
        std::vector<Primitive> m_primitives;
    };

    class Model {
    public:
        Model(std::string name, Skeleton &skeleton,
              std::vector<SubModel> &&sub_models)
            : m_name(std::move(name)),
              m_skeleton(skeleton),
              m_submodels(std::move(sub_models)) {
        }

        Model(std::string name, Skeleton &skeleton)
            : m_name(std::move(name)),
              m_skeleton(skeleton) {
        }

        Model(std::string name, std::vector<SubModel> &&sub_models)
            : m_name(std::move(name)),
              m_submodels(std::move(sub_models)) {
        }

        explicit Model(std::string name)
            : m_name(std::move(name)) {
        }

        [[nodiscard]] const std::string &name() const {
            return m_name;
        }

        [[nodiscard]] const std::optional<Skeleton> &skeleton() const {
            return m_skeleton;
        }

        [[nodiscard]] const std::vector<SubModel> &submodels() const {
            return m_submodels;
        }

        [[nodiscard]] std::vector<SubModel> &submodels() {
            return m_submodels;
        }

        void set_skeleton(Skeleton &skeleton) {
            m_skeleton = std::move(skeleton);
        }

    private:
        std::string m_name;

        std::optional<Skeleton> m_skeleton;
        std::vector<SubModel> m_submodels;
    };

    class Node {
    public:
        Node(std::string name, const Transform &transform, const std::shared_ptr<Model> &model)
            : m_name(std::move(name)),
              m_transform(transform),
              m_model(model) {
        }

        Node(std::string name, const Transform &transform)
            : m_name(std::move(name)),
              m_transform(transform) {
        }

        [[nodiscard]] const std::string &name() const {
            return m_name;
        }

        [[nodiscard]] const Transform &transform() const {
            return m_transform;
        }

        [[nodiscard]] const std::vector<Node> &children() const {
            return m_children;
        }

        [[nodiscard]] std::vector<Node> &children() {
            return m_children;
        }

        [[nodiscard]] const std::shared_ptr<Model> &model() const {
            return m_model;
        }

    private:
        std::string m_name;
        Transform m_transform;

        std::vector<Node> m_children;

        std::shared_ptr<Model> m_model{nullptr};
    };

    class Scene {
    public:
        explicit Scene(std::string m_name)
            : m_name(std::move(m_name)) {
        }

        [[nodiscard]] const std::string &name() const {
            return m_name;
        }

        [[nodiscard]] const std::vector<Node> &nodes() const {
            return m_nodes;
        }

        [[nodiscard]] const std::vector<std::shared_ptr<Model> > &models() const {
            return m_models;
        }

        [[nodiscard]] const std::vector<std::shared_ptr<Material> > &materials() const {
            return m_materials;
        }

        [[nodiscard]] std::string &name() {
            return m_name;
        }

        [[nodiscard]] std::vector<Node> &nodes() {
            return m_nodes;
        }

        [[nodiscard]] std::vector<std::shared_ptr<Model> > &models() {
            return m_models;
        }

        [[nodiscard]] std::vector<std::shared_ptr<Material> > &materials() {
            return m_materials;
        }

    private:
        std::string m_name;
        std::vector<Node> m_nodes;
        std::vector<std::shared_ptr<Model> > m_models;
        std::vector<std::shared_ptr<Material> > m_materials;
    };

    void to_gltf(const Scene &scene, GltfHelper &helper);
}
