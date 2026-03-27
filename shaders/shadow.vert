#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 3) in uvec4 inJointIndices;
layout(location = 4) in vec4 inJointWeights;

layout(binding = 0) uniform SceneUniforms
{
    mat4 view;
    mat4 proj;
    vec4 cameraPosition;
    vec4 lightPositions[4];
    vec4 lightColors[4];
    vec4 sunDirection;
    vec4 sunColor;
    vec4 ambientColor;
    vec4 celestialPositions[2];
    vec4 celestialColors[2];
    vec4 clearColor;
    vec4 spotLightPositions[32];
    vec4 spotLightDirections[32];
    vec4 spotLightColors[32];
    vec4 spotLightParams[32];
    vec4 shadowedSpotLightPositions[4];
    vec4 shadowedSpotLightDirections[4];
    vec4 shadowedSpotLightColors[4];
    vec4 shadowedSpotLightParams[4];
    vec4 sceneLightCounts;
    mat4 shadowViewProj[6];
    vec4 shadowParams;
    mat4 skinJoints[64];
    vec4 paintSplatPositions[128];
    vec4 paintSplatNormals[128];
    vec4 paintSplatColors[128];
    vec4 surfaceMaskParamsA;
    vec4 paintSplatCounts;
} uniforms;

layout(push_constant) uniform DrawPushConstants
{
    mat4 model;
    uint skinned;
    uint shadowCascade;
    uint pointLightMask;
    uint spotLightMask;
    uint shadowedSpotLightMask;
} drawPush;

void main()
{
    vec4 localPosition = vec4(inPosition, 1.0);
    if (drawPush.skinned != 0u)
    {
        mat4 skinMatrix =
            uniforms.skinJoints[inJointIndices.x] * inJointWeights.x +
            uniforms.skinJoints[inJointIndices.y] * inJointWeights.y +
            uniforms.skinJoints[inJointIndices.z] * inJointWeights.z +
            uniforms.skinJoints[inJointIndices.w] * inJointWeights.w;
        localPosition = skinMatrix * localPosition;
    }

    gl_Position = uniforms.shadowViewProj[drawPush.shadowCascade] * drawPush.model * localPosition;
}
