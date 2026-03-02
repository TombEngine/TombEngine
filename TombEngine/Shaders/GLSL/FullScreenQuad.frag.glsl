#version 450 core

layout(binding = 0) uniform sampler2D Texture;

in VS_OUT {
    vec2 UV;
    vec4 Color;
} fs_in;

layout(location = 0) out vec4 FragColor;

void main()
{
    vec4 output_color = texture(Texture, fs_in.UV);
    vec4 colorMul = min(fs_in.Color, 1.0);
    output_color = output_color * colorMul;

    FragColor = output_color;
}
