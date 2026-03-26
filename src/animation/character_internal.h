#pragma once

#include <string>

#include "animation/character.h"
#include "animation_ufbx.h"

std::string NormalizeJointName(std::string s);
Mat4 Mat4FromUfbx(const ufbx_matrix& m);
JointPose PoseFromUfbx(const ufbx_transform& t);
Vec4 NormalizeQuat(Vec4 q);
JointPose LerpPose(JointPose a, JointPose b, float t);
Mat4 Mat4TranslateLocal(Vec3 t);
Mat4 Mat4ScaleLocal(Vec3 s);
Mat4 Mat4FromQuat(Vec4 q);
Mat4 Mat4FromPose(JointPose pose);
JointPose SampleClipJoint(const AnimationClip& clip, std::uint32_t joint, const JointPose& fallback, float timeSeconds);

AnimationClip LoadFbxAnimationClip(
    std::string_view path,
    const SkeletonData& skeleton,
    std::string_view clipName,
    float sampleRate
);
