#version 450 core

layout(binding = 0) uniform sampler2D ColorTexture;

in VS_OUT {
    vec2 UV;
    vec4 Color;
    vec4 PositionCopy;
} fs_in;

layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor = texture(ColorTexture, fs_in.UV);
}
