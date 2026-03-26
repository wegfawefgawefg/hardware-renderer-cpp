#include "animation/character_internal.h"

#include <algorithm>
#include <vector>

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
