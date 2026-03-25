#pragma once

#include <cstdint>

struct Vec2
{
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Vec4
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
};

struct Mat4
{
    float m[16] = {};
};

Vec2 Vec2Make(float x, float y);
Vec3 Vec3Make(float x, float y, float z);
Vec4 Vec4Make(float x, float y, float z, float w);

Vec3 Vec3Add(Vec3 a, Vec3 b);
Vec3 Vec3Sub(Vec3 a, Vec3 b);
Vec3 Vec3Scale(Vec3 v, float s);
Vec3 Vec3Mul(Vec3 a, Vec3 b);
float Vec3Dot(Vec3 a, Vec3 b);
Vec3 Vec3Cross(Vec3 a, Vec3 b);
float Vec3Length(Vec3 v);
Vec3 Vec3Normalize(Vec3 v);

Mat4 Mat4Identity();
Mat4 Mat4Mul(Mat4 a, Mat4 b);
Mat4 Mat4Translate(Vec3 t);
Mat4 Mat4RotateX(float radians);
Mat4 Mat4RotateY(float radians);
Mat4 Mat4RotateZ(float radians);
Mat4 Mat4Scale(float s);
Mat4 Mat4Perspective(float fovYRadians, float aspect, float zNear, float zFar);
Mat4 Mat4LookAt(Vec3 eye, Vec3 target, Vec3 up);
Vec4 Mat4MulVec4(Mat4 m, Vec4 v);

float DegreesToRadians(float degrees);
