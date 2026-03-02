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
    vec4 color = texture(ColorTexture, fs_in.UV);

    vec3 exColor = color.xyz + (1.0 - color.xyz) - 2.0 * color.xyz * (1.0 - color.xyz);
    vec3 result = mix(color.rgb, exColor, EffectStrength);

    FragColor = vec4(result, color.a);
}
