#version 460

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main()
{
    vec3 color = fragColor / (fragColor + vec3(1.0));
    outColor = vec4(color, 0.95);
}
