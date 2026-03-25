#include "assets/obj_loader.h"

#include <SDL3/SDL_log.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include "assets/texture_loader.h"

namespace
{
struct VertexKey
{
    int vertexIndex = -1;
    int normalIndex = -1;
    int texcoordIndex = -1;

    bool operator==(const VertexKey& other) const
    {
        return vertexIndex == other.vertexIndex &&
               normalIndex == other.normalIndex &&
               texcoordIndex == other.texcoordIndex;
    }
};

struct VertexKeyHasher
{
    std::size_t operator()(const VertexKey& key) const
    {
        std::size_t h0 = std::hash<int>{}(key.vertexIndex);
        std::size_t h1 = std::hash<int>{}(key.normalIndex);
        std::size_t h2 = std::hash<int>{}(key.texcoordIndex);
        return h0 ^ (h1 << 1) ^ (h2 << 2);
    }
};

Vec3 ReadPosition(const tinyobj::attrib_t& attrib, int index)
{
    return Vec3Make(
        attrib.vertices[static_cast<std::size_t>(index) * 3 + 0],
        attrib.vertices[static_cast<std::size_t>(index) * 3 + 1],
        attrib.vertices[static_cast<std::size_t>(index) * 3 + 2]
    );
}

Vec3 ReadNormal(const tinyobj::attrib_t& attrib, int index)
{
    return Vec3Normalize(
        Vec3Make(
            attrib.normals[static_cast<std::size_t>(index) * 3 + 0],
            attrib.normals[static_cast<std::size_t>(index) * 3 + 1],
            attrib.normals[static_cast<std::size_t>(index) * 3 + 2]
        )
    );
}

Vec2 ReadUv(const tinyobj::attrib_t& attrib, int index)
{
    return Vec2Make(
        attrib.texcoords[static_cast<std::size_t>(index) * 2 + 0],
        1.0f - attrib.texcoords[static_cast<std::size_t>(index) * 2 + 1]
    );
}

void GenerateNormals(MeshData& mesh)
{
    for (Vertex& vertex : mesh.vertices)
    {
        vertex.normal = Vec3Make(0.0f, 0.0f, 0.0f);
    }

    for (std::size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
    {
        Vertex& a = mesh.vertices[mesh.indices[i + 0]];
        Vertex& b = mesh.vertices[mesh.indices[i + 1]];
        Vertex& c = mesh.vertices[mesh.indices[i + 2]];
        Vec3 ab = Vec3Sub(b.position, a.position);
        Vec3 ac = Vec3Sub(c.position, a.position);
        Vec3 faceNormal = Vec3Normalize(Vec3Cross(ab, ac));
        a.normal = Vec3Add(a.normal, faceNormal);
        b.normal = Vec3Add(b.normal, faceNormal);
        c.normal = Vec3Add(c.normal, faceNormal);
    }

    for (Vertex& vertex : mesh.vertices)
    {
        vertex.normal = Vec3Normalize(vertex.normal);
    }
}

std::filesystem::path ResolveTexturePath(
    const std::filesystem::path& objDirectory,
    const std::string& textureName
)
{
    std::filesystem::path asGiven = objDirectory / textureName;
    if (std::filesystem::exists(asGiven))
    {
        return asGiven;
    }

    std::filesystem::path fallbackTextures =
        std::filesystem::path("/home/vega/Coding/Graphics/software-renderer-cpp/assets/textures") / textureName;
    if (std::filesystem::exists(fallbackTextures))
    {
        return fallbackTextures;
    }

    return asGiven;
}

TextureData BuildMaterialTexture(
    const std::filesystem::path& objDirectory,
    const tinyobj::material_t& material
)
{
    if (!material.diffuse_texname.empty())
    {
        std::filesystem::path path = ResolveTexturePath(objDirectory, material.diffuse_texname);
        return LoadTexture(path.string());
    }

    auto clampColor = [](float v) -> std::uint8_t {
        v = std::clamp(v, 0.0f, 1.0f);
        return static_cast<std::uint8_t>(v * 255.0f + 0.5f);
    };

    return MakeSolidTexture(
        clampColor(material.diffuse[0]),
        clampColor(material.diffuse[1]),
        clampColor(material.diffuse[2])
    );
}
}

ModelData LoadObjModel(std::string_view path)
{
    tinyobj::ObjReaderConfig config{};
    std::filesystem::path objPath(path);
    std::filesystem::path objDirectory = objPath.parent_path();
    if (!objDirectory.empty())
    {
        config.mtl_search_path = objDirectory.string();
    }

    tinyobj::ObjReader reader{};
    if (!reader.ParseFromFile(std::string(path), config))
    {
        throw std::runtime_error("Failed to load OBJ: " + std::string(path) + " " + reader.Error());
    }

    if (!reader.Warning().empty())
    {
        SDL_Log("tinyobj warning: %s", reader.Warning().c_str());
    }

    const tinyobj::attrib_t& attrib = reader.GetAttrib();
    const std::vector<tinyobj::shape_t>& shapes = reader.GetShapes();
    const std::vector<tinyobj::material_t>& materials = reader.GetMaterials();

    ModelData model{};
    std::unordered_map<VertexKey, std::uint32_t, VertexKeyHasher> vertexMap{};
    std::vector<std::vector<std::uint32_t>> indexBuckets;

    bool hasNormals = !attrib.normals.empty();
    std::size_t materialCount = materials.empty() ? 1 : materials.size();
    model.materials.resize(materialCount);
    model.textures.resize(materialCount);
    indexBuckets.resize(materialCount);

    if (materials.empty())
    {
        model.materials[0].name = "default";
        model.materials[0].textureIndex = 0;
        model.textures[0] = MakeSolidTexture(255, 255, 255);
    }
    else
    {
        for (std::size_t i = 0; i < materials.size(); ++i)
        {
            model.materials[i].name = materials[i].name;
            model.materials[i].textureIndex = static_cast<std::int32_t>(i);
            model.textures[i] = BuildMaterialTexture(objDirectory, materials[i]);
        }
    }

    for (const tinyobj::shape_t& shape : shapes)
    {
        std::size_t indexOffset = 0;
        for (std::size_t face = 0; face < shape.mesh.num_face_vertices.size(); ++face)
        {
            unsigned char vertexCount = shape.mesh.num_face_vertices[face];
            int materialId = shape.mesh.material_ids.empty() ? -1 : shape.mesh.material_ids[face];
            std::size_t bucketIndex = materialId >= 0 ? static_cast<std::size_t>(materialId) : 0;
            if (bucketIndex >= indexBuckets.size())
            {
                bucketIndex = 0;
            }

            if (vertexCount != 3)
            {
                indexOffset += vertexCount;
                continue;
            }

            for (unsigned char i = 0; i < 3; ++i)
            {
                const tinyobj::index_t idx = shape.mesh.indices[indexOffset + i];
                VertexKey key{idx.vertex_index, idx.normal_index, idx.texcoord_index};
                auto found = vertexMap.find(key);
                if (found != vertexMap.end())
                {
                    indexBuckets[bucketIndex].push_back(found->second);
                    continue;
                }

                Vertex vertex{};
                vertex.position = ReadPosition(attrib, idx.vertex_index);
                vertex.normal = hasNormals && idx.normal_index >= 0
                    ? ReadNormal(attrib, idx.normal_index)
                    : Vec3Make(0.0f, 1.0f, 0.0f);
                vertex.uv = idx.texcoord_index >= 0
                    ? ReadUv(attrib, idx.texcoord_index)
                    : Vec2Make(0.0f, 0.0f);

                std::uint32_t newIndex = static_cast<std::uint32_t>(model.mesh.vertices.size());
                model.mesh.vertices.push_back(vertex);
                indexBuckets[bucketIndex].push_back(newIndex);
                vertexMap.emplace(key, newIndex);
            }

            indexOffset += vertexCount;
        }
    }

    for (std::size_t materialIndex = 0; materialIndex < indexBuckets.size(); ++materialIndex)
    {
        const std::vector<std::uint32_t>& bucket = indexBuckets[materialIndex];
        if (bucket.empty())
        {
            continue;
        }

        PrimitiveData primitive{};
        primitive.firstIndex = static_cast<std::uint32_t>(model.mesh.indices.size());
        primitive.indexCount = static_cast<std::uint32_t>(bucket.size());
        primitive.materialIndex = static_cast<std::uint32_t>(materialIndex);

        model.mesh.indices.insert(model.mesh.indices.end(), bucket.begin(), bucket.end());
        model.primitives.push_back(primitive);
    }

    if (!hasNormals)
    {
        GenerateNormals(model.mesh);
    }

    return model;
}
