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
    mat4 shadowViewProj[2];
    vec4 shadowParams;
    mat4 skinJoints[64];
} uniforms;

layout(binding = 1) uniform sampler2D albedoTexture;
layout(binding = 2) uniform sampler2D shadowMaps[2];

layout(location = 0) in vec3 fragWorldPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUv;
layout(location = 3) in vec4 fragShadowPosition0;
layout(location = 4) in vec4 fragShadowPosition1;
layout(location = 5) in float fragViewDepth;

layout(location = 0) out vec4 outColor;

bool shadowPositionCovered(vec4 shadowPosition, int cascadeIndex)
{
    vec3 projected = shadowPosition.xyz / max(shadowPosition.w, 0.0001);
    vec2 uv = projected.xy * 0.5 + 0.5;
    float currentDepth = projected.z;
    ivec2 mapSize = textureSize(shadowMaps[cascadeIndex], 0);
    vec2 texel = 1.0 / vec2(mapSize);
    float border = uniforms.shadowParams.x < 0.5 ? 1.0 : 2.0;
    vec2 margin = texel * border;
    return uv.x >= margin.x && uv.x <= 1.0 - margin.x &&
           uv.y >= margin.y && uv.y <= 1.0 - margin.y &&
           currentDepth > 0.0 && currentDepth < 1.0;
}

float sampleShadow(int cascadeIndex, vec4 shadowPosition, vec3 normal, vec3 lightDir)
{
    vec3 projected = shadowPosition.xyz / max(shadowPosition.w, 0.0001);
    vec2 uv = projected.xy * 0.5 + 0.5;
    float currentDepth = projected.z;
    if (currentDepth <= 0.0 || currentDepth >= 1.0)
    {
        return 1.0;
    }

    float bias = max(0.00025, 0.0012 * (1.0 - max(dot(normal, lightDir), 0.0)));
    ivec2 mapSize = textureSize(shadowMaps[cascadeIndex], 0);
    vec2 texel = 1.0 / vec2(mapSize);
    if (uniforms.shadowParams.x < 0.5)
    {
        ivec2 texelCoord = clamp(ivec2(uv * vec2(mapSize)), ivec2(0), mapSize - ivec2(1));
        float closestDepth = texelFetch(shadowMaps[cascadeIndex], texelCoord, 0).r;
        return currentDepth - bias <= closestDepth ? 1.0 : 0.0;
    }

    float visibility = 0.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            float closestDepth = texture(shadowMaps[cascadeIndex], uv + vec2(x, y) * texel).r;
            visibility += currentDepth - bias <= closestDepth ? 1.0 : 0.0;
        }
    }
    return visibility / 9.0;
}

void main()
{
    vec3 normal = normalize(fragNormal);
    vec3 albedo = texture(albedoTexture, fragUv).rgb;
    vec3 ambient = albedo * uniforms.ambientColor.rgb;

    vec3 lighting = ambient;
    vec3 sunDir = normalize(-uniforms.sunDirection.xyz);
    float sunNdotL = max(dot(normal, sunDir), 0.0);
    bool covered0 = shadowPositionCovered(fragShadowPosition0, 0);
    bool covered1 = shadowPositionCovered(fragShadowPosition1, 1);
    float sunShadow = 1.0;
    if (covered0 && covered1)
    {
        float split = uniforms.shadowParams.y;
        float blendWidth = max(uniforms.shadowParams.z, 0.0001);
        float nearShadow = sampleShadow(0, fragShadowPosition0, normal, sunDir);
        float farShadow = sampleShadow(1, fragShadowPosition1, normal, sunDir);
        float blendT = smoothstep(split - blendWidth, split + blendWidth, fragViewDepth);
        sunShadow = mix(nearShadow, farShadow, blendT);
    }
    else if (covered0)
    {
        sunShadow = sampleShadow(0, fragShadowPosition0, normal, sunDir);
    }
    else if (covered1)
    {
        sunShadow = sampleShadow(1, fragShadowPosition1, normal, sunDir);
    }
    lighting += albedo * uniforms.sunColor.rgb * sunNdotL * sunShadow;

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
