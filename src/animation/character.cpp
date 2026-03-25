#include "animation/character.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define UFBX_IMPLEMENTATION
#include "animation_ufbx.h"

#include "assets/texture_loader.h"

namespace
{
std::string NormalizeJointName(std::string s)
{
    auto cutAfterLast = [&](char c) {
        std::size_t p = s.find_last_of(c);
        if (p != std::string::npos && p + 1 < s.size())
        {
            s = s.substr(p + 1);
        }
    };

    cutAfterLast('|');
    cutAfterLast(':');
    return s;
}

Mat4 Mat4FromUfbx(const ufbx_matrix& m)
{
    Mat4 out = Mat4Identity();
    out.m[0] = static_cast<float>(m.m00);
    out.m[1] = static_cast<float>(m.m10);
    out.m[2] = static_cast<float>(m.m20);
    out.m[4] = static_cast<float>(m.m01);
    out.m[5] = static_cast<float>(m.m11);
    out.m[6] = static_cast<float>(m.m21);
    out.m[8] = static_cast<float>(m.m02);
    out.m[9] = static_cast<float>(m.m12);
    out.m[10] = static_cast<float>(m.m22);
    out.m[12] = static_cast<float>(m.m03);
    out.m[13] = static_cast<float>(m.m13);
    out.m[14] = static_cast<float>(m.m23);
    return out;
}

JointPose PoseFromUfbx(const ufbx_transform& t)
{
    JointPose pose{};
    pose.translation = Vec3Make(
        static_cast<float>(t.translation.x),
        static_cast<float>(t.translation.y),
        static_cast<float>(t.translation.z)
    );
    pose.rotation = Vec4Make(
        static_cast<float>(t.rotation.x),
        static_cast<float>(t.rotation.y),
        static_cast<float>(t.rotation.z),
        static_cast<float>(t.rotation.w)
    );
    pose.scale = Vec3Make(
        static_cast<float>(t.scale.x),
        static_cast<float>(t.scale.y),
        static_cast<float>(t.scale.z)
    );
    return pose;
}

Vec4 NormalizeQuat(Vec4 q)
{
    float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (length <= 1e-8f)
    {
        return Vec4Make(0.0f, 0.0f, 0.0f, 1.0f);
    }
    float inv = 1.0f / length;
    return Vec4Make(q.x * inv, q.y * inv, q.z * inv, q.w * inv);
}

JointPose LerpPose(JointPose a, JointPose b, float t)
{
    float dot = a.rotation.x * b.rotation.x +
                a.rotation.y * b.rotation.y +
                a.rotation.z * b.rotation.z +
                a.rotation.w * b.rotation.w;
    if (dot < 0.0f)
    {
        b.rotation.x = -b.rotation.x;
        b.rotation.y = -b.rotation.y;
        b.rotation.z = -b.rotation.z;
        b.rotation.w = -b.rotation.w;
    }

    JointPose out{};
    out.translation = Vec3Add(a.translation, Vec3Scale(Vec3Sub(b.translation, a.translation), t));
    out.scale = Vec3Add(a.scale, Vec3Scale(Vec3Sub(b.scale, a.scale), t));
    out.rotation = NormalizeQuat(Vec4Make(
        a.rotation.x + (b.rotation.x - a.rotation.x) * t,
        a.rotation.y + (b.rotation.y - a.rotation.y) * t,
        a.rotation.z + (b.rotation.z - a.rotation.z) * t,
        a.rotation.w + (b.rotation.w - a.rotation.w) * t
    ));
    return out;
}

Mat4 Mat4TranslateLocal(Vec3 t)
{
    Mat4 out = Mat4Identity();
    out.m[12] = t.x;
    out.m[13] = t.y;
    out.m[14] = t.z;
    return out;
}

Mat4 Mat4ScaleLocal(Vec3 s)
{
    Mat4 out = Mat4Identity();
    out.m[0] = s.x;
    out.m[5] = s.y;
    out.m[10] = s.z;
    return out;
}

Mat4 Mat4FromQuat(Vec4 q)
{
    q = NormalizeQuat(q);
    float xx = q.x * q.x;
    float yy = q.y * q.y;
    float zz = q.z * q.z;
    float xy = q.x * q.y;
    float xz = q.x * q.z;
    float yz = q.y * q.z;
    float wx = q.w * q.x;
    float wy = q.w * q.y;
    float wz = q.w * q.z;

    Mat4 out = Mat4Identity();
    out.m[0] = 1.0f - 2.0f * (yy + zz);
    out.m[1] = 2.0f * (xy + wz);
    out.m[2] = 2.0f * (xz - wy);
    out.m[4] = 2.0f * (xy - wz);
    out.m[5] = 1.0f - 2.0f * (xx + zz);
    out.m[6] = 2.0f * (yz + wx);
    out.m[8] = 2.0f * (xz + wy);
    out.m[9] = 2.0f * (yz - wx);
    out.m[10] = 1.0f - 2.0f * (xx + yy);
    return out;
}

Mat4 Mat4FromPose(JointPose pose)
{
    return Mat4Mul(
        Mat4TranslateLocal(pose.translation),
        Mat4Mul(Mat4FromQuat(pose.rotation), Mat4ScaleLocal(pose.scale))
    );
}

JointPose SampleClipJoint(const AnimationClip& clip, std::uint32_t joint, const JointPose& fallback, float timeSeconds)
{
    if (clip.jointCount == 0 || joint >= clip.jointCount || clip.samples.empty())
    {
        return fallback;
    }

    std::uint32_t frameCount = static_cast<std::uint32_t>(clip.samples.size() / clip.jointCount);
    if (frameCount == 0)
    {
        return fallback;
    }

    if (clip.duration <= 0.0f || clip.sampleRate <= 0.0f)
    {
        return clip.samples[joint];
    }

    float localTime = std::fmod(timeSeconds, clip.duration);
    if (localTime < 0.0f)
    {
        localTime += clip.duration;
    }

    float frame = localTime * clip.sampleRate;
    std::uint32_t i0 = static_cast<std::uint32_t>(std::floor(frame));
    std::uint32_t i1 = std::min(i0 + 1, frameCount - 1);
    float frac = frame - static_cast<float>(i0);
    i0 = std::min(i0, frameCount - 1);

    const JointPose& a = clip.samples[static_cast<std::size_t>(i0) * clip.jointCount + joint];
    const JointPose& b = clip.samples[static_cast<std::size_t>(i1) * clip.jointCount + joint];
    return LerpPose(a, b, frac);
}

struct SkinInfluence4
{
    std::array<std::uint16_t, 4> joint = {0, 0, 0, 0};
    std::array<float, 4> weight = {1.0f, 0.0f, 0.0f, 0.0f};
};

SkinInfluence4 InfluencesFromVertex(const ufbx_skin_deformer& skin, std::uint32_t vertexId)
{
    SkinInfluence4 out{};
    if (vertexId >= skin.vertices.count)
    {
        return out;
    }

    const ufbx_skin_vertex sv = skin.vertices.data[vertexId];
    std::uint32_t begin = sv.weight_begin;
    std::uint32_t count = sv.num_weights;
    if (count == 0 || begin >= skin.weights.count)
    {
        return out;
    }

    float sum = 0.0f;
    int taken = 0;
    for (std::uint32_t i = 0; i < count && taken < 4; ++i)
    {
        std::uint32_t wi = begin + i;
        if (wi >= skin.weights.count)
        {
            break;
        }

        const ufbx_skin_weight w = skin.weights.data[wi];
        out.joint[taken] = static_cast<std::uint16_t>(w.cluster_index);
        out.weight[taken] = static_cast<float>(w.weight);
        sum += out.weight[taken];
        ++taken;
    }

    if (sum > 0.0f)
    {
        for (float& weight : out.weight)
        {
            weight /= sum;
        }
    }

    return out;
}

AnimationClip LoadFbxAnimationClip(
    std::string_view path,
    const SkeletonData& skeleton,
    std::string_view clipName,
    float sampleRate
)
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

    const ufbx_anim* anim = scene->anim;
    double t0 = 0.0;
    double t1 = 0.0;
    double bestDuration = 0.0;
    for (std::size_t i = 0; i < scene->anim_stacks.count; ++i)
    {
        const ufbx_anim_stack* stack = scene->anim_stacks.data[i];
        if (stack == nullptr)
        {
            continue;
        }

        double duration = stack->time_end - stack->time_begin;
        if (duration > bestDuration)
        {
            bestDuration = duration;
            t0 = stack->time_begin;
            t1 = stack->time_end;
            if (stack->anim != nullptr)
            {
                anim = stack->anim;
            }
        }
    }

    if (anim == nullptr)
    {
        ufbx_free_scene(scene);
        throw std::runtime_error("FBX has no animation: " + std::string(path));
    }
    if (t1 <= t0)
    {
        t0 = anim->time_begin;
        t1 = anim->time_end;
    }
    if (t1 <= t0)
    {
        t0 = 0.0;
        t1 = 1.0;
    }

    std::unordered_map<std::string, const ufbx_node*> nodeByName{};
    nodeByName.reserve(scene->nodes.count);
    for (std::size_t i = 0; i < scene->nodes.count; ++i)
    {
        const ufbx_node* node = scene->nodes.data[i];
        if (node != nullptr)
        {
            nodeByName.emplace(NormalizeJointName(std::string(node->name.data, node->name.length)), node);
        }
    }

    AnimationClip clip{};
    clip.name = std::string(clipName);
    clip.sampleRate = sampleRate > 0.0f ? sampleRate : 30.0f;
    clip.jointCount = static_cast<std::uint32_t>(skeleton.joints.size());
    clip.duration = static_cast<float>(std::max(0.0, t1 - t0));

    float dt = 1.0f / clip.sampleRate;
    std::uint32_t frames = clip.duration > 0.0f
        ? static_cast<std::uint32_t>(std::ceil(clip.duration / dt)) + 1
        : 1;
    clip.samples.resize(static_cast<std::size_t>(frames) * clip.jointCount);

    for (std::uint32_t frame = 0; frame < frames; ++frame)
    {
        double time = std::min(t0 + static_cast<double>(frame) * static_cast<double>(dt), t1);
        for (std::uint32_t jointIndex = 0; jointIndex < clip.jointCount; ++jointIndex)
        {
            JointPose pose = skeleton.joints[jointIndex].restLocal;
            auto found = nodeByName.find(skeleton.joints[jointIndex].name);
            if (found != nodeByName.end())
            {
                pose = PoseFromUfbx(ufbx_evaluate_transform(anim, found->second, time));
            }
            clip.samples[static_cast<std::size_t>(frame) * clip.jointCount + jointIndex] = pose;
        }
    }

    ufbx_free_scene(scene);
    return clip;
}
}

CharacterAnimationSet LoadKenneyCharacterAnimationSet(
    std::string_view characterPath,
    std::string_view texturePath,
    std::string_view idlePath,
    std::string_view runPath,
    std::string_view jumpPath
)
{
    ufbx_load_opts opts{};
    opts.target_axes.right = UFBX_COORDINATE_AXIS_POSITIVE_X;
    opts.target_axes.up = UFBX_COORDINATE_AXIS_POSITIVE_Y;
    opts.target_axes.front = UFBX_COORDINATE_AXIS_NEGATIVE_Z;
    opts.target_unit_meters = 1.0f;
    opts.allow_nodes_out_of_root = false;

    ufbx_error error{};
    ufbx_scene* scene = ufbx_load_file(std::string(characterPath).c_str(), &opts, &error);
    if (scene == nullptr)
    {
        throw std::runtime_error("ufbx_load_file failed: " + std::string(characterPath));
    }

    const ufbx_mesh* mesh = nullptr;
    const ufbx_node* meshNode = nullptr;
    const ufbx_skin_deformer* skin = nullptr;
    for (std::size_t meshIndex = 0; meshIndex < scene->meshes.count; ++meshIndex)
    {
        const ufbx_mesh* candidate = scene->meshes.data[meshIndex];
        if (candidate == nullptr || candidate->skin_deformers.count == 0)
        {
            continue;
        }

        mesh = candidate;
        skin = candidate->skin_deformers.data[0];
        meshNode = candidate->instances.count > 0 ? candidate->instances.data[0] : scene->root_node;
        break;
    }

    if (mesh == nullptr || skin == nullptr)
    {
        ufbx_free_scene(scene);
        throw std::runtime_error("FBX has no skinned mesh: " + std::string(characterPath));
    }

    CharacterAnimationSet set{};
    set.asset.texture = LoadTexture(texturePath);
    set.asset.skeleton.worldToModel = Mat4FromUfbx(ufbx_matrix_invert(&meshNode->geometry_to_world));

    std::unordered_set<const ufbx_node*> required{};
    required.insert(scene->root_node);
    for (std::size_t clusterIndex = 0; clusterIndex < skin->clusters.count; ++clusterIndex)
    {
        const ufbx_skin_cluster* cluster = skin->clusters.data[clusterIndex];
        if (cluster == nullptr || cluster->bone_node == nullptr)
        {
            continue;
        }

        const ufbx_node* node = cluster->bone_node;
        while (node != nullptr)
        {
            required.insert(node);
            if (node->is_root)
            {
                break;
            }
            node = node->parent;
        }
    }

    std::vector<const ufbx_node*> ordered{};
    std::unordered_map<const ufbx_node*, std::uint16_t> nodeToJoint{};
    auto dfs = [&](auto&& self, const ufbx_node* node) -> void {
        if (node == nullptr)
        {
            return;
        }

        if (required.find(node) != required.end())
        {
            std::uint16_t jointIndex = static_cast<std::uint16_t>(ordered.size());
            ordered.push_back(node);
            nodeToJoint.emplace(node, jointIndex);
        }

        for (std::size_t childIndex = 0; childIndex < node->children.count; ++childIndex)
        {
            self(self, node->children.data[childIndex]);
        }
    };
    dfs(dfs, scene->root_node);

    if (ordered.size() > kMaxSkinJoints)
    {
        ufbx_free_scene(scene);
        throw std::runtime_error("Character skeleton exceeds joint limit");
    }

    set.asset.skeleton.joints.resize(ordered.size());
    for (std::size_t jointIndex = 0; jointIndex < ordered.size(); ++jointIndex)
    {
        const ufbx_node* node = ordered[jointIndex];
        SkeletonJoint joint{};
        joint.name = NormalizeJointName(std::string(node->name.data, node->name.length));
        joint.restLocal = PoseFromUfbx(node->local_transform);
        joint.invBind = Mat4Identity();

        auto parentIt = nodeToJoint.find(node->parent);
        joint.parent = parentIt != nodeToJoint.end() ? static_cast<int>(parentIt->second) : -1;
        set.asset.skeleton.joints[jointIndex] = joint;
    }

    std::vector<std::uint16_t> clusterToJoint(skin->clusters.count, 0);
    for (std::size_t clusterIndex = 0; clusterIndex < skin->clusters.count; ++clusterIndex)
    {
        const ufbx_skin_cluster* cluster = skin->clusters.data[clusterIndex];
        if (cluster == nullptr || cluster->bone_node == nullptr)
        {
            continue;
        }

        auto found = nodeToJoint.find(cluster->bone_node);
        if (found == nodeToJoint.end())
        {
            continue;
        }

        clusterToJoint[clusterIndex] = found->second;
        set.asset.skeleton.joints[found->second].invBind = Mat4FromUfbx(cluster->geometry_to_bone);
    }

    std::vector<std::uint32_t> triIndices(mesh->max_face_triangles * 3);
    auto appendVertex = [&](std::uint32_t idx) {
        Vertex out{};

        if (mesh->vertex_position.exists && idx < mesh->vertex_position.indices.count)
        {
            std::uint32_t vi = mesh->vertex_position.indices.data[idx];
            if (vi < mesh->vertex_position.values.count)
            {
                const ufbx_vec3 v = mesh->vertex_position.values.data[vi];
                out.position = Vec3Make(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z));
            }
        }
        if (mesh->vertex_normal.exists && idx < mesh->vertex_normal.indices.count)
        {
            std::uint32_t ni = mesh->vertex_normal.indices.data[idx];
            if (ni < mesh->vertex_normal.values.count)
            {
                const ufbx_vec3 v = mesh->vertex_normal.values.data[ni];
                out.normal = Vec3Normalize(
                    Vec3Make(static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z))
                );
            }
        }
        if (mesh->vertex_uv.exists && idx < mesh->vertex_uv.indices.count)
        {
            std::uint32_t ui = mesh->vertex_uv.indices.data[idx];
            if (ui < mesh->vertex_uv.values.count)
            {
                const ufbx_vec2 v = mesh->vertex_uv.values.data[ui];
                out.uv = Vec2Make(static_cast<float>(v.x), 1.0f - static_cast<float>(v.y));
            }
        }

        std::uint32_t vertexId = idx < mesh->vertex_indices.count ? mesh->vertex_indices.data[idx] : idx;
        SkinInfluence4 influence = InfluencesFromVertex(*skin, vertexId);
        for (int k = 0; k < 4; ++k)
        {
            std::uint16_t cluster = influence.joint[k];
            out.jointIndices[k] = cluster < clusterToJoint.size() ? clusterToJoint[cluster] : 0;
        }
        out.jointWeights = Vec4Make(
            influence.weight[0],
            influence.weight[1],
            influence.weight[2],
            influence.weight[3]
        );

        std::uint32_t outIndex = static_cast<std::uint32_t>(set.asset.mesh.vertices.size());
        set.asset.mesh.vertices.push_back(out);
        set.asset.mesh.indices.push_back(outIndex);
    };

    for (std::size_t faceIndex = 0; faceIndex < mesh->faces.count; ++faceIndex)
    {
        const ufbx_face face = mesh->faces.data[faceIndex];
        if (face.num_indices < 3)
        {
            continue;
        }

        std::uint32_t triCount = ufbx_triangulate_face(triIndices.data(), triIndices.size(), mesh, face);
        for (std::uint32_t tri = 0; tri < triCount; ++tri)
        {
            appendVertex(triIndices[tri * 3 + 0]);
            appendVertex(triIndices[tri * 3 + 1]);
            appendVertex(triIndices[tri * 3 + 2]);
        }
    }

    Mat4 rotFix = Mat4RotateX(-DegreesToRadians(90.0f));
    Vec3 mn = set.asset.mesh.vertices.empty() ? Vec3Make(0.0f, 0.0f, 0.0f) : Vec3Make(1e30f, 1e30f, 1e30f);
    Vec3 mx = set.asset.mesh.vertices.empty() ? Vec3Make(1.0f, 1.0f, 1.0f) : Vec3Make(-1e30f, -1e30f, -1e30f);
    for (const Vertex& vertex : set.asset.mesh.vertices)
    {
        Vec4 rotated = Mat4MulVec4(rotFix, Vec4Make(vertex.position.x, vertex.position.y, vertex.position.z, 1.0f));
        mn.x = std::min(mn.x, rotated.x);
        mn.y = std::min(mn.y, rotated.y);
        mn.z = std::min(mn.z, rotated.z);
        mx.x = std::max(mx.x, rotated.x);
        mx.y = std::max(mx.y, rotated.y);
        mx.z = std::max(mx.z, rotated.z);
    }

    float height = std::max(mx.y - mn.y, 1e-5f);
    float scale = 1.0f / height;
    Vec3 pivot = Vec3Make((mn.x + mx.x) * 0.5f, mn.y, (mn.z + mx.z) * 0.5f);
    Vec3 pivotTranslation = Vec3Make(-pivot.x * scale, -pivot.y * scale, -pivot.z * scale);
    set.asset.modelOffset = Mat4Mul(
        Mat4TranslateLocal(pivotTranslation),
        Mat4Mul(Mat4Scale(scale), rotFix)
    );

    ufbx_free_scene(scene);

    set.idle = LoadFbxAnimationClip(idlePath, set.asset.skeleton, "idle", 30.0f);
    set.run = LoadFbxAnimationClip(runPath, set.asset.skeleton, "run", 30.0f);
    set.jump = LoadFbxAnimationClip(jumpPath, set.asset.skeleton, "jump", 30.0f);
    return set;
}

void EvaluateCharacterClip(
    const SkinnedCharacterAsset& asset,
    const AnimationClip& clip,
    float timeSeconds,
    CharacterRenderState& outState
)
{
    outState.jointCount = static_cast<std::uint32_t>(
        std::min<std::size_t>(asset.skeleton.joints.size(), outState.joints.size())
    );
    outState.visible = outState.jointCount > 0;

    std::vector<Mat4> global(outState.jointCount, Mat4Identity());
    for (std::uint32_t jointIndex = 0; jointIndex < outState.jointCount; ++jointIndex)
    {
        const SkeletonJoint& joint = asset.skeleton.joints[jointIndex];
        JointPose localPose = SampleClipJoint(clip, jointIndex, joint.restLocal, timeSeconds);
        Mat4 localMatrix = Mat4FromPose(localPose);
        global[jointIndex] = joint.parent >= 0
            ? Mat4Mul(global[static_cast<std::size_t>(joint.parent)], localMatrix)
            : localMatrix;
        outState.joints[jointIndex] = Mat4Mul(
            asset.skeleton.worldToModel,
            Mat4Mul(global[jointIndex], joint.invBind)
        );
    }

    for (std::size_t i = outState.jointCount; i < outState.joints.size(); ++i)
    {
        outState.joints[i] = Mat4Identity();
    }
}
