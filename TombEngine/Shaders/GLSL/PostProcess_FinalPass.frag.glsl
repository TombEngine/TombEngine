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
    vec4 result = texture(ColorTexture, fs_in.UV);

    vec3 colorMul = min(fs_in.Color.xyz, 1.0);

    float y = gl_FragCoord.y / float(ViewportSize.y);

    if (y > 1.0 - CinematicBarsHeight ||
        y < 0.0 + CinematicBarsHeight)
    {
        result = vec4(0, 0, 0, 1);
    }
    else
    {
        result.xyz = result.xyz * colorMul.xyz * ScreenFadeFactor;
        result.w = 1.0;
    }

    result.xyz = result.xyz * Tint;

    FragColor = result;
}
