#version 450 core

#include "CBPostProcess.glsl"

layout(binding = 0) uniform sampler2D ColorTexture;
layout(binding = 3) uniform sampler2D GlowTexture;

in VS_OUT {
    vec2 UV;
    vec4 Color;
} fs_in;

layout(location = 0) out vec4 FragColor;

vec3 SoftAddBlend(vec3 base, vec3 add)
{
    return 1.0 - (1.0 - base) * (1.0 - add);
}

void main()
{
    vec3 base = texture(ColorTexture, fs_in.UV).rgb;
    vec3 glow = texture(GlowTexture, fs_in.UV).rgb * GlowIntensity;

    vec3 outc = (GlowSoftAdd != 0) ? SoftAddBlend(base, glow)
                                    : clamp(base + glow, 0.0, 1.0);

    FragColor = vec4(outc, 1);
}
