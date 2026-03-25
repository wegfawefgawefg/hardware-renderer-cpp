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
    mat4 skinJoints[64];
} uniforms;

layout(push_constant) uniform DrawPushConstants
{
    mat4 model;
    uint skinned;
    uint padding0;
    uint padding1;
    uint padding2;
} drawPush;

layout(location = 0) out vec3 fragWorldPosition;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUv;

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

    gl_Position = uniforms.proj * uniforms.view * worldPosition;
}
