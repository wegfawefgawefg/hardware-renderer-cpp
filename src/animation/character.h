#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "scene.h"

constexpr std::uint32_t kMaxSkinJoints = 64;

struct JointPose
{
    Vec3 translation = {};
    Vec4 rotation = {0.0f, 0.0f, 0.0f, 1.0f};
    Vec3 scale = {1.0f, 1.0f, 1.0f};
};

struct SkeletonJoint
{
    std::string name;
    int parent = -1;
    JointPose restLocal = {};
    Mat4 invBind = {};
};

struct SkeletonData
{
    std::vector<SkeletonJoint> joints;
    Mat4 worldToModel = {};
};

struct AnimationClip
{
    std::string name;
    float duration = 0.0f;
    float sampleRate = 30.0f;
    std::uint32_t jointCount = 0;
    std::vector<JointPose> samples;
};

struct SkinnedCharacterAsset
{
    MeshData mesh;
    TextureData texture;
    SkeletonData skeleton;
    Mat4 modelOffset = {};
};

struct CharacterAnimationSet
{
    SkinnedCharacterAsset asset;
    AnimationClip idle;
    AnimationClip run;
    AnimationClip jump;
};

struct CharacterRenderState
{
    Mat4 model = {};
    std::array<Mat4, kMaxSkinJoints> joints = {};
    std::uint32_t jointCount = 0;
    bool visible = false;
};

CharacterAnimationSet LoadKenneyCharacterAnimationSet(
    std::string_view characterPath,
    std::string_view texturePath,
    std::string_view idlePath,
    std::string_view runPath,
    std::string_view jumpPath
);

void EvaluateCharacterClip(
    const SkinnedCharacterAsset& asset,
    const AnimationClip& clip,
    float timeSeconds,
    CharacterRenderState& outState
);
