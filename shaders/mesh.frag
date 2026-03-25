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
    mat4 skinJoints[64];
} uniforms;

layout(binding = 1) uniform sampler2D albedoTexture;

layout(location = 0) in vec3 fragWorldPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUv;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 normal = normalize(fragNormal);
    vec3 albedo = texture(albedoTexture, fragUv).rgb;
    vec3 ambient = albedo * uniforms.ambientColor.rgb;

    vec3 lighting = ambient;
    vec3 sunDir = normalize(-uniforms.sunDirection.xyz);
    float sunNdotL = max(dot(normal, sunDir), 0.0);
    lighting += albedo * uniforms.sunColor.rgb * sunNdotL;

    for (int i = 0; i < 4; ++i)
    {
        vec3 lightOffset = uniforms.lightPositions[i].xyz - fragWorldPosition;
        float distanceToLight = length(lightOffset);
        vec3 lightDir = distanceToLight > 0.0001 ? lightOffset / distanceToLight : vec3(0.0, 1.0, 0.0);
        float attenuation = 1.0 / (1.0 + 0.14 * distanceToLight + 0.03 * distanceToLight * distanceToLight);
        float ndotl = max(dot(normal, lightDir), 0.0);
        lighting += albedo * uniforms.lightColors[i].rgb * ndotl * attenuation;
    }

    vec3 color = lighting;
    color = color / (color + vec3(1.0));

    outColor = vec4(color, 1.0);
}
