#version 450 core

#include "CBPostProcess.glsl"

layout(binding = 0) uniform sampler2D ColorTexture;

in VS_OUT {
    vec2 UV;
    vec4 Color;
} fs_in;

layout(location = 0) out vec4 FragColor;

void main()
{
    vec2 halfDstUV = 0.5 / (vec2(ViewportSize) / DownscaleFactor);

    const float wc = 0.5;
    const float w1 = 0.125;
    const float wd = 0.03125;

    vec2 dx = vec2(halfDstUV.x, 0);
    vec2 dy = vec2(0, halfDstUV.y);

    vec3 c = vec3(0.0);

    // center
    c += textureLod(ColorTexture, fs_in.UV, 0).rgb * wc;

    // N/S/E/W
    c += textureLod(ColorTexture, fs_in.UV + dx, 0).rgb * w1;
    c += textureLod(ColorTexture, fs_in.UV - dx, 0).rgb * w1;
    c += textureLod(ColorTexture, fs_in.UV + dy, 0).rgb * w1;
    c += textureLod(ColorTexture, fs_in.UV - dy, 0).rgb * w1;

    // diagonals
    c += textureLod(ColorTexture, fs_in.UV + dx + dy, 0).rgb * wd;
    c += textureLod(ColorTexture, fs_in.UV + dx - dy, 0).rgb * wd;
    c += textureLod(ColorTexture, fs_in.UV - dx + dy, 0).rgb * wd;
    c += textureLod(ColorTexture, fs_in.UV - dx - dy, 0).rgb * wd;

    FragColor = vec4(c, 1);
}
