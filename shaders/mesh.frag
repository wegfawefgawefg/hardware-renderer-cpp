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
    vec4 paintSplatCounts;
} uniforms;

layout(binding = 1) uniform sampler2D albedoTexture;
layout(binding = 2) uniform sampler2D shadowMaps[6];
struct PersistentPaintStamp
{
    vec4 positionRadius;
    vec4 normalSeed;
    vec4 colorOpacity;
};
layout(std430, binding = 3) readonly buffer PersistentPaintBuffer
{
    PersistentPaintStamp persistentPaintStamps[];
};

layout(location = 0) in vec3 fragWorldPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUv;
layout(location = 3) in vec3 fragLocalPosition;
layout(location = 4) in vec3 fragLocalNormal;
layout(location = 5) in vec4 fragShadowPosition0;
layout(location = 6) in vec4 fragShadowPosition1;
layout(location = 7) in float fragViewDepth;

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

vec3 applyPaintSplats(vec3 baseAlbedo, vec3 worldPosition, vec3 normal)
{
    vec3 painted = baseAlbedo;
    int splatCount = int(uniforms.paintSplatCounts.x);
    for (int i = 0; i < splatCount; ++i)
    {
        vec3 center = uniforms.paintSplatPositions[i].xyz;
        float radius = uniforms.paintSplatPositions[i].w;
        if (radius <= 0.0001)
        {
            continue;
        }

        vec3 toFrag = worldPosition - center;
        float dist = length(toFrag);
        if (dist >= radius)
        {
            continue;
        }

        vec3 splatNormal = normalize(uniforms.paintSplatNormals[i].xyz);
        float facing = dot(normal, splatNormal);
        if (facing <= 0.35)
        {
            continue;
        }

        float radial = 1.0 - dist / radius;
        float noise = 0.5 + 0.5 * sin(dot(worldPosition, vec3(8.7, 5.3, 6.1)) + float(i) * 3.17);
        float mask = smoothstep(0.18, 0.85, radial + (noise - 0.5) * 0.35);
        mask *= smoothstep(0.35, 0.65, facing);
        painted = mix(painted, uniforms.paintSplatColors[i].rgb, clamp(mask, 0.0, 1.0));
    }
    return painted;
}

vec3 applyPersistentPaint(vec3 baseAlbedo, vec3 localPosition, vec3 localNormal)
{
    vec3 painted = baseAlbedo;
    uint end = drawPush.persistentPaintOffset + drawPush.persistentPaintCount;
    for (uint i = drawPush.persistentPaintOffset; i < end; ++i)
    {
        PersistentPaintStamp stamp = persistentPaintStamps[i];
        float radius = stamp.positionRadius.w;
        if (radius <= 0.0001)
        {
            continue;
        }

        vec3 toFrag = localPosition - stamp.positionRadius.xyz;
        float dist = length(toFrag);
        if (dist >= radius)
        {
            continue;
        }

        vec3 stampNormal = normalize(stamp.normalSeed.xyz);
        float facing = dot(localNormal, stampNormal);
        if (facing <= 0.2)
        {
            continue;
        }

        float radial = 1.0 - dist / radius;
        float noise = 0.5 + 0.5 * sin(dot(localPosition, vec3(9.3, 4.7, 7.1)) + stamp.normalSeed.w * 1.371);
        float mask = smoothstep(0.12, 0.92, radial + (noise - 0.5) * 0.42);
        mask *= smoothstep(0.2, 0.6, facing);
        mask *= clamp(stamp.colorOpacity.w, 0.0, 1.0);
        painted = mix(painted, stamp.colorOpacity.rgb, clamp(mask, 0.0, 1.0));
    }
    return painted;
}

void main()
{
    vec3 normal = normalize(fragNormal);
    vec3 albedo = texture(albedoTexture, fragUv).rgb;
    albedo = applyPersistentPaint(albedo, fragLocalPosition, normalize(fragLocalNormal));
    albedo = applyPaintSplats(albedo, fragWorldPosition, normal);
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
        if (((drawPush.pointLightMask >> i) & 1u) == 0u)
        {
            continue;
        }
        vec3 lightOffset = uniforms.lightPositions[i].xyz - fragWorldPosition;
        float distanceToLight = length(lightOffset);
        float lightRange = uniforms.lightPositions[i].w;
        if (lightRange <= 0.0001 || distanceToLight >= lightRange)
        {
            continue;
        }
        vec3 lightDir = distanceToLight > 0.0001 ? lightOffset / distanceToLight : vec3(0.0, 1.0, 0.0);
        float attenuation = 1.0 - distanceToLight / lightRange;
        attenuation = max(attenuation, 0.0);
        attenuation *= attenuation;
        float ndotl = max(dot(normal, lightDir), 0.0);
        lighting += albedo * uniforms.lightColors[i].rgb * ndotl * attenuation;
    }

    int shadowedSpotLightCount = int(uniforms.sceneLightCounts.y);
    for (int i = 0; i < shadowedSpotLightCount; ++i)
    {
        if (((drawPush.shadowedSpotLightMask >> i) & 1u) == 0u)
        {
            continue;
        }
        vec3 lightOffset = uniforms.shadowedSpotLightPositions[i].xyz - fragWorldPosition;
        float distanceToLight = length(lightOffset);
        if (distanceToLight <= 0.0001 || distanceToLight >= uniforms.shadowedSpotLightPositions[i].w)
        {
            continue;
        }

        vec3 lightDir = lightOffset / distanceToLight;
        vec3 spotDir = normalize(uniforms.shadowedSpotLightDirections[i].xyz);
        float coneCos = dot(-lightDir, spotDir);
        float innerCos = uniforms.shadowedSpotLightParams[i].x;
        float outerCos = uniforms.shadowedSpotLightParams[i].y;
        float coneAttenuation = clamp((coneCos - outerCos) / max(innerCos - outerCos, 0.0001), 0.0, 1.0);
        if (coneAttenuation <= 0.0)
        {
            continue;
        }

        float rangeAttenuation = 1.0 - distanceToLight / uniforms.shadowedSpotLightPositions[i].w;
        rangeAttenuation = max(rangeAttenuation, 0.0);
        rangeAttenuation *= rangeAttenuation;
        float ndotl = max(dot(normal, lightDir), 0.0);
        vec4 shadowPosition = uniforms.shadowViewProj[2 + i] * vec4(fragWorldPosition, 1.0);
        float spotShadow = shadowPositionCovered(shadowPosition, 2 + i)
            ? sampleShadow(2 + i, shadowPosition, normal, lightDir)
            : 1.0;
        lighting += albedo *
            uniforms.shadowedSpotLightColors[i].rgb *
            uniforms.shadowedSpotLightColors[i].w *
            ndotl *
            rangeAttenuation *
            coneAttenuation *
            spotShadow;
    }

    int sceneLightCount = int(uniforms.sceneLightCounts.x);
    for (int i = 0; i < sceneLightCount; ++i)
    {
        if (((drawPush.spotLightMask >> i) & 1u) == 0u)
        {
            continue;
        }
        vec3 lightOffset = uniforms.spotLightPositions[i].xyz - fragWorldPosition;
        float distanceToLight = length(lightOffset);
        if (distanceToLight <= 0.0001 || distanceToLight >= uniforms.spotLightPositions[i].w)
        {
            continue;
        }

        vec3 lightDir = lightOffset / distanceToLight;
        vec3 spotDir = normalize(uniforms.spotLightDirections[i].xyz);
        float coneCos = dot(-lightDir, spotDir);
        float innerCos = uniforms.spotLightParams[i].x;
        float outerCos = uniforms.spotLightParams[i].y;
        float coneAttenuation = clamp((coneCos - outerCos) / max(innerCos - outerCos, 0.0001), 0.0, 1.0);
        if (coneAttenuation <= 0.0)
        {
            continue;
        }

        float rangeAttenuation = 1.0 - distanceToLight / uniforms.spotLightPositions[i].w;
        rangeAttenuation = max(rangeAttenuation, 0.0);
        rangeAttenuation *= rangeAttenuation;
        float ndotl = max(dot(normal, lightDir), 0.0);
        lighting += albedo *
            uniforms.spotLightColors[i].rgb *
            uniforms.spotLightColors[i].w *
            ndotl *
            rangeAttenuation *
            coneAttenuation;
    }

    vec3 color = lighting;
    color = color / (color + vec3(1.0));

    outColor = vec4(color, 1.0);
}
