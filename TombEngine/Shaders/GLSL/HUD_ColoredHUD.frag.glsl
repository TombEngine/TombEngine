#version 450 core

in VS_OUT {
    vec2 UV;
    vec4 Color;
} fs_in;

layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor = fs_in.Color;
}
