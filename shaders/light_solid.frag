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
    vec4 paintSplatCounts;
} uniforms;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec3 fragWorldPosition;
layout(location = 0) out vec4 outColor;

void main()
{
    vec3 dpdx = dFdx(fragWorldPosition);
    vec3 dpdy = dFdy(fragWorldPosition);
    vec3 normal = normalize(cross(dpdx, dpdy));
    if (!gl_FrontFacing)
    {
        normal = -normal;
    }

    vec3 sunDir = normalize(-uniforms.sunDirection.xyz);
    float diffuse = max(dot(normal, sunDir), 0.0);
    vec3 ambient = uniforms.ambientColor.rgb * 0.85 + vec3(0.08);
    vec3 lit = fragColor * (ambient + uniforms.sunColor.rgb * diffuse * 0.85);
    outColor = vec4(lit, 1.0);
}
