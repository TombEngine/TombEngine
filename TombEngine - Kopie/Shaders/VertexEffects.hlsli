#include "./Math.hlsli"

#define WIBBLE_FRAME_PERIOD 64.0f

float Wibble(float3 effect, int hash)
{
    float shouldWibble = step(0.0f, effect.x + effect.y);
    float wibble = sin((((Frame + hash) % 256) / WIBBLE_FRAME_PERIOD) * PI2);

    return wibble * shouldWibble;
}

float3 Glow(float3 color, float3 effect, float wibble)
{
    float shouldGlow = step(0.0f, effect.x);
    float intensity = effect.x * lerp(-0.5f, 1.0f, wibble * 0.5f + 0.5f);
    float3 glowEffect = float3(intensity, intensity, intensity) * shouldGlow;

    return color + glowEffect;
}

float3 Move(float3 position, float3 effect, float wibble)
{
    float weight = effect.z;
    float shouldMove = step(0.0f, effect.y) * step(0.0f, weight);
    float offset = wibble * effect.y * weight * 128.0f * shouldMove;

    return position + float3(0.0f, offset, 0.0f);
}