#include "math_types.h"

#include <cmath>

namespace
{
constexpr float kPi = 3.14159265358979323846f;
}

Vec2 Vec2Make(float x, float y)
{
    return Vec2{x, y};
}

Vec3 Vec3Make(float x, float y, float z)
{
    return Vec3{x, y, z};
}

Vec4 Vec4Make(float x, float y, float z, float w)
{
    return Vec4{x, y, z, w};
}

Vec3 Vec3Add(Vec3 a, Vec3 b)
{
    return Vec3Make(a.x + b.x, a.y + b.y, a.z + b.z);
}

Vec3 Vec3Sub(Vec3 a, Vec3 b)
{
    return Vec3Make(a.x - b.x, a.y - b.y, a.z - b.z);
}

Vec3 Vec3Scale(Vec3 v, float s)
{
    return Vec3Make(v.x * s, v.y * s, v.z * s);
}

Vec3 Vec3Mul(Vec3 a, Vec3 b)
{
    return Vec3Make(a.x * b.x, a.y * b.y, a.z * b.z);
}

float Vec3Dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 Vec3Cross(Vec3 a, Vec3 b)
{
    return Vec3Make(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

float Vec3Length(Vec3 v)
{
    return std::sqrt(Vec3Dot(v, v));
}

Vec3 Vec3Normalize(Vec3 v)
{
    float length = Vec3Length(v);
    if (length <= 0.000001f)
    {
        return Vec3Make(0.0f, 0.0f, 0.0f);
    }
    return Vec3Scale(v, 1.0f / length);
}

Mat4 Mat4Identity()
{
    Mat4 result{};
    result.m[0] = 1.0f;
    result.m[5] = 1.0f;
    result.m[10] = 1.0f;
    result.m[15] = 1.0f;
    return result;
}

Mat4 Mat4Mul(Mat4 a, Mat4 b)
{
    Mat4 result{};
    for (int row = 0; row < 4; ++row)
    {
        for (int col = 0; col < 4; ++col)
        {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k)
            {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            result.m[col * 4 + row] = sum;
        }
    }
    return result;
}

Mat4 Mat4Translate(Vec3 t)
{
    Mat4 result = Mat4Identity();
    result.m[12] = t.x;
    result.m[13] = t.y;
    result.m[14] = t.z;
    return result;
}

Mat4 Mat4RotateX(float radians)
{
    Mat4 result = Mat4Identity();
    float c = std::cos(radians);
    float s = std::sin(radians);
    result.m[5] = c;
    result.m[6] = s;
    result.m[9] = -s;
    result.m[10] = c;
    return result;
}

Mat4 Mat4RotateY(float radians)
{
    Mat4 result = Mat4Identity();
    float c = std::cos(radians);
    float s = std::sin(radians);
    result.m[0] = c;
    result.m[2] = -s;
    result.m[8] = s;
    result.m[10] = c;
    return result;
}

Mat4 Mat4RotateZ(float radians)
{
    Mat4 result = Mat4Identity();
    float c = std::cos(radians);
    float s = std::sin(radians);
    result.m[0] = c;
    result.m[1] = s;
    result.m[4] = -s;
    result.m[5] = c;
    return result;
}

Mat4 Mat4Scale(float s)
{
    Mat4 result = Mat4Identity();
    result.m[0] = s;
    result.m[5] = s;
    result.m[10] = s;
    return result;
}

Mat4 Mat4Perspective(float fovYRadians, float aspect, float zNear, float zFar)
{
    float tanHalfFov = std::tan(fovYRadians * 0.5f);

    Mat4 result{};
    result.m[0] = 1.0f / (aspect * tanHalfFov);
    result.m[5] = 1.0f / tanHalfFov;
    result.m[10] = -(zFar + zNear) / (zFar - zNear);
    result.m[11] = -1.0f;
    result.m[14] = -(2.0f * zFar * zNear) / (zFar - zNear);
    return result;
}

Mat4 Mat4LookAt(Vec3 eye, Vec3 target, Vec3 up)
{
    Vec3 forward = Vec3Normalize(Vec3Sub(target, eye));
    Vec3 right = Vec3Normalize(Vec3Cross(forward, up));
    Vec3 realUp = Vec3Cross(right, forward);

    Mat4 result = Mat4Identity();
    result.m[0] = right.x;
    result.m[1] = realUp.x;
    result.m[2] = -forward.x;
    result.m[4] = right.y;
    result.m[5] = realUp.y;
    result.m[6] = -forward.y;
    result.m[8] = right.z;
    result.m[9] = realUp.z;
    result.m[10] = -forward.z;
    result.m[12] = -Vec3Dot(right, eye);
    result.m[13] = -Vec3Dot(realUp, eye);
    result.m[14] = Vec3Dot(forward, eye);
    return result;
}

Vec4 Mat4MulVec4(Mat4 m, Vec4 v)
{
    return Vec4Make(
        m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z + m.m[12] * v.w,
        m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z + m.m[13] * v.w,
        m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14] * v.w,
        m.m[3] * v.x + m.m[7] * v.y + m.m[11] * v.z + m.m[15] * v.w
    );
}

float DegreesToRadians(float degrees)
{
    return degrees * (kPi / 180.0f);
}
