#version 460

layout(binding = 0) uniform sampler2D sceneTexture;

layout(location = 0) in vec2 fragUv;
layout(location = 0) out vec4 outColor;

vec3 sampleScene(vec2 uv)
{
    return texture(sceneTexture, vec2(uv.x, 1.0 - uv.y)).rgb;
}

vec3 brightPass(vec3 color)
{
    float brightness = max(max(color.r, color.g), color.b);
    float mask = smoothstep(0.72, 1.10, brightness);
    return color * mask;
}

void main()
{
    ivec2 sizeI = textureSize(sceneTexture, 0);
    vec2 texel = 1.0 / vec2(max(sizeI.x, 1), max(sizeI.y, 1));

    vec3 base = sampleScene(fragUv);
    vec3 bloom = vec3(0.0);
    float weight = 0.0;

    const vec2 offsets[12] = vec2[](
        vec2(-1.0, 0.0), vec2(1.0, 0.0),
        vec2(0.0, -1.0), vec2(0.0, 1.0),
        vec2(-2.0, 0.0), vec2(2.0, 0.0),
        vec2(0.0, -2.0), vec2(0.0, 2.0),
        vec2(-1.5, -1.5), vec2(1.5, -1.5),
        vec2(-1.5, 1.5), vec2(1.5, 1.5)
    );
    const float weights[12] = float[](0.13, 0.13, 0.13, 0.13, 0.08, 0.08, 0.08, 0.08, 0.06, 0.06, 0.06, 0.06);

    bloom += brightPass(base) * 0.18;
    weight += 0.18;

    for (int i = 0; i < 12; ++i)
    {
        vec3 tap = sampleScene(fragUv + offsets[i] * texel * 3.0);
        bloom += brightPass(tap) * weights[i];
        weight += weights[i];
    }

    bloom /= max(weight, 0.0001);
    vec3 color = base + bloom * 1.65;
    outColor = vec4(color, 1.0);
}
