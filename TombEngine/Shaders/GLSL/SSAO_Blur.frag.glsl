#version 450 core

#include "Math.glsl"
#include "CBCamera.glsl"
#include "CBPostProcess.glsl"

#define SIGMA 3.0
#define BSIGMA 0.3
#define MSIZE 5

layout(binding = 9) uniform sampler2D SSAOTexture;

in VS_OUT {
    vec2 UV;
} fs_in;

layout(location = 0) out float FragColor;

float normpdf(float x, float sigma)
{
    return 0.39894 * exp(-0.5 * x * x / (sigma * sigma)) / sigma;
}

void main()
{
    vec2 texelSize = TexelSize;
    float result = 0.0;

    const int kernelSize = (MSIZE - 1) / 2;
    float kernel_arr[MSIZE];
    float bZ = 0.0;

    // Create the 1-D kernel
    for (int j = 0; j <= kernelSize; j++)
    {
        float kval = normpdf(float(j), SIGMA);
        kernel_arr[kernelSize + j] = kval;
        kernel_arr[kernelSize - j] = kval;
    }

    float baseColor = texture(SSAOTexture, fs_in.UV).x;
    float bZnorm = 1.0 / normpdf(0.0, BSIGMA);

    // Read out the texels
    for (int i = -kernelSize; i <= kernelSize; i++)
    {
        for (int j = -kernelSize; j <= kernelSize; j++)
        {
            vec2 offset = vec2(float(i), float(j)) * texelSize;
            float color = texture(SSAOTexture, fs_in.UV + offset).x;

            float gfactor = kernel_arr[kernelSize + j] * kernel_arr[kernelSize + i];
            float bfactor = normpdf(color - baseColor, BSIGMA) * bZnorm * gfactor;
            bZ += bfactor;

            result += bfactor * color;
        }
    }

    FragColor = result / bZ;
}
