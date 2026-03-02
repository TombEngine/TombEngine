#version 450 core

layout(location = 0) in vec3 in_Position;
layout(location = 2) in vec2 in_UV;
layout(location = 3) in vec4 in_Color;

out VS_OUT {
    vec2 UV;
    vec4 Color;
    vec4 PositionCopy;
} vs_out;

void main()
{
    gl_Position = vec4(in_Position, 1.0);
    vs_out.UV = vec2(in_UV.x, 1.0 - in_UV.y);
    vs_out.Color = in_Color;
    vs_out.PositionCopy = gl_Position;
}
