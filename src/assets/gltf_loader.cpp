#include "assets/gltf_loader.h"

#include <SDL3/SDL_log.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#define CGLTF_IMPLEMENTATION
#include "assets/cgltf.h"

#include "assets/texture_loader.h"

namespace
{
Mat4 Mat4FromArray(const float* values)
{
    Mat4 result{};
    for (int i = 0; i < 16; ++i)
    {
        result.m[i] = values[i];
    }
    return result;
}

Vec3 ReadVec3(const cgltf_accessor* accessor, cgltf_size index, Vec3 fallback)
{
    if (accessor == nullptr)
    {
        return fallback;
    }

    std::array<float, 4> values{};
    cgltf_accessor_read_float(accessor, index, values.data(), 3);
    return Vec3Make(values[0], values[1], values[2]);
}

Vec2 ReadVec2(const cgltf_accessor* accessor, cgltf_size index, Vec2 fallback)
{
    if (accessor == nullptr)
    {
        return fallback;
    }

    std::array<float, 4> values{};
    cgltf_accessor_read_float(accessor, index, values.data(), 2);
    return Vec2Make(values[0], 1.0f - values[1]);
}

TextureData LoadGltfMaterialTexture(
    const std::filesystem::path& sceneDirectory,
    const cgltf_material* material
)
{
    if (material != nullptr && material->has_pbr_metallic_roughness)
    {
        const cgltf_texture* texture =
            material->pbr_metallic_roughness.base_color_texture.texture;
        if (texture != nullptr && texture->image != nullptr && texture->image->uri != nullptr)
        {
            std::filesystem::path imagePath = sceneDirectory / texture->image->uri;
            if (std::filesystem::exists(imagePath))
            {
                return LoadTexture(imagePath.string());
            }
        }
    }

    auto clampColor = [](float v) -> std::uint8_t {
        v = std::clamp(v, 0.0f, 1.0f);
        return static_cast<std::uint8_t>(v * 255.0f + 0.5f);
    };

    if (material != nullptr && material->has_pbr_metallic_roughness)
    {
        const float* factor = material->pbr_metallic_roughness.base_color_factor;
        return MakeSolidTexture(
            clampColor(factor[0]),
            clampColor(factor[1]),
            clampColor(factor[2]),
            clampColor(factor[3])
        );
    }

    return MakeSolidTexture(255, 255, 255, 255);
}

std::uint32_t EnsureMaterial(
    ModelData& model,
    const std::filesystem::path& sceneDirectory,
    const cgltf_material* material,
    std::unordered_map<const cgltf_material*, std::uint32_t>& materialMap
)
{
    auto found = materialMap.find(material);
    if (found != materialMap.end())
    {
        return found->second;
    }

    MaterialData outMaterial{};
    outMaterial.name = material != nullptr && material->name != nullptr ? material->name : "default";
    outMaterial.textureIndex = static_cast<std::int32_t>(model.textures.size());
    model.textures.push_back(LoadGltfMaterialTexture(sceneDirectory, material));

    std::uint32_t materialIndex = static_cast<std::uint32_t>(model.materials.size());
    model.materials.push_back(outMaterial);
    materialMap.emplace(material, materialIndex);
    return materialIndex;
}

ModelData BuildModelFromMesh(const std::filesystem::path& sceneDirectory, const cgltf_mesh& mesh)
{
    ModelData model{};
    std::unordered_map<const cgltf_material*, std::uint32_t> materialMap{};

    for (cgltf_size primitiveIndex = 0; primitiveIndex < mesh.primitives_count; ++primitiveIndex)
    {
        const cgltf_primitive& primitive = mesh.primitives[primitiveIndex];
        if (primitive.type != cgltf_primitive_type_triangles)
        {
            continue;
        }

        const cgltf_accessor* positions = nullptr;
        const cgltf_accessor* normals = nullptr;
        const cgltf_accessor* uvs = nullptr;
        for (cgltf_size attributeIndex = 0; attributeIndex < primitive.attributes_count; ++attributeIndex)
        {
            const cgltf_attribute& attribute = primitive.attributes[attributeIndex];
            switch (attribute.type)
            {
            case cgltf_attribute_type_position:
                positions = attribute.data;
                break;
            case cgltf_attribute_type_normal:
                normals = attribute.data;
                break;
            case cgltf_attribute_type_texcoord:
                if (attribute.index == 0)
                {
                    uvs = attribute.data;
                }
                break;
            default:
                break;
            }
        }

        if (positions == nullptr)
        {
            continue;
        }

        PrimitiveData outPrimitive{};
        outPrimitive.firstIndex = static_cast<std::uint32_t>(model.mesh.indices.size());
        outPrimitive.materialIndex = EnsureMaterial(model, sceneDirectory, primitive.material, materialMap);

        cgltf_size elementCount = primitive.indices != nullptr ? primitive.indices->count : positions->count;
        model.mesh.vertices.reserve(model.mesh.vertices.size() + elementCount);
        model.mesh.indices.reserve(model.mesh.indices.size() + elementCount);

        for (cgltf_size i = 0; i < elementCount; ++i)
        {
            cgltf_size vertexIndex = primitive.indices != nullptr
                ? cgltf_accessor_read_index(primitive.indices, i)
                : i;

            Vertex vertex{};
            vertex.position = ReadVec3(positions, vertexIndex, Vec3Make(0.0f, 0.0f, 0.0f));
            vertex.normal = Vec3Normalize(ReadVec3(normals, vertexIndex, Vec3Make(0.0f, 1.0f, 0.0f)));
            vertex.uv = ReadVec2(uvs, vertexIndex, Vec2Make(0.0f, 0.0f));

            std::uint32_t dstIndex = static_cast<std::uint32_t>(model.mesh.vertices.size());
            model.mesh.vertices.push_back(vertex);
            model.mesh.indices.push_back(dstIndex);
        }

        outPrimitive.indexCount = static_cast<std::uint32_t>(elementCount);
        model.primitives.push_back(outPrimitive);
    }

    if (model.materials.empty())
    {
        MaterialData material{};
        material.name = "default";
        material.textureIndex = 0;
        model.materials.push_back(material);
        model.textures.push_back(MakeSolidTexture(255, 255, 255, 255));
    }

    return model;
}

void GatherNodeEntities(
    SceneData& scene,
    const cgltf_data* data,
    const std::vector<std::uint32_t>& meshToModel,
    const cgltf_node* node
)
{
    if (node == nullptr)
    {
        return;
    }

    if (node->mesh != nullptr)
    {
        std::ptrdiff_t meshIndex = node->mesh - data->meshes;
        if (meshIndex >= 0 && static_cast<std::size_t>(meshIndex) < meshToModel.size())
        {
            std::array<float, 16> transform{};
            cgltf_node_transform_world(node, transform.data());

            EntityData entity{};
            entity.modelIndex = meshToModel[static_cast<std::size_t>(meshIndex)];
            entity.transform = Mat4FromArray(transform.data());
            scene.entities.push_back(entity);
        }
    }

    for (cgltf_size childIndex = 0; childIndex < node->children_count; ++childIndex)
    {
        GatherNodeEntities(scene, data, meshToModel, node->children[childIndex]);
    }
}
}

SceneData LoadGltfScene(std::string_view path)
{
    std::filesystem::path scenePath(path);
    std::filesystem::path sceneDirectory = scenePath.parent_path();

    cgltf_options options{};
    cgltf_data* data = nullptr;
    cgltf_result result = cgltf_parse_file(&options, std::string(path).c_str(), &data);
    if (result != cgltf_result_success || data == nullptr)
    {
        throw std::runtime_error("Failed to parse glTF: " + std::string(path));
    }

    result = cgltf_load_buffers(&options, data, std::string(path).c_str());
    if (result != cgltf_result_success)
    {
        cgltf_free(data);
        throw std::runtime_error("Failed to load glTF buffers: " + std::string(path));
    }

    if (data->scene == nullptr && data->scenes_count > 0)
    {
        data->scene = &data->scenes[0];
    }

    SceneData scene{};
    std::vector<std::uint32_t> meshToModel(data->meshes_count, 0);
    scene.models.reserve(data->meshes_count);

    for (cgltf_size meshIndex = 0; meshIndex < data->meshes_count; ++meshIndex)
    {
        meshToModel[meshIndex] = static_cast<std::uint32_t>(scene.models.size());
        scene.models.push_back(BuildModelFromMesh(sceneDirectory, data->meshes[meshIndex]));
    }

    if (data->scene != nullptr)
    {
        for (cgltf_size nodeIndex = 0; nodeIndex < data->scene->nodes_count; ++nodeIndex)
        {
            GatherNodeEntities(scene, data, meshToModel, data->scene->nodes[nodeIndex]);
        }
    }

    if (scene.entities.empty() && !scene.models.empty())
    {
        SDL_Log("glTF scene had no instanced nodes; creating one identity entity");
        EntityData entity{};
        entity.modelIndex = 0;
        entity.transform = Mat4Identity();
        scene.entities.push_back(entity);
    }

    cgltf_free(data);
    return scene;
}
