#version 460

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
    vec4 surfaceMaskParamsB;
    vec4 paintSplatCounts;
} uniforms;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;
layout(location = 5) in vec4 inModelRow0;
layout(location = 6) in vec4 inModelRow1;
layout(location = 7) in vec4 inModelRow2;
layout(location = 8) in vec4 inModelRow3;
layout(location = 9) in uvec4 inLightMasks;
layout(location = 10) in uvec4 inBatchInfo;

layout(location = 0) out vec3 fragWorldPosition;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUv;
layout(location = 3) out vec3 fragLocalPosition;
layout(location = 4) out vec3 fragLocalNormal;
layout(location = 5) out vec4 fragShadowPosition0;
layout(location = 6) out vec4 fragShadowPosition1;
layout(location = 7) out float fragViewDepth;
layout(location = 8) flat out uvec4 fragLightMasks;
layout(location = 9) flat out uvec4 fragBatchInfo;

void main()
{
    mat4 model = mat4(inModelRow0, inModelRow1, inModelRow2, inModelRow3);
    vec4 localPosition = vec4(inPosition, 1.0);
    vec4 worldPosition = model * localPosition;
    mat3 normalMatrix = mat3(model);

    fragWorldPosition = worldPosition.xyz;
    fragNormal = normalize(normalMatrix * inNormal);
    fragUv = inUv;
    fragLocalPosition = inPosition;
    fragLocalNormal = normalize(inNormal);
    fragLightMasks = inLightMasks;
    fragBatchInfo = inBatchInfo;
    vec4 viewPosition = uniforms.view * worldPosition;
    fragShadowPosition0 = uniforms.shadowViewProj[0] * worldPosition;
    fragShadowPosition1 = uniforms.shadowViewProj[1] * worldPosition;
    fragViewDepth = -viewPosition.z;

    gl_Position = uniforms.proj * viewPosition;
}
