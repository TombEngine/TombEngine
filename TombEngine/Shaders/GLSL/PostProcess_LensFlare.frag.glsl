#version 450 core

#include "CBCamera.glsl"
#include "CBPostProcess.glsl"

layout(binding = 0) uniform sampler2D ColorTexture;

in VS_OUT {
    vec2 UV;
    vec4 Color;
} fs_in;

layout(location = 0) out vec4 FragColor;

vec3 LensFlare(vec2 uv, vec2 pos)
{
    float intensity = 0.5;

    vec2 main_vec = uv - pos;
    vec2 uvd = uv * length(uv);

    // Angular and distance calculations
    float ang = atan(main_vec.y, main_vec.x);
    float dist = length(main_vec);
    dist = pow(dist, 0.1);

    // Sunflare effect with rotation and ray length variation
    float f0 = 1.0 / (length(uv - pos) * 35.0 + 1.0);
    f0 = pow(f0, 1.5);

    // Calculate position-based rotation offset
    float rotationOffset = dot(pos, vec2(0.5, 0.5)) * (PI / 2.0);

    // Modify starburst pattern with rotation and variation
    f0 += f0 * (sin((ang + rotationOffset + 1.0 / 18.0) * 8.0) * 0.2 + dist * 0.1 + 0.2);

    // Lensflare glow components
    float f2  = max(1.0 / (1.0 + 32.0 * pow(length(uvd + 0.8  * pos), 1.0)), 0.0) * 0.25;
    float f22 = max(1.0 / (1.0 + 32.0 * pow(length(uvd + 0.85 * pos), 1.0)), 0.0) * 0.23;
    float f23 = max(1.0 / (1.0 + 32.0 * pow(length(uvd + 0.9  * pos), 1.0)), 0.0) * 0.21;

    // Circular lens artifacts
    vec2 uvx = mix(uv, uvd, -0.5);
    float f4  = max(0.01 - pow(length(uvx + 0.4  * pos), 2.4), 0.0) * 9.0;
    float f42 = max(0.01 - pow(length(uvx + 0.45 * pos), 2.4), 0.0) * 7.0;
    float f43 = max(0.01 - pow(length(uvx + 0.5  * pos), 2.4), 0.0) * 6.0;

    // Smaller lens artifacts
    uvx = mix(uv, uvd, -0.4);
    float f5  = max(0.01 - pow(length(uvx + 0.2 * pos), 5.5), 0.0) * 2.0;
    float f52 = max(0.01 - pow(length(uvx + 0.4 * pos), 5.5), 0.0) * 2.0;
    float f53 = max(0.01 - pow(length(uvx + 0.6 * pos), 5.5), 0.0) * 2.0;

    // Symmetric artifacts
    uvx = mix(uv, uvd, -0.5);
    float f6  = max(0.01 - pow(length(uvx - 0.3   * pos), 1.6), 0.0) * 9.0;
    float f62 = max(0.01 - pow(length(uvx - 0.325 * pos), 1.6), 0.0) * 6.0;
    float f63 = max(0.01 - pow(length(uvx - 0.35  * pos), 1.6), 0.0) * 7.0;

    // Sunflare and lensflare outputs
    vec3 sunflare = vec3(f0, f0, f0);
    vec3 lensflare = vec3(
        f2 + f4 + f5 + f6,
        f22 + f42 + f52 + f62,
        f23 + f43 + f53 + f63
    );

    // Subtle anamorphic glare
    float anamorphicScaleX = 1.2; // Horizontal stretch factor
    float anamorphicScaleY = 35.0;  // Vertical compression factor
    vec2 anamorphicOffset = main_vec; // Center at the lensflare source
    float glare = exp(-pow(anamorphicOffset.x * anamorphicScaleX, 2.0) - pow(anamorphicOffset.y * anamorphicScaleY, 2.0));
    vec3 anamorphicGlare = vec3(glare * 0.6, glare * 0.5, glare * 1.0) * 0.05;

    // Combine the effects and adjust intensity
    vec3 c = clamp(sunflare, 0.0, 1.0) * 0.5 + lensflare + anamorphicGlare;
    c = c * 1.3 - vec3(length(uvd) * 0.05, length(uvd) * 0.05, length(uvd) * 0.05);

    return c * intensity;
}

vec3 LensFlareColorCorrection(vec3 color, float factor, float factor2)
{
    float w = color.x + color.y + color.z;
    return mix(color, vec3(w, w, w) * factor, w * factor2);
}

void main()
{
    vec4 color = texture(ColorTexture, fs_in.UV);

    // Reconstruct clip-space position from fullscreen quad vertex position
    vec4 position = vec4(fs_in.UV * 2.0 - 1.0, 0.0, 1.0);
    vec3 totalLensFlareColor = vec3(0.0, 0.0, 0.0);

    for (int i = 0; i < NumLensFlares; i++)
    {
        vec4 lensFlarePosition = vec4(LensFlares[i].Position, 1.0);
        lensFlarePosition = Projection * View * lensFlarePosition;
        lensFlarePosition.xyz /= lensFlarePosition.w;

        vec3 lensFlareColor = max(vec3(0.0, 0.0, 0.0),
            LensFlares[i].Color *
            vec3(4.5, 3.6, 3.6) *
            LensFlare(position.xy, lensFlarePosition.xy));
        lensFlareColor = LensFlareColorCorrection(lensFlareColor, 0.5, 0.1);
        totalLensFlareColor += lensFlareColor;
    }

    color.xyz = mix(color.xyz, color.xyz + totalLensFlareColor, clamp(dot(totalLensFlareColor, vec3(0.5, 0.5, 0.5)), 0.0, 1.0));

    FragColor = color;
}
