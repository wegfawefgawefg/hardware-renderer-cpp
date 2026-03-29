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
layout(binding = 3) uniform sampler2D paintTexture;
layout(binding = 4) uniform sampler2D effectPatternTexture;
layout(binding = 5) uniform sampler2D normalTexture;

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
    uint materialFlags;
} drawPush;

layout(location = 0) out vec4 outColor;

struct SurfaceMaskEffects
{
    vec3 albedo;
    vec3 emissive;
    float wetness;
    float visibility;
};

float hash31(vec3 p)
{
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float noise3(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float n000 = hash31(i + vec3(0.0, 0.0, 0.0));
    float n100 = hash31(i + vec3(1.0, 0.0, 0.0));
    float n010 = hash31(i + vec3(0.0, 1.0, 0.0));
    float n110 = hash31(i + vec3(1.0, 1.0, 0.0));
    float n001 = hash31(i + vec3(0.0, 0.0, 1.0));
    float n101 = hash31(i + vec3(1.0, 0.0, 1.0));
    float n011 = hash31(i + vec3(0.0, 1.0, 1.0));
    float n111 = hash31(i + vec3(1.0, 1.0, 1.0));

    float nx00 = mix(n000, n100, f.x);
    float nx10 = mix(n010, n110, f.x);
    float nx01 = mix(n001, n101, f.x);
    float nx11 = mix(n011, n111, f.x);
    float nxy0 = mix(nx00, nx10, f.y);
    float nxy1 = mix(nx01, nx11, f.y);
    return mix(nxy0, nxy1, f.z);
}

float fbm3(vec3 p)
{
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 4; ++i)
    {
        value += noise3(p) * amplitude;
        p = p * 2.07 + vec3(17.1, 9.2, 13.7);
        amplitude *= 0.5;
    }
    return value;
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

vec3 sampleMappedNormal(vec3 baseNormal, vec3 worldPosition, vec2 uv)
{
    vec3 map = texture(normalTexture, uv).xyz * 2.0 - 1.0;
    bool flipNormalY = (drawPush.materialFlags & 1u) != 0u;
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

vec3 sampleVanishRippedAlbedo(vec2 uv, vec3 localPosition)
{
    float time = uniforms.cameraPosition.w;
    float vanish = clamp(texture(paintTexture, uv).a, 0.0, 1.0);
    if (vanish <= 0.001)
    {
        return texture(albedoTexture, uv).rgb;
    }

    float staticNoise = fbm3(localPosition * 8.0 + vec3(uv * 31.0, time * 9.0));
    float scan = sin(uv.y * 180.0 + time * 18.0 + localPosition.x * 11.0);
    float wobble = sin(time * 7.5 + uv.y * 26.0 + localPosition.y * 5.0) * 0.5 +
                   sin(time * 12.0 - uv.x * 19.0 + localPosition.z * 4.0) * 0.5;
    float splitStrength = max(uniforms.surfaceMaskParamsA.x, 0.0);
    float jitterStrength = max(uniforms.surfaceMaskParamsA.y, 0.0);
    float staticStrength = max(uniforms.surfaceMaskParamsA.z, 0.0);
    float split = vanish * vanish * (0.0015 + staticNoise * 0.0065) * splitStrength;
    vec2 jitter = vec2((scan * 0.5 + wobble * 0.5) * split * 3.2, sin(time * 15.0 + uv.x * 60.0) * split * 0.9);
    jitter *= jitterStrength;
    vec2 rgbAxis = normalize(vec2(0.9, 0.35) + vec2(staticNoise - 0.5, wobble * 0.35));
    vec2 rgbOffset = rgbAxis * split;

    vec2 uvR = uv + jitter + rgbOffset;
    vec2 uvG = uv + jitter * 0.6;
    vec2 uvB = uv + jitter - rgbOffset;

    vec3 ripped;
    ripped.r = texture(albedoTexture, uvR).r;
    ripped.g = texture(albedoTexture, uvG).g;
    ripped.b = texture(albedoTexture, uvB).b;

    float whiteStatic = smoothstep(0.78, 0.97, staticNoise) * vanish * 0.55 * staticStrength;
    ripped = mix(ripped, vec3(0.92, 0.96, 1.0), whiteStatic);
    return ripped;
}

SurfaceMaskEffects applySurfaceMasks(vec3 baseAlbedo, vec2 uv, vec3 localPosition)
{
    float time = uniforms.cameraPosition.w;
    vec4 masks = texture(paintTexture, uv);
    float grime = clamp(masks.r, 0.0, 1.0);
    float glow = clamp(masks.g, 0.0, 1.0);
    float wetness = clamp(masks.b, 0.0, 1.0);
    float vanish = clamp(masks.a, 0.0, 1.0);

    vec3 albedo = baseAlbedo;
    float luma = dot(albedo, vec3(0.299, 0.587, 0.114));
    vec3 desaturated = mix(albedo, vec3(luma), 0.68);

    vec3 grimeSpace = localPosition * 3.4 + vec3(uv * 9.0, 2.0);
    float grimeNoise = fbm3(grimeSpace);
    float grimeBlobs = smoothstep(0.32, 0.78, grimeNoise);
    float grimeStreaks = smoothstep(0.38, 0.86, fbm3(localPosition * vec3(1.8, 6.8, 1.8) + vec3(0.0, uv.y * 17.0, uv.x * 8.0)));
    float grimeCrust = smoothstep(0.46, 0.90, fbm3(localPosition * 8.4 + vec3(uv * 21.0, 7.0)));
    float grimeSpeckle = smoothstep(0.55, 0.93, fbm3(localPosition * 14.0 + vec3(uv * 37.0, 5.0)));
    float grimeMask = clamp(grime * mix(grimeBlobs, grimeStreaks, 0.25) * mix(0.95, 1.65, grimeCrust) * mix(1.00, 1.35, grimeSpeckle), 0.0, 1.0);
    vec3 grimeTint = mix(vec3(0.020, 0.016, 0.012), vec3(0.115, 0.082, 0.042), grimeCrust);
    vec3 grimeLayer = mix(desaturated * 0.035, grimeTint, 0.84);
    albedo = mix(albedo, grimeLayer, clamp(grimeMask * 1.38, 0.0, 1.0));

    float wetNoise = smoothstep(0.26, 0.90, fbm3(localPosition * 5.7 + vec3(uv * 19.0, 4.0 + time * 0.18)));
    vec2 dripUvA = uv * vec2(2.8, 4.8) + vec2(localPosition.x * 0.15, -time * 0.08 + localPosition.y * 0.08);
    vec2 dripUvB = uv * vec2(4.2, 7.5) + vec2(1.7, -time * 0.14);
    float dripA = texture(effectPatternTexture, dripUvA).r;
    float dripB = texture(effectPatternTexture, dripUvB).g;
    float wetRipples = 0.5 + 0.5 * sin(uv.x * 41.0 + uv.y * 19.0 + time * 5.5 + localPosition.y * 7.0);
    wetRipples *= 0.5 + 0.5 * sin(uv.y * 33.0 - time * 4.8 + localPosition.x * 5.0);
    wetRipples = smoothstep(0.18, 0.88, wetRipples);
    float dripMask = smoothstep(0.25, 0.92, mix(dripA, dripB, 0.45));
    float wetMask = clamp(wetness * mix(0.75, 1.35, wetNoise) * mix(0.70, 1.65, dripMask) * mix(0.80, 1.25, wetRipples), 0.0, 1.0);
    vec3 wetTint = vec3(0.36, 0.48, 0.78);
    albedo = mix(albedo, albedo * wetTint * 0.86, clamp(wetMask * 0.95, 0.0, 1.0));

    float dissolveNoise = fbm3(localPosition * 4.8 + vec3(uv * 23.0, 7.0));
    float dissolveThreshold = 1.0 - vanish;
    float visibility = 1.0 - smoothstep(dissolveThreshold - 0.09, dissolveThreshold + 0.05, dissolveNoise);
    float dissolveEdge = smoothstep(0.02, 0.18, visibility) * (1.0 - smoothstep(0.18, 0.34, visibility));
    float edgeGlowStrength = max(uniforms.surfaceMaskParamsA.w, 0.0);

    SurfaceMaskEffects effects;
    effects.albedo = albedo;
    float glowPulse = 0.65 + 0.35 * fbm3(localPosition * 6.5 + vec3(uv * 25.0, 13.0));
    float glowMask = clamp(glow * mix(0.75, 1.30, glowPulse), 0.0, 1.0);
    vec3 glowColor = mix(vec3(0.18, 1.75, 0.48), vec3(0.62, 2.60, 0.78), glowPulse);
    effects.emissive = glowColor * (glowMask * glowMask * 8.5);
    effects.emissive += vec3(3.6, 1.4, 0.35) * vanish * dissolveEdge * 1.6 * edgeGlowStrength;
    effects.emissive += vec3(0.16, 0.28, 0.55) * wetMask * dripMask * 0.65;
    effects.wetness = wetMask;
    effects.visibility = visibility;
    return effects;
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

void main()
{
    vec3 normal = sampleMappedNormal(fragNormal, fragWorldPosition, fragUv);
    vec4 albedoSample = texture(albedoTexture, fragUv);
    bool alphaBlend = (drawPush.materialFlags & 2u) != 0u;
    if ((!alphaBlend && albedoSample.a <= 0.1) ||
        (alphaBlend && albedoSample.a <= 0.01))
    {
        discard;
    }
    vec3 albedo = albedoSample.rgb;
    int materialDebugMode = int(uniforms.paintSplatCounts.y + 0.5);
    if (materialDebugMode == 2)
    {
        outColor = vec4(visualizeUv(fragUv), 1.0);
        return;
    }
    if (materialDebugMode == 1)
    {
        outColor = vec4(albedo, 1.0);
        return;
    }
    albedo = sampleVanishRippedAlbedo(fragUv, fragLocalPosition);
    SurfaceMaskEffects maskEffects = applySurfaceMasks(albedo, fragUv, fragLocalPosition);
    float outAlpha = alphaBlend ? albedoSample.a * maskEffects.visibility : 1.0;
    if (maskEffects.visibility <= 0.01 || outAlpha <= 0.01)
    {
        discard;
    }
    albedo = maskEffects.albedo;
    albedo = applyPaintSplats(albedo, fragWorldPosition, normal);
    vec3 ambient = albedo * uniforms.ambientColor.rgb;

    vec3 lighting = ambient;
    vec3 sunDir = normalize(-uniforms.sunDirection.xyz);
    vec3 viewDir = normalize(uniforms.cameraPosition.xyz - fragWorldPosition);
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
    vec3 sunHalfDir = normalize(sunDir + viewDir);
    float sunFresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 4.0);
    float sunSpec = pow(max(dot(normal, sunHalfDir), 0.0), mix(72.0, 5.0, maskEffects.wetness));
    lighting += uniforms.sunColor.rgb * sunShadow * sunSpec * maskEffects.wetness * (0.45 + sunFresnel * 1.55);

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
        vec3 halfDir = normalize(lightDir + viewDir);
        float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 4.0);
        float spec = pow(max(dot(normal, halfDir), 0.0), mix(84.0, 5.0, maskEffects.wetness));
        lighting += uniforms.lightColors[i].rgb * spec * attenuation * maskEffects.wetness * (0.20 + fresnel * 0.95);
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
        vec3 halfDir = normalize(lightDir + viewDir);
        float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 4.0);
        float spec = pow(max(dot(normal, halfDir), 0.0), mix(84.0, 5.0, maskEffects.wetness));
        lighting += uniforms.shadowedSpotLightColors[i].rgb *
            uniforms.shadowedSpotLightColors[i].w *
            spec *
            rangeAttenuation *
            coneAttenuation *
            spotShadow *
            maskEffects.wetness * (0.24 + fresnel * 1.05);
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
        vec3 halfDir = normalize(lightDir + viewDir);
        float fresnel = pow(1.0 - max(dot(normal, viewDir), 0.0), 4.0);
        float spec = pow(max(dot(normal, halfDir), 0.0), mix(84.0, 5.0, maskEffects.wetness));
        lighting += uniforms.spotLightColors[i].rgb *
            uniforms.spotLightColors[i].w *
            spec *
            rangeAttenuation *
            coneAttenuation *
            maskEffects.wetness * (0.20 + fresnel * 0.95);
    }

    vec3 color = mix(uniforms.clearColor.rgb, lighting + maskEffects.emissive, maskEffects.visibility);
    color = color / (color + vec3(1.0));

    outColor = vec4(color, outAlpha);
}
