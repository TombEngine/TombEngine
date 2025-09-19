#include "./Math.hlsli"

#define WIBBLE_FRAME_PERIOD 64.0f

static float DecodeGlow(uint effect)
{
    return float((effect >> 0) & 63) / 63.0f;
}

static float DecodeMove(uint effect)
{
    return float((effect >> 6) & 63) / 63.0f;
}

static float DecodeWeight(uint effect)
{
    return float((effect >> 12) & 1);
}

float Wibble(uint effect, int hash)
{
    float glow = DecodeGlow(effect);
    float move = DecodeMove(effect);
    
    float enabled = (glow + move) > 0.0f ? 1.0f : 0.0f;
    
    float phaseOffset = (float) ((uint) hash & 255) * (1.0f / 256.0f);
    
    float phase = frac(InterpolatedFrame / WIBBLE_FRAME_PERIOD + phaseOffset);

    float wibble = sin(phase * PI2); // [-1,1]
    return wibble * enabled;
}


float3 Glow(float3 color, uint effect, float wibble)
{
    float glow = DecodeGlow(effect);
    
    float shouldGlow = step(0.0f, glow);
    float intensity = glow * lerp(-0.5f, 1.0f, wibble * 0.5f + 0.5f);
    float3 glowEffect = float3(intensity, intensity, intensity) * shouldGlow;

    return color + glowEffect;
}

float3 Move(float3 position, uint effect, float wibble)
{
    float move = DecodeMove(effect);
    float weight = DecodeWeight(effect);
    
    float shouldMove = step(0.0f, move) * step(0.0f, weight);
    float offset = wibble * move * weight * 128.0f * shouldMove;

    return position + float3(0.0f, offset, 0.0f);
}