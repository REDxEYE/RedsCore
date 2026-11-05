#pragma once

#include "redscore/platform/model/data.hpp"

#include <optional>
#include <string_view>
#include <glm/gtc/quaternion.hpp>
#include <nlohmann/json.hpp>

namespace ISR {
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
    double inner_cone_angle{0.};
    double outer_cone_angle{0.7853981633974483}; // Spot half-angle in radians.
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
    // Serialized as a child subtree of this placement node. Each use creates
    // distinct glTF nodes; referenced Models keep their mesh data shared.
    NodePtr prefab;
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
    std::vector<std::shared_ptr<Material>> materials;
};
} // namespace ISR
