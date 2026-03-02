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

    float luma = Luma(color.rgb);
    vec3 result = mix(color.rgb, vec3(luma, luma, luma), EffectStrength);

    FragColor = vec4(result, color.a);
}
