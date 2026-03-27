#include "assets/fbx_loader.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "animation_ufbx.h"
#include "assets/texture_loader.h"

namespace
{
std::string MaterialName(const ufbx_material* material)
{
    if (material == nullptr || material->name.data == nullptr)
    {
        return "default";
    }
    return std::string(material->name.data, material->name.length);
}

Vec3 Vec3FromUfbx(const ufbx_vec3& v)
{
    return Vec3Make(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z));
}

Vec2 Vec2FromUfbxUv(const ufbx_vec2& v)
{
    return Vec2Make(static_cast<float>(v.x), 1.0f - static_cast<float>(v.y));
}

Vec3 TransformPoint(const ufbx_matrix& m, Vec3 p)
{
    return Vec3Make(
        static_cast<float>(m.m00 * p.x + m.m01 * p.y + m.m02 * p.z + m.m03),
        static_cast<float>(m.m10 * p.x + m.m11 * p.y + m.m12 * p.z + m.m13),
        static_cast<float>(m.m20 * p.x + m.m21 * p.y + m.m22 * p.z + m.m23)
    );
}

Vec3 TransformDirection(const ufbx_matrix& m, Vec3 v)
{
    return Vec3Normalize(Vec3Make(
        static_cast<float>(m.m00 * v.x + m.m01 * v.y + m.m02 * v.z),
        static_cast<float>(m.m10 * v.x + m.m11 * v.y + m.m12 * v.z),
        static_cast<float>(m.m20 * v.x + m.m21 * v.y + m.m22 * v.z)
    ));
}

TextureData LoadFbxMaterialTexture(
    const std::filesystem::path& modelDirectory,
    const ufbx_material* material
)
{
    if (material != nullptr)
    {
        const ufbx_texture* texture = material->pbr.base_color.texture;
        if (texture != nullptr && texture->filename.data != nullptr)
        {
            std::filesystem::path texturePath =
                modelDirectory / std::string(texture->filename.data, texture->filename.length);
            if (std::filesystem::exists(texturePath))
            {
                return LoadTexture(texturePath.string());
            }
        }

        const ufbx_vec4 color = material->pbr.base_color.value_vec4;
        return MakeSolidTexture(
            static_cast<std::uint8_t>(std::clamp(color.x, 0.0, 1.0) * 255.0 + 0.5),
            static_cast<std::uint8_t>(std::clamp(color.y, 0.0, 1.0) * 255.0 + 0.5),
            static_cast<std::uint8_t>(std::clamp(color.z, 0.0, 1.0) * 255.0 + 0.5),
            static_cast<std::uint8_t>(std::clamp(color.w, 0.0, 1.0) * 255.0 + 0.5)
        );
    }

    return MakeSolidTexture(255, 255, 255, 255);
}

std::uint32_t EnsureMaterial(
    ModelData& model,
    const std::filesystem::path& modelDirectory,
    const ufbx_material* material,
    std::unordered_map<const ufbx_material*, std::uint32_t>& materialMap
)
{
    auto found = materialMap.find(material);
    if (found != materialMap.end())
    {
        return found->second;
    }

    MaterialData outMaterial{};
    outMaterial.name = MaterialName(material);
    outMaterial.textureIndex = static_cast<std::int32_t>(model.textures.size());
    model.textures.push_back(LoadFbxMaterialTexture(modelDirectory, material));

    std::uint32_t materialIndex = static_cast<std::uint32_t>(model.materials.size());
    model.materials.push_back(outMaterial);
    materialMap.emplace(material, materialIndex);
    return materialIndex;
}

void AppendMesh(
    ModelData& model,
    const std::filesystem::path& modelDirectory,
    const ufbx_mesh& mesh,
    const ufbx_node& node,
    std::unordered_map<const ufbx_material*, std::uint32_t>& materialMap
)
{
    std::vector<std::uint32_t> triIndices(mesh.max_face_triangles * 3);
    const ufbx_matrix geometryToWorld = node.geometry_to_world;
    auto appendVertex = [&](std::uint32_t wedgeIndex) {
        Vertex vertex{};

        if (mesh.vertex_position.exists && wedgeIndex < mesh.vertex_position.indices.count)
        {
            const std::uint32_t valueIndex = mesh.vertex_position.indices.data[wedgeIndex];
            if (valueIndex < mesh.vertex_position.values.count)
            {
                vertex.position = TransformPoint(
                    geometryToWorld,
                    Vec3FromUfbx(mesh.vertex_position.values.data[valueIndex])
                );
            }
        }

        if (mesh.vertex_normal.exists && wedgeIndex < mesh.vertex_normal.indices.count)
        {
            const std::uint32_t valueIndex = mesh.vertex_normal.indices.data[wedgeIndex];
            if (valueIndex < mesh.vertex_normal.values.count)
            {
                vertex.normal = TransformDirection(
                    geometryToWorld,
                    Vec3FromUfbx(mesh.vertex_normal.values.data[valueIndex])
                );
            }
        }
        else
        {
            vertex.normal = Vec3Make(0.0f, 1.0f, 0.0f);
        }

        if (mesh.vertex_uv.exists && wedgeIndex < mesh.vertex_uv.indices.count)
        {
            const std::uint32_t valueIndex = mesh.vertex_uv.indices.data[wedgeIndex];
            if (valueIndex < mesh.vertex_uv.values.count)
            {
                vertex.uv = Vec2FromUfbxUv(mesh.vertex_uv.values.data[valueIndex]);
            }
        }

        model.mesh.indices.push_back(static_cast<std::uint32_t>(model.mesh.vertices.size()));
        model.mesh.vertices.push_back(vertex);
    };

    auto appendFaceRange = [&](std::size_t faceBegin, std::size_t faceEnd, const ufbx_material* material) {
        const std::uint32_t materialIndex = EnsureMaterial(model, modelDirectory, material, materialMap);
        PrimitiveData primitive{};
        primitive.firstIndex = static_cast<std::uint32_t>(model.mesh.indices.size());
        primitive.materialIndex = materialIndex;

        for (std::size_t faceIndex = faceBegin; faceIndex < faceEnd; ++faceIndex)
        {
            const ufbx_face face = mesh.faces.data[faceIndex];
            if (face.num_indices < 3)
            {
                continue;
            }

            const std::uint32_t triCount =
                ufbx_triangulate_face(triIndices.data(), triIndices.size(), &mesh, face);
            for (std::uint32_t tri = 0; tri < triCount; ++tri)
            {
                appendVertex(triIndices[tri * 3 + 0]);
                appendVertex(triIndices[tri * 3 + 2]);
                appendVertex(triIndices[tri * 3 + 1]);
            }
        }

        primitive.indexCount = static_cast<std::uint32_t>(model.mesh.indices.size()) - primitive.firstIndex;
        if (primitive.indexCount > 0)
        {
            model.primitives.push_back(primitive);
        }
    };

    if (mesh.material_parts.count > 0)
    {
        for (std::size_t partIndex = 0; partIndex < mesh.material_parts.count; ++partIndex)
        {
            const ufbx_mesh_part& part = mesh.material_parts.data[partIndex];
            const ufbx_material* material = nullptr;
            if (part.index < node.materials.count)
            {
                material = node.materials.data[part.index];
            }
            else if (part.index < mesh.materials.count)
            {
                material = mesh.materials.data[part.index];
            }
            appendFaceRange(
                part.face_indices.data[0],
                part.face_indices.data[part.num_faces - 1] + 1,
                material
            );
        }
        return;
    }

    appendFaceRange(0, mesh.faces.count, nullptr);
}
}

ModelData LoadFbxModel(std::string_view path)
{
    ufbx_load_opts opts{};
    opts.target_axes.right = UFBX_COORDINATE_AXIS_POSITIVE_X;
    opts.target_axes.up = UFBX_COORDINATE_AXIS_POSITIVE_Y;
    opts.target_axes.front = UFBX_COORDINATE_AXIS_NEGATIVE_Z;
    opts.target_unit_meters = 1.0f;

    ufbx_error error{};
    ufbx_scene* scene = ufbx_load_file(std::string(path).c_str(), &opts, &error);
    if (scene == nullptr)
    {
        throw std::runtime_error("ufbx_load_file failed: " + std::string(path));
    }

    ModelData model{};
    std::filesystem::path modelPath(path);
    std::filesystem::path modelDirectory = modelPath.parent_path();
    std::unordered_map<const ufbx_material*, std::uint32_t> materialMap{};

    for (std::size_t meshIndex = 0; meshIndex < scene->meshes.count; ++meshIndex)
    {
        const ufbx_mesh* mesh = scene->meshes.data[meshIndex];
        if (mesh == nullptr || mesh->instances.count == 0)
        {
            continue;
        }

        for (std::size_t instanceIndex = 0; instanceIndex < mesh->instances.count; ++instanceIndex)
        {
            const ufbx_node* node = mesh->instances.data[instanceIndex];
            if (node == nullptr)
            {
                continue;
            }
            AppendMesh(model, modelDirectory, *mesh, *node, materialMap);
        }
    }

    if (model.materials.empty())
    {
        model.materials.push_back(MaterialData{
            .name = "default",
            .textureIndex = 0,
        });
        model.textures.push_back(MakeSolidTexture(255, 255, 255, 255));
    }

    ufbx_free_scene(scene);
    return model;
}
