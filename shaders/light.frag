#version 460

layout(location = 0) in vec3 fragColor;
layout(location = 0) out vec4 outColor;

void main()
{
    vec2 centered = gl_PointCoord * 2.0 - 1.0;
    float radius2 = dot(centered, centered);
    if (radius2 > 1.0)
    {
        discard;
    }

    float radius = sqrt(radius2);
    float glow = smoothstep(1.0, 0.10, radius);
    vec3 color = fragColor / (fragColor + vec3(1.0));
    outColor = vec4(color, glow);
}
