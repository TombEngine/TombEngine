#ifndef VERTEXEFFECTSSHADER
#define VERTEXEFFECTSSHADER

#include "Math.glsl"

#define WIBBLE_FRAME_PERIOD 64.0

float DecodeGlow(uint effect)
{
    return float((effect >> 0u) & 255u) / 255.0;
}

float DecodeMove(uint effect)
{
    return float((effect >> 8u) & 255u) / 255.0;
}

float DecodeWeight(uint effect)
{
    return float((effect >> 24u) & 1u);
}

int DecodeIndexInPoly(uint effect)
{
    return int((effect >> 25u) & 3u);
}

float DecodeSheen(uint effect)
{
    return float((effect >> 16u) & 255u) / 255.0;
}

int DecodeAnimationFrameOffset(uint animationFrameOffsetIndexHash)
{
    return int((animationFrameOffsetIndexHash >> 24u) & 255u);
}

int DecodeHash(uint animationFrameOffsetIndexHash)
{
    return int(animationFrameOffsetIndexHash & 255u);
}

float Wibble(uint effect, int hash)
{
    float glow = DecodeGlow(effect);
    float move = DecodeMove(effect);

    float enabled = (glow + move) > 0.0 ? 1.0 : 0.0;

    float phaseOffset = float(hash) * (1.0 / 256.0);

    float phase = fract(InterpolatedFrame / WIBBLE_FRAME_PERIOD + phaseOffset);

    float wibble = sin(phase * PI2); // [-1,1]
    return wibble * enabled;
}

vec3 Glow(vec3 color, uint effect, float wibble)
{
    float glow = DecodeGlow(effect);

    float shouldGlow = step(0.0, glow);
    float intensity = glow * mix(-0.5, 1.0, wibble * 0.5 + 0.5);
    vec3 glowEffect = vec3(intensity, intensity, intensity) * shouldGlow;

    return color + glowEffect;
}

vec3 Move(vec3 position, uint effect, float wibble)
{
    float move = DecodeMove(effect);
    float weight = DecodeWeight(effect);

    float shouldMove = step(0.0, move) * step(0.0, weight);
    float offset = wibble * move * weight * 128.0 * shouldMove;

    return position + vec3(0.0, offset, 0.0);
}

#endif // VERTEXEFFECTSSHADER
