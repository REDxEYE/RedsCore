//
// Created by red_eye on 5/26/26.
//

#include "redscore/platform/model/model.hpp"

#include "redscore/platform/gltf_helper.h"

GltfHelper::Handle<tinygltf::Skin> skeleton_to_gltf(const ISR::Skeleton &skeleton, GltfHelper &helper) {
    std::vector<glm::mat4> global_transforms(skeleton.bones().size());
    IO::Buffer inv_matrices_buffer(skeleton.bones().size() * sizeof(glm::mat4));
    const auto inv_matrices_view = inv_matrices_buffer.writable_view_as<glm::mat4>();

    const auto skin = helper.make<tinygltf::Skin>();
    skin->name = skeleton.name();
    for (const auto &[id, bone]: skeleton.bones() | std::ranges::views::enumerate) {
        auto node = helper.make<tinygltf::Node>();
        const auto &transform = bone.transform();
        const auto &position = transform.position();
        const auto &rotation = transform.rotation();
        const auto &scale = transform.scale();


        node->name = bone.name();
        node->translation = {position.x, position.y, position.z};
        node->rotation = {rotation.x, rotation.y, rotation.z, rotation.w};
        node->scale = {scale.x, scale.y, scale.z};
        skin->joints.push_back(node.index());
        auto matrix = transform.matrix();

        if (bone.parent_id() == -1) {
            helper.add_to_scene(node);
        } else {
            auto parent = helper.find_node_in_skin(skin, skeleton.bones()[bone.parent_id()].name());
            matrix = global_transforms[bone.parent_id()] * matrix;
            helper.set_parent(parent, node);
        }

        global_transforms[id] = matrix;
        inv_matrices_view[id] = glm::inverse(matrix);
    }

    const auto accessor_chain = helper.create_accessor_chain(
        inv_matrices_buffer.data(), inv_matrices_buffer.size(),
        0, TINYGLTF_COMPONENT_TYPE_FLOAT, TINYGLTF_TYPE_MAT4,
        global_transforms.size(), false, 0, 0, "INV_BIND_MATRICES");
    skin->inverseBindMatrices = accessor_chain.accessor.index();

    return skin;
}

GltfHelper::Handle<tinygltf::Mesh> submodel_to_gltf(const ISR::Scene &scene, const ISR::SubModel &submodel,
                                                    GltfHelper &helper) {
    const auto mesh = helper.make<tinygltf::Mesh>();
    mesh->name = submodel.name();
    const auto &materials = scene.materials();

    for (const auto &primitive: submodel.primitives()) {
        auto &gl_primitive = mesh->primitives.emplace_back();

        auto material_id = std::ranges::find(materials, primitive.material_id());
        if (material_id != materials.end()) {
            gl_primitive.material = static_cast<i32>(std::distance(materials.begin(), material_id));
        }

        gl_primitive.mode = TINYGLTF_MODE_TRIANGLES;
        const auto &vertex_buffer = primitive.vertex_buffer();

        const BufferRebuildInput old_vertex_format{vertex_buffer.view(), primitive.layout()};
        const auto [new_buffer, layout] = convert_buffer(old_vertex_format);

        if (!layout.gltf_valid()) {
            throw std::logic_error("Invalid GLTF layout");
        }

        auto gl_buffer = helper.create_buffer(new_buffer.as_span(), std::format("{} vertex buffer", submodel.name()));
        auto gl_buffer_view = helper.make<tinygltf::BufferView>();
        gl_buffer_view->buffer = gl_buffer.index();
        gl_buffer_view->byteStride = layout.stride;
        gl_buffer_view->byteLength = new_buffer.size();
        gl_buffer_view->target = TINYGLTF_TARGET_ARRAY_BUFFER;

        for (const auto &attribute: layout.attributes) {
            auto accessor = helper.make<tinygltf::Accessor>();
            accessor->bufferView = gl_buffer_view.index();
            accessor->byteOffset = layout.attribute_offset(attribute.usage);
            accessor->count = primitive.vertex_count();
            accessor->componentType = layout.gltf_component_type(attribute.usage);
            accessor->type = layout.gltf_type(attribute.usage);
            accessor->normalized = attribute.normalized;

            gl_primitive.attributes.emplace(attribute.gltf_name(), accessor.index());
        }

        const auto &index_buffer = primitive.index_buffer();
        auto index_gl_buffer = helper.create_buffer(index_buffer.as_span(),
                                                    std::format("{} index buffer", submodel.name()));
        auto index_gl_buffer_view = helper.make<tinygltf::BufferView>();
        index_gl_buffer_view->buffer = index_gl_buffer.index();
        index_gl_buffer_view->byteLength = index_buffer.size();
        index_gl_buffer_view->target = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;

        const auto &index_accessor = helper.make<tinygltf::Accessor>();
        index_accessor->bufferView = index_gl_buffer_view.index();
        switch (primitive.index_type()) {
            case ISR::IndexType::uint8:
                index_accessor->componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
                break;
            case ISR::IndexType::uint16:
                index_accessor->componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT;
                break;
            case ISR::IndexType::uint32:
                index_accessor->componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
                break;
        }
        index_accessor->count = primitive.index_count();
        index_accessor->type = TINYGLTF_TYPE_SCALAR;
        gl_primitive.indices = index_accessor.index();
    }

    return mesh;
}

using ModelToNodeMap = std::unordered_map<std::shared_ptr<ISR::Model>, GltfHelper::Handle<tinygltf::Node> >;

void create_node(const ISR::Node &node, GltfHelper &helper,
                 const ModelToNodeMap &model_to_node,
                 const GltfHelper::Handle<tinygltf::Node> parent) {
    const auto gl_node = helper.make<tinygltf::Node>();
    gl_node->name = node.name();
    GltfHelper::set_node_transform(gl_node, node.transform().position(), node.transform().scale(),
                                   node.transform().rotation());
    if (parent.is_valid()) {
        helper.set_parent(parent, gl_node);
    } else {
        helper.add_to_scene(gl_node);
    }
    if (node.model()) {
        const auto gl_model_node = model_to_node.at(node.model());
        helper.set_parent(gl_node, gl_model_node);
    }
    for (const auto &child: node.children()) {
        create_node(child, helper, model_to_node, gl_node);
    }
}

void material_to_gltf(const std::shared_ptr<ISR::Material> &material, GltfHelper &helper) {
    const auto gl_material = helper.make<tinygltf::Material>();
    gl_material->name = material->name();

    if (material->albedo()) {
        const auto &albedo = material->albedo();
        // const auto texture = albedo.load_texture()();
        // auto png_data = texture.save_to_memory(MemoryFormat::PNG);
        // const auto &gl_texture = helper.create_texture_png_data(std::move(png_data), albedo.identifier());
        // gl_material->pbrMetallicRoughness.baseColorTexture.index = gl_texture.index();
    }

    gl_material->pbrMetallicRoughness.metallicFactor = material->metalic_factor();
    gl_material->pbrMetallicRoughness.roughnessFactor = material->roughness_factor();
}

void ISR::to_gltf(const Scene &scene, GltfHelper &helper) {
    ModelToNodeMap model_to_node;

    for (const auto &material: scene.materials()) {
        material_to_gltf(material, helper);
    }

    for (const auto &[index, model]: scene.models() | std::ranges::views::enumerate) {
        auto model_node = helper.make<tinygltf::Node>();
        model_node->name = model->name();
        GltfHelper::Handle<tinygltf::Skin> skin{};
        if (model->skeleton()) {
            skin = skeleton_to_gltf(model->skeleton().value(), helper);
        }
        for (const auto &submodel: model->submodels()) {
            auto gl_mesh = submodel_to_gltf(scene, submodel, helper);
            auto mesh_node = helper.make<tinygltf::Node>();
            mesh_node->name = submodel.name();
            mesh_node->mesh = gl_mesh.index();
            if (skin.is_valid())
                mesh_node->skin = skin.index();
            helper.set_parent(model_node, mesh_node);
        }
        model_to_node.emplace(model, model_node);
    }


    for (const auto &node: scene.nodes()) {
        create_node(node, helper, model_to_node, {});
    }
}
