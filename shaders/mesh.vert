#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;

layout(binding = 0) uniform SceneUniforms
{
    mat4 model;
    mat4 view;
    mat4 proj;
    vec4 cameraPosition;
    vec4 pointLightPosition;
    vec4 pointLightColor;
    vec4 ambientColor;
} uniforms;

layout(location = 0) out vec3 fragWorldPosition;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec2 fragUv;

void main()
{
    vec4 worldPosition = uniforms.model * vec4(inPosition, 1.0);
    mat3 normalMatrix = mat3(uniforms.model);

    fragWorldPosition = worldPosition.xyz;
    fragNormal = normalize(normalMatrix * inNormal);
    fragUv = inUv;

    gl_Position = uniforms.proj * uniforms.view * worldPosition;
}
