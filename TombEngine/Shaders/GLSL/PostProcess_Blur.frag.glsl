#version 450 core

#include "CBPostProcess.glsl"

#define MAX_BLUR_RADIUS 100

layout(binding = 0) uniform sampler2D ColorTexture;

in VS_OUT {
    vec2 UV;
    vec4 Color;
} fs_in;

layout(location = 0) out vec4 FragColor;

void main()
{
    int r = clamp(BlurRadius, 0, MAX_BLUR_RADIUS);

    float w0 = Gaussian(0.0, BlurSigma);
    vec4 color = texture(ColorTexture, fs_in.UV) * w0;
    float weightSum = w0;

    for (int k = 1; k <= MAX_BLUR_RADIUS; ++k)
    {
        if (k > r)
            break;

        float wk = Gaussian(float(k), BlurSigma);

        vec2 off = BlurDirection * TexelSize * float(k);
        vec4 c1 = texture(ColorTexture, fs_in.UV + off);
        vec4 c2 = texture(ColorTexture, fs_in.UV - off);

        color += (c1 + c2) * wk;
        weightSum += 2.0 * wk;
    }

    FragColor = color / weightSum;
}
