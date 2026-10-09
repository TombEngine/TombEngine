#ifndef TEN_NOISE
#define TEN_NOISE

// Noise functions of the legacy renderer (Shaders/Math.hlsli), copied unchanged for the RHI shaders.

#define OCTAVES 6

float3 Mod289(float3 x)
{
    return (x - floor(x * (1 / 289.0f)) * 289.0f);
}

float4 Mod289(float4 x)
{
    return (x - floor(x * (1 / 289.0f)) * 289.0f);
}

float4 Permute(float4 x)
{
    return Mod289(((x * 34.0f) + 10.0f) * x);
}

float4 TaylorInvSqrt(float4 r)
{
    return (1.79284291400159f - 0.85373472095314f * r);
}

float SimplexNoise(float3 v)
{ 
    const float2 C = float2(1 / 6.0f, 1 / 3.0f) ;
    const float4 D = float4(0.0f, 0.5f, 1.0f, 2.0f);

    // First corner.
    float3 i = floor(v + dot(v, C.yyy));
    float3 x0 = v - i + dot(i, C.xxx);

    // Other corners.
    float3 g = step(x0.yzx, x0.xyz);
    float3 l = 1.0f - g;
    float3 i1 = min(g.xyz, l.zxy);
    float3 i2 = max(g.xyz, l.zxy);

    // x0 = x0 - 0.0 + 0.0 * C.xxx;
    // x1 = x0 - i1  + 1.0 * C.xxx;
    // x2 = x0 - i2  + 2.0 * C.xxx;
    // x3 = x0 - 1.0 + 3.0 * C.xxx;
    float3 x1 = x0 - i1 + C.xxx;
    float3 x2 = x0 - i2 + C.yyy;// 2.0 * C.x = 1 / 3 = C.y
    float3 x3 = x0 - D.yyy;        // -1.0 + 3.0 * C.x = -0.5 = -D.y

    // Permutations.
    i = Mod289(i); 
    float4 p = Permute(
        Permute(
            Permute(
                i.z + float4(0.0f, i1.z, i2.z, 1.0f)) +
            i.y + float4(0.0f, i1.y, i2.y, 1.0f)) +
        i.x + float4(0.0f, i1.x, i2.x, 1.0f));

    // Gradients: 7x7 points over square, mapped onto octahedron.
    // Ring size 17*17 = 289 is close to multiple of 49 (49 * 6 = 294).
    float n_ = 0.142857142857f; // 1.0/7.0
    float3 ns = n_ * D.wyz - D.xzx;

    float4 j = p - 49.0f * floor(p * ns.z * ns.z);// mod(p, 7 * 7)

    float4 x_ = floor(j * ns.z);
    float4 y_ = floor(j - 7.0f * x_);// mod(j, N)

    float4 x = x_ * ns.x + ns.yyyy;
    float4 y = y_ * ns.x + ns.yyyy;
    float4 h = 1.0f - abs(x) - abs(y);

    float4 b0 = float4(x.x, x.y, y.x, y.y);
    float4 b1 = float4(x.z, x.w, y.z, y.w);

    //float4 s0 = float4(lessThan(b0,0.0))*2.0 - 1.0;
    //float4 s1 = float4(lessThan(b1,0.0))*2.0 - 1.0;
    float4 s0 = floor(b0) * 2.0 + 1.0;
    float4 s1 = floor(b1) * 2.0 + 1.0;
    float4 sh = -step(h, float4(0.0f, 0.0f, 0.0f, 0.0f));

    float4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
    float4 a1 = b1.xzyw + s1.xzyw * sh.zzww;

    float3 p0 = float3(a0.xy, h.x);
    float3 p1 = float3(a0.zw, h.y);
    float3 p2 = float3(a1.xy, h.z);
    float3 p3 = float3(a1.zw, h.w);

    // Normalize gradients.
    float4 norm = TaylorInvSqrt(float4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;

    // Mix final noise value.
    float4 m = max(0.5 - float4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
    m = m * m;

    return 105.0 * dot(m*m, float4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}

float2 Gradient2D(float2 p)
{
    float angle = 2 * 3.14159265359f * frac(sin(dot(p, float2(12.9898f, 78.233f))) * 43758.5453f);
    return normalize(float2(cos(angle), sin(angle)));
}

float NebularNoise(float2 uv, float frequency, float amplitude, float persistence)
{
    float noiseValue = 0.0f;
    float scale = 1.0f;
    float range = 1.0f;
    float2 p = uv * frequency;

    float2 floorP = floor(p);
    float2 fracP = frac(p);
    float4 v = float4(0.0f, 1.0f, 0.0f, 1.0f);

    float2 bl = floorP;
    float2 br = bl + float2(1.0f, 0.0f);
    float2 tl = bl + float2(0.0f, 1.0f);
    float2 tr = bl + float2(1.0f, 1.0f);

    float2 v1 = fracP;
    float2 v2 = v1 - float2(1.0f, 0.0f);
    float2 v3 = v1 - float2(0.0f, 1.0f);
    float2 v4 = v1 - float2(1.0f, 1.0f);

    float dot1 = dot(Gradient2D(bl), v1);
    float dot2 = dot(Gradient2D(br), v2);
    float dot3 = dot(Gradient2D(tl), v3);
    float dot4 = dot(Gradient2D(tr), v4);

    float u = smoothstep(0.0f, 1.0f, v1.x);
    float2 a = lerp(float2(dot1, dot3), float2(dot2, dot4), u);

    float k = smoothstep(0.0f, 1.0f, v1.y);
    float b = lerp(a.x, a.y, k);

    noiseValue += b * scale / range;

    range += amplitude;
    scale *= persistence;
    p *= 2.0f;

    return noiseValue;
}

float Noise(float2 p)
{
    return frac(sin(dot(p, float2(12.9898f, 78.233f))) * 43758.5453f);
}

float PerlinNoise(float2 p)
{
    float2 i = floor(p);
    float2 f = frac(p);

    float a = Noise(i);
    float b = Noise(i + float2(1.0f, 0.0f));
    float c = Noise(i + float2(0.0f, 1.0f));
    float d = Noise(i + float2(1.0, 1.0f));

    float2 u = f * f * (3.0f - 2.0f * f);

    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

float4 AnimatedNebula(float2 uv, float time)
{
    // Scale UV coordinates.
    float2 scaledUV = uv;

    // Apply horizontal mirroring.
    scaledUV.y = abs(scaledUV.y);

    // Generate noise value using Perlin noise.
    float noiseValue = PerlinNoise(scaledUV.xy + time);

    // Apply turbulence to noise value.
    float turbulence = noiseValue * 0.9;
    float2 distortedUV = scaledUV + turbulence;

    // Interpolate between two main colors based on distorted UV coordinates.
    float4 color1 = float4(0.2f, 0.4f, 0.8f, 0.0f);
    float4 color2 = float4(0.6f, 0.1f, 0.5f, 0.0f);
    float4 interpolatedColor = lerp(color1, color2, saturate(distortedUV.y));

    // Add some brightness flickering based on noise value.
    interpolatedColor *= 1.0f + noiseValue * 1.3f;
    return interpolatedColor;
}

float RandomValue(float2 input)
{
    return frac(sin(dot(input, float2(12.9898, 78.233))) * 43758.5453123);
}

float Noise2D(float2 input)
{
    float2 i = floor(input);
    float2 f = frac(input);

    // Four corners in 2D of a tile
    float a = RandomValue(i);
    float b = RandomValue(i + float2(1.0, 0.0));
    float c = RandomValue(i + float2(0.0, 1.0));
    float d = RandomValue(i + float2(1.0, 1.0));

    float2 u = f * f * (3.0 - 2.0 * f);

    return lerp(a, b, u.x) +
        (c - a) * u.y * (1.0 - u.x) +
        (d - b) * u.x * u.y;
}

float FractalNoise(float2 input)
{
    float value = 0.0;
    float amplitude = .5;
    float frequency = 0.;
    
    // Octave loop.
    for (int i = 0; i < OCTAVES; i++)
    {
        value += amplitude * Noise2D(input);
        input *= 2.0;
        amplitude *= 0.5;
    }

    return value;
}

#endif // TEN_NOISE
