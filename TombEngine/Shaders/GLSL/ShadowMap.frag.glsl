#version 450 core

in VS_OUT {
    vec4 PositionCopy;
    float Depth;
} fs_in;

layout(location = 0) out vec4 FragColor;

void main()
{
    FragColor = vec4(fs_in.PositionCopy.z / fs_in.PositionCopy.w, 0.0, 0.0, 0.0);
}
