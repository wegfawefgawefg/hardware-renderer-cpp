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

layout(binding = 1) uniform sampler2D albedoTexture;
layout(binding = 2) uniform sampler2D shadowMaps[6];
layout(binding = 5) uniform sampler2D normalTexture;
layout(std430, binding = 6) readonly buffer ProcCityDynamicLights
{
    vec4 positionRange[];
} procCityLightPositions;
layout(std430, binding = 7) readonly buffer ProcCityDynamicLightIndices
{
    uint lightIndices[];
} procCityLightIndices;
layout(std430, binding = 8) readonly buffer ProcCityLightTiles
{
    uvec4 tileHeaders[];
} procCityLightTiles;
layout(std430, binding = 9) readonly buffer ProcCityTileLightIndices
{
    uint tileLightIndices[];
} procCityTileLightIndices;

layout(location = 0) in vec3 fragWorldPosition;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec2 fragUv;
layout(location = 3) in vec3 fragLocalPosition;
layout(location = 4) in vec3 fragLocalNormal;
layout(location = 5) in vec4 fragShadowPosition0;
layout(location = 6) in vec4 fragShadowPosition1;
layout(location = 7) in float fragViewDepth;
layout(location = 8) flat in uvec4 fragLightMasks;
layout(location = 9) flat in uvec4 fragBatchInfo;

layout(location = 0) out vec4 outColor;

const uint kFeatureSunLighting = 1u << 2;
const uint kFeatureSunShadows = 1u << 3;
const uint kFeatureLocalLights = 1u << 4;
const uint kFeatureLocalLightShadows = 1u << 5;
const uint kFeatureProcCityDynamicLights = 1u << 6;
const uint kFeatureProcCityTiledLights = 1u << 7;

bool shaderFeatureEnabled(uint featureBit)
{
    uint featureMask = uint(uniforms.shadowParams.w + 0.5);
    return (featureMask & featureBit) != 0u;
}

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

vec3 sampleMappedNormal(vec3 baseNormal, vec3 worldPosition, vec2 uv)
{
    vec3 map = texture(normalTexture, uv).xyz * 2.0 - 1.0;
    bool flipNormalY = (fragLightMasks.w & 1u) != 0u;
    if (flipNormalY)
    {
        map.y = -map.y;
    }
    float strength = max(uniforms.surfaceMaskParamsB.x, 0.0);
    if (map.z <= -0.999)
    {
        return normalize(baseNormal);
    }

    vec3 dp1 = dFdx(worldPosition);
    vec3 dp2 = dFdy(worldPosition);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);
    float det = duv1.x * duv2.y - duv1.y * duv2.x;
    if (abs(det) <= 1e-6)
    {
        return normalize(baseNormal);
    }

    vec3 tangent = normalize((dp1 * duv2.y - dp2 * duv1.y) / det);
    vec3 bitangent = normalize((-dp1 * duv2.x + dp2 * duv1.x) / det);
    vec3 normal = normalize(baseNormal);
    mat3 tbn = mat3(tangent, bitangent, normal);
    vec3 mapped = normalize(tbn * normalize(vec3(map.xy * max(strength, 0.0), map.z)));
    return normalize(mix(normal, mapped, clamp(strength, 0.0, 1.0)));
}

vec2 generatedQuadMaterialUv(vec3 localPosition, vec3 localNormal)
{
    float quadSize = max(uniforms.surfaceMaskParamsB.y, 0.05);
    vec3 n = abs(normalize(localNormal));
    if (n.y >= n.x && n.y >= n.z)
    {
        return vec2(localPosition.x, -localPosition.z) / quadSize + vec2(0.5);
    }
    if (n.x >= n.z)
    {
        return vec2(localPosition.z, -localPosition.y) / quadSize + vec2(0.5);
    }
    return vec2(localPosition.x, -localPosition.y) / quadSize + vec2(0.5);
}

vec3 visualizeUv(vec2 uv)
{
    float scale = max(uniforms.paintSplatCounts.z, 1.0);
    int mode = int(uniforms.paintSplatCounts.w + 0.5);
    vec2 scaledUv = uv * scale;
    vec2 tiledUv = fract(scaledUv);
    if (mode == 1)
    {
        return vec3(tiledUv.x);
    }
    if (mode == 2)
    {
        return vec3(tiledUv.y);
    }
    if (mode == 3)
    {
        return vec3(tiledUv, 0.0);
    }
    if (mode == 4)
    {
        return vec3(clamp(uv.x, 0.0, 1.0));
    }
    if (mode == 5)
    {
        return vec3(clamp(uv.y, 0.0, 1.0));
    }
    if (mode == 6)
    {
        bool outOfRange = uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0;
        vec2 fracUv = fract(abs(uv));
        vec3 base = vec3(fracUv, 1.0 - 0.5 * (fracUv.x + fracUv.y));
        return outOfRange ? mix(base, vec3(1.0, 0.0, 1.0), 0.8) : base * 0.35;
    }
    vec2 gridDist = abs(tiledUv - 0.5);
    float gridLine = 1.0 - smoothstep(0.46, 0.50, max(gridDist.x, gridDist.y));
    vec3 uvColor = vec3(tiledUv, 1.0 - 0.5 * (tiledUv.x + tiledUv.y));
    uvColor = mix(uvColor, vec3(0.02), gridLine * 0.85);
    return uvColor;
}

vec4 procCityLightColorIntensity(uint lightIndex)
{
    return procCityLightPositions.positionRange[lightIndex * 2u + 1u];
}

vec4 procCityLightPositionRange(uint lightIndex)
{
    return procCityLightPositions.positionRange[lightIndex * 2u + 0u];
}

uvec4 procCityTileHeader(uint tileIndex)
{
    return procCityLightTiles.tileHeaders[tileIndex];
}

void main()
{
    vec2 materialUv = fragUv;
    if ((fragLightMasks.w & 4u) != 0u)
    {
        materialUv = generatedQuadMaterialUv(fragLocalPosition, fragLocalNormal);
    }

    int materialDebugMode = int(uniforms.paintSplatCounts.y + 0.5);
    if (materialDebugMode == 2)
    {
        outColor = vec4(visualizeUv(materialUv), 1.0);
        return;
    }

    vec4 albedoSample = texture(albedoTexture, materialUv);
    bool alphaBlend = (fragLightMasks.w & 2u) != 0u;
    if ((!alphaBlend && albedoSample.a <= 0.1) ||
        (alphaBlend && albedoSample.a <= 0.01))
    {
        discard;
    }
    if (materialDebugMode == 1)
    {
        outColor = vec4(albedoSample.rgb, 1.0);
        return;
    }

    vec3 albedo = albedoSample.rgb;
    vec3 normal = sampleMappedNormal(fragNormal, fragWorldPosition, materialUv);
    vec3 lighting = albedo * uniforms.ambientColor.rgb;

    if (shaderFeatureEnabled(kFeatureSunLighting))
    {
        vec3 sunDir = normalize(-uniforms.sunDirection.xyz);
        float sunShadow = 1.0;
        if (shaderFeatureEnabled(kFeatureSunShadows))
        {
            bool covered0 = shadowPositionCovered(fragShadowPosition0, 0);
            bool covered1 = shadowPositionCovered(fragShadowPosition1, 1);
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
        }
        lighting += albedo * uniforms.sunColor.rgb * max(dot(normal, sunDir), 0.0) * sunShadow;
    }

    if (shaderFeatureEnabled(kFeatureLocalLights))
    {
        for (int i = 0; i < 4; ++i)
        {
            if (((fragLightMasks.x >> i) & 1u) == 0u)
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
            lighting += albedo * uniforms.lightColors[i].rgb * max(dot(normal, lightDir), 0.0) * attenuation;
        }

        int shadowedSpotLightCount = int(uniforms.sceneLightCounts.y);
        for (int i = 0; i < shadowedSpotLightCount; ++i)
        {
            if (((fragLightMasks.z >> i) & 1u) == 0u)
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
            float spotShadow = 1.0;
            if (shaderFeatureEnabled(kFeatureLocalLightShadows))
            {
                vec4 shadowPosition = uniforms.shadowViewProj[2 + i] * vec4(fragWorldPosition, 1.0);
                spotShadow = shadowPositionCovered(shadowPosition, 2 + i)
                    ? sampleShadow(2 + i, shadowPosition, normal, lightDir)
                    : 1.0;
            }
            lighting += albedo *
                uniforms.shadowedSpotLightColors[i].rgb *
                uniforms.shadowedSpotLightColors[i].w *
                max(dot(normal, lightDir), 0.0) *
                rangeAttenuation *
                coneAttenuation *
                spotShadow;
        }

        int sceneLightCount = int(uniforms.sceneLightCounts.x);
        for (int i = 0; i < sceneLightCount; ++i)
        {
            if (((fragLightMasks.y >> i) & 1u) == 0u)
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
            lighting += albedo *
                uniforms.spotLightColors[i].rgb *
                uniforms.spotLightColors[i].w *
                max(dot(normal, lightDir), 0.0) *
                rangeAttenuation *
                coneAttenuation;
        }
    }

    if (shaderFeatureEnabled(kFeatureProcCityDynamicLights))
    {
        bool tiledLights = shaderFeatureEnabled(kFeatureProcCityTiledLights);
        uint localOffset = fragBatchInfo.x;
        uint localCount = fragBatchInfo.y;
        uint tileOffset = localOffset;
        uint tileCount = localCount;
        if (tiledLights)
        {
            const uint tileSize = 32u;
            uvec2 tileCoord = uvec2(gl_FragCoord.xy) / tileSize;
            uint tileCountX = max(uint(uniforms.surfaceMaskParamsB.z + 0.5), 1u);
            uint tileCountY = max(uint(uniforms.surfaceMaskParamsB.w + 0.5), 1u);
            tileCoord.x = min(tileCoord.x, tileCountX - 1u);
            tileCoord.y = min(tileCoord.y, tileCountY - 1u);
            uint tileIndex = tileCoord.y * tileCountX + tileCoord.x;
            uvec4 header = procCityTileHeader(tileIndex);
            tileOffset = header.x;
            tileCount = header.y;
        }

        for (uint i = 0u; i < tileCount; ++i)
        {
            uint lightIndex = tiledLights
                ? procCityTileLightIndices.tileLightIndices[tileOffset + i]
                : procCityLightIndices.lightIndices[localOffset + i];
            vec4 positionRange = procCityLightPositionRange(lightIndex);
            vec4 colorIntensity = procCityLightColorIntensity(lightIndex);
            vec3 lightOffset = positionRange.xyz - fragWorldPosition;
            float distanceToLight = length(lightOffset);
            if (distanceToLight <= 0.0001 || distanceToLight >= positionRange.w)
            {
                continue;
            }

            vec3 lightDir = lightOffset / distanceToLight;
            float rangeAttenuation = 1.0 - distanceToLight / positionRange.w;
            rangeAttenuation = max(rangeAttenuation, 0.0);
            rangeAttenuation *= rangeAttenuation;
            lighting += albedo *
                colorIntensity.rgb *
                colorIntensity.w *
                max(dot(normal, lightDir), 0.0) *
                rangeAttenuation;
        }
    }

    vec3 color = lighting / (lighting + vec3(1.0));
    outColor = vec4(color, alphaBlend ? albedoSample.a : 1.0);
}
