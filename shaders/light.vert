#version 460

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

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

layout(location = 0) out vec3 fragColor;

void main()
{
    fragColor = inColor;
    gl_PointSize = 16.0;
    gl_Position = uniforms.proj * uniforms.view * vec4(inPosition, 1.0);
}
