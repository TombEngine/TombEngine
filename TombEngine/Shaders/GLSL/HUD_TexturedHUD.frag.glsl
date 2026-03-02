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

void main()
{
    vec2 uv = vec2((fs_in.UV.x * BarScale.x) + BarStartUV.x, (fs_in.UV.y * BarScale.y) + BarStartUV.y);
    vec4 output_color = texture(Texture, uv);
    FragColor = output_color;
}
