#include "serializer.hpp"
#include <fstream>

namespace ISR::gltf_writer {
int Serializer::texture(const TexturePtr &t) {
    if (!t)
        return -1;
    if (auto i = textures.find(t.get()); i != textures.end())
        return i->second;
    DataBuffer loaded;
    const DataBuffer *data = &t->png;
    if (t->load_png) {
        loaded = t->load_png();
        data = &loaded;
    }
    if (data->empty()) {
        textures.emplace(t.get(), -1);
        return -1;
    }
    int result;
    if (texture_folder) {
        const auto filename = std::to_string(out.model().images.size()) + ".png";
        const auto destination = *texture_folder / filename;
        std::ofstream file(destination, std::ios::binary);
        file.write(reinterpret_cast<const char *>(data->data()), data->size());
        if (!file)
            throw std::runtime_error("Failed to write model texture: " + destination.string());
        auto image = out.make<tinygltf::Image>();
        image->name = t->name;
        image->uri = texture_folder->filename().generic_string() + "/" + filename;
        auto texture = out.make<tinygltf::Texture>();
        texture->name = t->name;
        texture->source = image.index();
        result = texture.index();
    } else {
        DataBuffer png = t->load_png ? std::move(loaded) : *data;
        result = out.create_texture_png_data(std::move(png), t->name).index();
    }
    textures[t.get()] = result;
    return result;
}

int Serializer::material(const std::shared_ptr<Material> &m) {
    if (!m)
        return -1;
    if (auto i = materials.find(m.get()); i != materials.end())
        return i->second;
    auto g = out.make<tinygltf::Material>();
    materials[m.get()] = g.index();
    g->name = m->name;
    g->pbrMetallicRoughness.baseColorFactor = {m->base_color.x, m->base_color.y, m->base_color.z, m->base_color.w};
    g->pbrMetallicRoughness.metallicFactor = m->metallic_factor;
    g->pbrMetallicRoughness.roughnessFactor = m->roughness_factor;
    g->pbrMetallicRoughness.baseColorTexture.index = texture(m->albedo);
    g->pbrMetallicRoughness.metallicRoughnessTexture.index = texture(m->metallic_roughness);
    if (m->specularUsed) {
        const int specular_index = texture(m->specular);
        const int color_index = m->specularColor == m->specular ? specular_index : texture(m->specularColor);
        const bool factor_override = m->specularFactor != 1.f || m->specularColorFactor != glm::vec3{1.f};
        if (specular_index >= 0 || color_index >= 0 || factor_override) {
            out.add_extension("KHR_materials_specular", true);
            auto spec_ext = tinygltf::Value::Object();
            if (specular_index >= 0) {
                auto info = tinygltf::Value::Object();
                info["index"] = tinygltf::Value(specular_index);
                info["texCoord"] = tinygltf::Value(0);
                spec_ext["specularTexture"] = tinygltf::Value(info);
            }
            if (color_index >= 0) {
                auto info = tinygltf::Value::Object();
                info["index"] = tinygltf::Value(color_index);
                info["texCoord"] = tinygltf::Value(0);
                spec_ext["specularColorTexture"] = tinygltf::Value(info);
            }
            if (m->specularFactor != 1.f)
                spec_ext["specularFactor"] = tinygltf::Value(m->specularFactor);
            if (m->specularColorFactor != glm::vec3{1.f}) {
                const auto &factor = m->specularColorFactor;
                spec_ext["specularColorFactor"] = tinygltf::Value(
                    {tinygltf::Value(factor[0]), tinygltf::Value(factor[1]), tinygltf::Value(factor[2])});
            }
            g->extensions["KHR_materials_specular"] = tinygltf::Value(spec_ext);
        }
    }
    g->normalTexture.index = texture(m->normal);
    g->normalTexture.scale = m->normal_scale;
    g->occlusionTexture.index = texture(m->occlusion);
    g->emissiveTexture.index = texture(m->emissive);
    g->emissiveFactor = {m->emissive_factor.x, m->emissive_factor.y, m->emissive_factor.z};
    g->alphaMode = m->alpha_mode == AlphaMode::Mask ? "MASK" : m->alpha_mode == AlphaMode::Blend ? "BLEND" : "OPAQUE";
    g->alphaCutoff = m->alpha_cutoff;
    g->doubleSided = m->double_sided;
    return g.index();
}
} // namespace ISR::gltf_writer
