#include "animation/character_internal.h"

#include <algorithm>
#include <cmath>

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
