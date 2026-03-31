#version 460

layout(binding = 0) uniform sampler2D sceneTexture;

layout(location = 0) in vec2 fragUv;
layout(location = 1) in vec4 fragColor;
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
    vec3 base = sampleScene(fragUv);
    outColor = vec4(base, 1.0);
}
