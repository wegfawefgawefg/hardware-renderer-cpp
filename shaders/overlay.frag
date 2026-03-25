#version 460

layout(binding = 0) uniform sampler2D overlayTexture;

layout(location = 0) in vec2 fragUv;
layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(overlayTexture, vec2(fragUv.x, 1.0 - fragUv.y));
}
