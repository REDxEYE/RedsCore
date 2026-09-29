#pragma once

// Engine-independent scene data, ported from redscore's Rust virtual_model.
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json.hpp>

class GltfHelper;

namespace ISR {
    using DataBuffer = std::vector<uint8_t>;

    enum class ElementFormat { U8, U16, U32, I8, I16, I32, F16, F32 };

    enum class ElementType { Scalar, Vec2, Vec3, Vec4, Mat3, Mat4 };

    enum class ElementUsage { Position, Normal, Tangent, Color, TexCoord, Joints, Weights, Custom };

    enum class IndexType { U8, U16, U32 };

    struct VertexAttribute {
        ElementUsage usage{ElementUsage::Position};
        uint32_t set{0};
        std::string custom_name;
        ElementFormat format{ElementFormat::F32};
        ElementType type{ElementType::Vec3};
        bool normalized{false};
        size_t count{0};
        // Each attribute owns tightly packed data; game-specific packing is decoded upstream.
        DataBuffer data;
    };

    struct TextureReference {
        std::string name;
        DataBuffer png;
        // Optional lazy loader; captures the provider and any game-specific processing.
        std::function<DataBuffer()> load_png;
    };

    using TexturePtr = std::shared_ptr<TextureReference>;

    TexturePtr png_texture(std::string name, DataBuffer data);

    enum class AlphaMode { Opaque, Mask, Blend };

    struct Material {
        std::string name;
        TexturePtr albedo, normal, metallic_roughness, occlusion, emissive;
        glm::vec4 base_color{1.f};
        glm::vec3 emissive_factor{0.f};
        float metallic_factor{1.f}, roughness_factor{1.f}, normal_scale{1.f}, alpha_cutoff{0.5f};
        AlphaMode alpha_mode{AlphaMode::Opaque};
        bool double_sided{false};
    };

    struct Primitive {
        std::string name;
        std::shared_ptr<Material> material;
        std::vector<VertexAttribute> attributes;
        IndexType index_type{IndexType::U32};
        size_t index_count{0};
        DataBuffer indices;

        void set_attribute(const VertexAttribute& attribute);

        void set_indices(const void *data, size_t bytes, IndexType type, size_t count);
    };

    struct Mesh {
        std::string name;
        std::vector<Primitive> primitives;
    };

    struct SubModel {
        std::string name;
        std::vector<Mesh> meshes;
    };

    struct Transform {
        glm::vec3 translation{0.f};
        glm::quat rotation{1.f, 0.f, 0.f, 0.f};
        glm::vec3 scale{1.f};
        std::optional<glm::mat4> matrix_override;

        [[nodiscard]] glm::mat4 matrix() const;
    };

    struct Bone {
        std::string name;
        int32_t parent{-1};
        Transform transform;
    };

    struct Skeleton {
        std::string name;
        std::vector<Bone> bones;
    };

    enum class Interpolation { Linear, Step, CubicSpline };

    enum class AnimationPath { Translation, Rotation, Scale, Weights };

    struct Channel {
        std::string bone;
        AnimationPath path{AnimationPath::Translation};
        Interpolation interpolation{Interpolation::Linear};
        // Seconds preserve non-integral frame timing without resampling.
        std::vector<float> times;
        std::vector<float> values; // XYZ or XYZW; cubic spline uses in/value/out triples.
    };

    struct Animation {
        std::string name;
        std::vector<Channel> channels;
    };

    struct Model {
        std::string name;
        std::vector<SubModel> submodels;
        std::shared_ptr<Skeleton> skeleton;
        std::vector<Animation> animations;
    };

    struct Node;
    using NodePtr = std::shared_ptr<Node>;

    struct SkeletonInstance {
        std::shared_ptr<Skeleton> skeleton;
        NodePtr root;
        std::vector<NodePtr> joints;
        std::vector<Animation> animations;
    };

    using SkinPtr = std::shared_ptr<SkeletonInstance>;

    enum class LightType { Directional, Point, Spot };

    struct Light {
        std::string name;
        LightType type{LightType::Point};
        glm::vec3 color{1.f};
        float intensity{1.f};
        float range{0.f}; // Zero means unbounded.
        float inner_cone_angle{0.f};
        float outer_cone_angle{0.7853981633974483f}; // Spot half-angle in radians.
    };

    struct Node {
        std::string name;
        Transform transform;
        nlohmann::json extras;
        std::optional<Light> light;
        std::vector<NodePtr> children;
        std::weak_ptr<Node> parent;
        std::shared_ptr<Model> model;
        std::weak_ptr<SkeletonInstance> skin;
    };

    struct ExtraFile {
        std::string name;
        DataBuffer data;
    };

    struct Scene {
        std::string name;
        std::vector<NodePtr> roots;
        std::vector<SkinPtr> skeletons;
        std::vector<ExtraFile> extra_files;
    };

    // Construction context: nodes are stable and skins/materials are local to one export.
    class SceneBuilder {
    public:
        Scene scene;

        NodePtr create_node(std::string name = {});

        std::shared_ptr<Material> material(std::string_view name);

        std::shared_ptr<Material> find_material(std::string_view name) const;

        SkinPtr add_skeleton(std::shared_ptr<Skeleton> skeleton);

        SkinPtr current_skin() const;

        void pop_skin();

        NodePtr find_node_in_skin(const SkinPtr &skin, std::string_view name) const;

        void add_to_scene(const NodePtr &node);

        void set_parent(const NodePtr &parent, const NodePtr &child);

        static glm::mat4 global_matrix(const NodePtr &node);

        void reset();

    private:
        std::vector<SkinPtr> skin_stack;
        std::vector<std::shared_ptr<Material> > materials;
    };

    // Only this boundary knows about glTF. App adapters consume the neutral types above.
    void to_gltf(const Scene &scene, GltfHelper &helper);

    bool save_gltf(const Scene &scene, const std::filesystem::path &path, bool embed_buffers = true);
}

namespace VM = ISR;
