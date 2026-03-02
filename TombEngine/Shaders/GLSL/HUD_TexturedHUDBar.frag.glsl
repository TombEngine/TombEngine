#version 450 core

#include "Math.glsl"

layout(std140, binding = 11) uniform HUDBarBuffer
{
    vec2 BarStartUV;
    vec2 BarScale;
    //------------
    float Percent;
    int Poisoned;
    int HUDFrame;
    int HUDBarBuffer_Buffer0;
};

layout(binding = 5) uniform sampler2D Texture;

in VS_OUT {
    vec2 UV;
    vec4 Color;
} fs_in;

layout(location = 0) out vec4 FragColor;

vec4 glassOverlay(vec2 UVs, vec4 originalColor)
{
    float y = UVs.y;
    y -= 0.15;
    y = distance(0.1, y);
    y = 1.0 - y;
    y = pow(y, 4.0);
    y = clamp(y, 0.0, 1.0);
    vec4 color = originalColor;
    return clamp(mix(color, (color * 1.6) + vec4(0.4, 0.4, 0.4, 0.0), y), 0.0, 1.0);
}

void main()
{
    if (fs_in.UV.x > Percent)
    {
        discard;
    }
    vec2 uv = vec2((fs_in.UV.x * BarScale.x) + BarStartUV.x, (fs_in.UV.y * BarScale.y) + BarStartUV.y);
    vec4 col = texture(Texture, uv);
    if (Poisoned != 0)
    {
        float factor = sin(((float(HUDFrame % 30) / 30.0)) * PI2) * 0.5 + 0.5;
        col = mix(col, vec4(214.0 / 512.0, 241.0 / 512.0, 18.0 / 512.0, 1.0), factor);
    }

    FragColor = glassOverlay(fs_in.UV, col);
}
