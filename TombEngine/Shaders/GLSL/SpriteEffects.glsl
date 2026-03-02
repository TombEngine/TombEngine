#ifndef SPRITEEFFECTSSHADER
#define SPRITEEFFECTSSHADER

#include "Math.glsl"

#define ZERO       vec3(0.0, 0.0, 0.0)
#define EIGHT_FIVE vec3(0.85, 0.85, 0.85)
#define BLENDING   0.707

vec4 DoLaserBarrierEffect(vec3 input_val, vec4 output_val, vec2 uv, float faceFactor, float timeUniform)
{
    vec2 noiseTexture = input_val.xy / uv;
    noiseTexture *= uv.x / uv.y;
    float noiseValue = FractalNoise(noiseTexture * 8.0 - timeUniform);

    vec4 color = output_val;
    float gradL = smoothstep(0.0, 1.0, uv.x);
    float gradR = smoothstep(1.0, 0.0, uv.x);
    float gradT = smoothstep(0.0, 0.25, uv.y);
    float gradB = 1.0 - smoothstep(0.75, 1.0, uv.y);

    float distortion = timeUniform / 1024.0;

    vec3 noisix = vec3(SimplexNoise(
        vec3(SimplexNoise(vec3(input_val.r * distortion, input_val.g, input_val.b)))
    ));
    vec3 shadowx = vec3(SimplexNoise(
        cos(vec3(SimplexNoise(sin(timeUniform + input_val.rgb * 400.0))))
    ));

    noisix.x = noisix.x > 0.9 ? 0.7 : noisix.x;
    noisix.y = noisix.y > 0.9 ? 0.7 : noisix.y;
    noisix.z = noisix.z > 0.9 ? 0.7 : noisix.z;
    color.rgb += noisix + 0.5;
    color.rgb -= noisix + 0.2;

    float frequency = 0.1;
    float amplitude = 0.8;
    float persistence = 0.5;
    float noiseValue2 = 0.0;
    float noiseValue3 = 0.0;

    vec2 uv84 = (uv * 2.4);
    uv84.y = (uv84.y - 1.3);
    uv84.x = (uv84.x / 1.3);
    vec2 uv85 = (uv / 2.4);

    noiseValue2 = AnimatedNebula(uv84, timeUniform * 0.1).x;

    frequency = 2.5;
    amplitude = 0.2;
    persistence = 4.7;

    vec2 uv83 = uv * 8.0;
    uv83.y = (uv.y + (timeUniform * 0.02));
    noiseValue3 = NebularNoise(uv83, frequency, amplitude, persistence);

    noiseValue2 += AnimatedNebula(uv / 2.0, timeUniform * 0.05).x;

    color.rgb *= noiseValue2 + 0.6;
    color.rgb += noiseValue3;
    color.a *= noiseValue + 0.01;

    color.rgb -= shadowx + 0.1;

    color.a *= noiseValue2 + 0.9;
    color.a *= noiseValue3 + 2.0;

    float fade0 = faceFactor * max(0.0, 1.0 - dot(vec2(BLENDING, BLENDING), vec2(gradL, gradT)));
    float fade1 = faceFactor * max(0.0, 1.0 - dot(vec2(BLENDING, BLENDING), vec2(gradL, gradB)));
    float fade2 = faceFactor * max(0.0, 1.0 - dot(vec2(BLENDING, BLENDING), vec2(gradR, gradB)));
    float fade3 = faceFactor * max(0.0, 1.0 - dot(vec2(BLENDING, BLENDING), vec2(gradR, gradT)));

    float fadeL = 1.40 * faceFactor * faceFactor * (1.0 - gradL);
    float fadeB = 2.75 * faceFactor * faceFactor * (1.0 - gradB);
    float fadeR = 1.40 * faceFactor * faceFactor * (1.0 - gradR);
    float fadeT = 2.75 * faceFactor * faceFactor * (1.0 - gradT);

    float fade = max(
        max(max(fade0, fade1), max(fade2, fade3)),
        max(max(fadeL, fadeR), max(fadeB, fadeT)));

    float scale = 1.0 - fade;

    color *= scale;
    float decayFactor = 1.0;
    if (uv.y > 0.5 && uv.y < 1.0)
    {
        decayFactor = uv.y / 2.0;
    }
    if (uv.y < 0.5 && uv.y > 0.0)
    {
        decayFactor = (1.0 - uv.y) / 2.0;
    }
    color *= decayFactor;

    color.rgb = smoothstep(ZERO, EIGHT_FIVE, color.rgb);
    return color;
}

vec4 DoLaserBeamEffect(vec3 input_val, vec4 output_val, vec2 uv, float faceFactor, float timeUniform)
{
    vec2 noiseTexture = input_val.xy / uv;
    noiseTexture *= uv.x / uv.y;
    float noiseValue = FractalNoise(noiseTexture * 0.1 + timeUniform);

    vec4 color = output_val;
    float gradL = smoothstep(0.0, 0.0, uv.x);
    float gradR = smoothstep(0.0, 0.0, uv.x);
    float gradT = smoothstep(0.0, 0.25, uv.y);
    float gradB = 1.0 - smoothstep(0.75, 1.0, uv.y);

    // Stretch UV coordinates.
    float stretchFactor = 0.005;
    uv.x *= stretchFactor;

    float distortion = timeUniform / 1024.0;

    vec3 noisix = vec3(SimplexNoise(
        vec3(SimplexNoise(vec3(input_val.r * distortion, input_val.g, input_val.b)))
    ));
    vec3 shadowx = vec3(SimplexNoise(
        cos(vec3(SimplexNoise(sin(timeUniform + input_val.rgb * 400.0))))
    ));

    noisix.x = noisix.x > 0.9 ? 0.7 : noisix.x;
    noisix.y = noisix.y > 0.9 ? 0.7 : noisix.y;
    noisix.z = noisix.z > 0.9 ? 0.7 : noisix.z;
    color.rgb += noisix + 0.5;
    color.rgb -= noisix + 0.2;

    float frequency = 0.1;
    float amplitude = 0.8;
    float persistence = 0.5;
    float noiseValue2 = 0.0;
    float noiseValue3 = 0.0;

    vec2 uv84 = (uv * 1.4);
    uv84.y = (uv84.y - 1.3);
    uv84.x = (uv84.x / 1.3);
    vec2 uv85 = (uv / 1.4);

    noiseValue2 = AnimatedNebula(uv84, timeUniform * 0.1).x;

    frequency = 2.5;
    amplitude = 0.2;
    persistence = 4.7;

    vec2 uv83 = uv * 6.0;
    uv83.y = (uv.y + (timeUniform * 0.02));
    noiseValue3 = NebularNoise(uv83, frequency, amplitude, persistence);

    noiseValue2 += AnimatedNebula(uv / 2.0, timeUniform * 0.05).x;

    color.rgb *= noiseValue2 + 0.6;
    color.rgb += noiseValue3;
    color.a *= noiseValue + 0.01;

    color.rgb -= shadowx + 0.1;

    color.a *= noiseValue2 + 0.9;
    color.a *= noiseValue3 + 2.0;

    float fade0 = faceFactor * max(0.0, 1.0 - dot(vec2(BLENDING, BLENDING), vec2(gradL, gradT)));
    float fade1 = faceFactor * max(0.0, 1.0 - dot(vec2(BLENDING, BLENDING), vec2(gradL, gradB)));
    float fade2 = faceFactor * max(0.0, 1.0 - dot(vec2(BLENDING, BLENDING), vec2(gradR, gradB)));
    float fade3 = faceFactor * max(0.0, 1.0 - dot(vec2(BLENDING, BLENDING), vec2(gradR, gradT)));

    float fadeL = 0.0; // Fade out height.
    float fadeB = 0.3 * faceFactor * faceFactor * (0.3 - gradB); // Fade out width.
    float fadeR = 0.0; // Fade out height.
    float fadeT = 0.3 * faceFactor * faceFactor * (0.3 - gradT); // Fade out width.

    float fade = max(
        max(max(fade0, fade1), max(fade2, fade3)),
        max(max(fadeL, fadeR), max(fadeB, fadeT)));

    float scale = 1.0 - fade;

    color *= scale;
    float decayFactor = 1.0;
    if (uv.y > 0.5 && uv.y < 1.0)
    {
        decayFactor = uv.y / 2.0;
    }
    if (uv.y < 0.5 && uv.y > 0.0)
    {
        decayFactor = (1.0 - uv.y) / 2.0;
    }
    color *= decayFactor;

    color.rgb *= 0.17; // Reduce brightness.
    color.rgb = smoothstep(ZERO, EIGHT_FIVE, color.rgb);
    return color;
}

#endif // SPRITEEFFECTSSHADER
