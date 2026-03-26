#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
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
    uint persistentPaintOffset;
    uint persistentPaintCount;
} drawPush;

layout(location = 0) out vec3 fragWorldPosition;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUv;
layout(location = 3) out vec3 fragLocalPosition;
layout(location = 4) out vec3 fragLocalNormal;
layout(location = 5) out vec4 fragShadowPosition0;
layout(location = 6) out vec4 fragShadowPosition1;
layout(location = 7) out float fragViewDepth;

void main()
{
    vec4 localPosition = vec4(inPosition, 1.0);
    vec3 localNormal = inNormal;
    if (drawPush.skinned != 0u)
    {
        mat4 skinMatrix =
            uniforms.skinJoints[inJointIndices.x] * inJointWeights.x +
            uniforms.skinJoints[inJointIndices.y] * inJointWeights.y +
            uniforms.skinJoints[inJointIndices.z] * inJointWeights.z +
            uniforms.skinJoints[inJointIndices.w] * inJointWeights.w;
        localPosition = skinMatrix * localPosition;
        localNormal = mat3(skinMatrix) * localNormal;
    }

    vec4 worldPosition = drawPush.model * localPosition;
    mat3 normalMatrix = mat3(drawPush.model);

    fragWorldPosition = worldPosition.xyz;
    fragNormal = normalize(normalMatrix * localNormal);
    fragUv = inUv;
    fragLocalPosition = localPosition.xyz;
    fragLocalNormal = normalize(localNormal);
    vec4 viewPosition = uniforms.view * worldPosition;
    fragShadowPosition0 = uniforms.shadowViewProj[0] * worldPosition;
    fragShadowPosition1 = uniforms.shadowViewProj[1] * worldPosition;
    fragViewDepth = -viewPosition.z;

    gl_Position = uniforms.proj * viewPosition;
}
