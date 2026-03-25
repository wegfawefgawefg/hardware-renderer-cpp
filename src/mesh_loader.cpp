#include "mesh_loader.h"

#include <SDL3/SDL_log.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <stdexcept>
#include <string>
#include <unordered_map>

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
}

MeshData LoadObjMesh(std::string_view path)
{
    tinyobj::ObjReaderConfig config{};
    std::size_t slash = path.find_last_of('/');
    if (slash != std::string_view::npos)
    {
        config.mtl_search_path = std::string(path.substr(0, slash));
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

    MeshData mesh{};
    std::unordered_map<VertexKey, std::uint32_t, VertexKeyHasher> vertexMap{};

    bool hasNormals = !attrib.normals.empty();

    for (const tinyobj::shape_t& shape : shapes)
    {
        std::size_t indexOffset = 0;
        for (unsigned char vertexCount : shape.mesh.num_face_vertices)
        {
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
                    mesh.indices.push_back(found->second);
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

                std::uint32_t newIndex = static_cast<std::uint32_t>(mesh.vertices.size());
                mesh.vertices.push_back(vertex);
                mesh.indices.push_back(newIndex);
                vertexMap.emplace(key, newIndex);
            }

            indexOffset += vertexCount;
        }
    }

    if (!hasNormals)
    {
        GenerateNormals(mesh);
    }

    return mesh;
}
