#version 460

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

layout(binding = 1) uniform sampler2D albedoTexture;

layout(location = 0) in vec3 fragWorldPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUv;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 normal = normalize(fragNormal);
    vec3 viewDir = normalize(uniforms.cameraPosition.xyz - fragWorldPosition);
    vec3 lightDir = normalize(uniforms.pointLightPosition.xyz - fragWorldPosition);
    vec3 halfDir = normalize(lightDir + viewDir);

    float distanceToLight = length(uniforms.pointLightPosition.xyz - fragWorldPosition);
    float attenuation = 1.0 / (1.0 + 0.18 * distanceToLight + 0.035 * distanceToLight * distanceToLight);

    float ndotl = max(dot(normal, lightDir), 0.0);
    float specular = pow(max(dot(normal, halfDir), 0.0), 48.0);

    vec3 albedo = texture(albedoTexture, fragUv).rgb;
    vec3 diffuse = albedo * uniforms.pointLightColor.rgb * ndotl * attenuation;
    vec3 ambient = albedo * uniforms.ambientColor.rgb;
    vec3 specularColor = uniforms.pointLightColor.rgb * specular * attenuation * 0.35;

    vec3 color = ambient + diffuse + specularColor;
    color = color / (color + vec3(1.0));
    color = pow(color, vec3(1.0 / 2.2));

    outColor = vec4(color, 1.0);
}
