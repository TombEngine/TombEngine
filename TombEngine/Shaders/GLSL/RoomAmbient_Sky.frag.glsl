#version 450 core

#include "CBCamera.glsl"
#include "CLightBufferSky.glsl"
#include "CBSky.glsl"
#include "Blending.glsl"
#include "Math.glsl"
#include "AnimatedTextures.glsl"

layout(binding = 0) uniform sampler2D Texture;

in VS_OUT {
    vec2 UV;
    vec4 Color;
    float ClipDepth;
} fs_in;

layout(location = 0) out vec4 FragColor;

void main()
{
    vec2 uv = fs_in.UV;

    if (Animated != 0u && Type == 1u)
        uv = CalculateUVRotate(uv, 0u);

    vec4 output_color = texture(Texture, uv);

    if (fs_in.ClipDepth < 0.0)
        discard;

    DoAlphaTest(output_color);

    vec3 light = clamp(SkyColor.xyz, 0.0, 1.0);
    output_color.xyz *= light;
    output_color.w *= SkyColor.w;

    FragColor = output_color;
}
