#ifndef BLENDINGSHADER
#define BLENDINGSHADER

#include "CBBlending.glsl"
#include "Math.glsl"

#define ALPHATEST_NONE          0
#define ALPHATEST_GREATER_THAN  1
#define ALPHATEST_LESS_THAN     2

#define BLENDMODE_OPAQUE      0
#define BLENDMODE_ALPHATEST   1
#define BLENDMODE_ADDITIVE    2
#define BLENDMODE_NOZTEST     4
#define BLENDMODE_SUBTRACTIVE 5
#define BLENDMODE_WIREFRAME   6
#define BLENDMODE_EXCLUDE     8
#define BLENDMODE_SCREEN      9
#define BLENDMODE_LIGHTEN     10
#define BLENDMODE_ALPHABLEND  11

#define ZERO       vec3(0.0, 0.0, 0.0)
#define EIGHT_FIVE vec3(0.85, 0.85, 0.85)
#define BLENDING   0.707

bool BlendModeSupportsSSAO()
{
    return (BlendMode == uint(BLENDMODE_OPAQUE) || BlendMode == uint(BLENDMODE_ALPHATEST) || BlendMode == uint(BLENDMODE_ALPHABLEND));
}

#ifdef FRAGMENT_SHADER
void DoAlphaTest(vec4 inputColor)
{
    if (AlphaTest == ALPHATEST_GREATER_THAN && inputColor.w < AlphaThreshold)
    {
        discard;
    }
    else if (AlphaTest == ALPHATEST_LESS_THAN && inputColor.w > AlphaThreshold)
    {
        discard;
    }
    else
    {
        return;
    }
}
#endif

vec4 DoDistanceFogForPixel(vec4 sourceColor, vec4 fogColor, float value)
{
    switch (int(BlendMode))
    {
        case BLENDMODE_ADDITIVE:
        case BLENDMODE_SCREEN:
        case BLENDMODE_LIGHTEN:
            fogColor.xyz *= Luma(sourceColor.xyz);
            break;

        case BLENDMODE_SUBTRACTIVE:
        case BLENDMODE_EXCLUDE:
            fogColor.xyz *= 1.0 - Luma(sourceColor.xyz);
            break;

        case BLENDMODE_ALPHABLEND:
            fogColor.w = sourceColor.w;
            break;

        default:
            break;
    }

    if (fogColor.w > sourceColor.w)
        fogColor.w = sourceColor.w;

    vec4 result = mix(sourceColor, fogColor, value);
    return result;
}

vec4 DoFogBulbsForPixel(vec4 sourceColor, vec4 fogColor)
{
    switch (int(BlendMode))
    {
        case BLENDMODE_ADDITIVE:
        case BLENDMODE_SCREEN:
        case BLENDMODE_LIGHTEN:
            fogColor.xyz *= Luma(sourceColor.xyz);
            break;

        case BLENDMODE_SUBTRACTIVE:
        case BLENDMODE_EXCLUDE:
            fogColor.xyz *= 1.0 - Luma(sourceColor.xyz);
            break;

        case BLENDMODE_ALPHABLEND:
            fogColor.w = sourceColor.w;
            break;

        default:
            break;
    }

    if (fogColor.w > sourceColor.w)
        fogColor.w = sourceColor.w;

    vec4 result = sourceColor;

    result.xyz += clamp(fogColor.xyz, 0.0, 1.0);

    return result;
}

#endif // BLENDINGSHADER
