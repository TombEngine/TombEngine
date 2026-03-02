#version 450 core

#include "Math.glsl"
#include "CBPostProcess.glsl"

#define FXAA_SPAN_MAX   8.0
#define FXAA_REDUCE_MUL (1.0 / 4.0)
#define FXAA_REDUCE_MIN (1.0 / 64.0)

layout(binding = 0) uniform sampler2D Texture;

in VS_OUT {
    vec2 UV;
    vec4 Color;
} fs_in;

layout(location = 0) out vec4 FragColor;

void main()
{
    vec2 add = 1.0 / vec2(ViewportSize);

    vec3 rgbNW = texture(Texture, fs_in.UV + vec2(-add.x, -add.y)).rgb;
    vec3 rgbNE = texture(Texture, fs_in.UV + vec2( add.x, -add.y)).rgb;
    vec3 rgbSW = texture(Texture, fs_in.UV + vec2(-add.x,  add.y)).rgb;
    vec3 rgbSE = texture(Texture, fs_in.UV + vec2( add.x,  add.y)).rgb;
    vec3 rgbM  = texture(Texture, fs_in.UV).rgb;

    float lumaNW = Luma(rgbNW);
    float lumaNE = Luma(rgbNE);
    float lumaSW = Luma(rgbSW);
    float lumaSE = Luma(rgbSE);
    float lumaM  = Luma(rgbM);

    float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
    float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

    vec2 dir;
    dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
    dir.y =  ((lumaNW + lumaSW) - (lumaNE + lumaSE));

    float dirReduce = max(
        (lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);

    float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);

    dir = min(vec2(FXAA_SPAN_MAX),
        max(vec2(-FXAA_SPAN_MAX), dir * rcpDirMin)) * add;

    vec3 rgbA = (1.0 / 2.0) *
        (texture(Texture, fs_in.UV + dir * (1.0 / 3.0 - 0.5)).rgb +
         texture(Texture, fs_in.UV + dir * (2.0 / 2.0 - 0.5)).rgb);

    vec3 rgbB = rgbA * (1.0 / 2.0) + (1.0 / 4.0) *
        (texture(Texture, fs_in.UV + dir * (0.0 / 3.0 - 0.5)).rgb +
         texture(Texture, fs_in.UV + dir * (3.0 / 3.0 - 0.5)).rgb);

    float lumaB = Luma(rgbB);

    if ((lumaB < lumaMin) || (lumaB > lumaMax))
        FragColor = vec4(rgbA, 1.0);
    else
        FragColor = vec4(rgbB, 1.0);
}
