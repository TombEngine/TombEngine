#ifndef MATH
#define MATH

#include "VertexInput.glsl"

#define PI      3.1415926535897932384626433832795028841971693993751058209749445923
#define PI2     6.2831853071795864769252867665590057683943387987502116419498891846
#define EPSILON 1e-6
#define OCTAVES 6

#define LT_SUN          0
#define LT_POINT        1
#define LT_SPOT         2
#define LT_SHADOW       3

#define LT_MASK         0xFFFF
#define LT_MASK_SUN     (1 << (31 - LT_SUN))
#define LT_MASK_POINT   (1 << (31 - LT_POINT))
#define LT_MASK_SPOT    (1 << (31 - LT_SPOT))
#define LT_MASK_SHADOW  (1 << (31 - LT_SHADOW))

#define SHADOWABLE_MASK (1 << 16)

#define MAX_DECALS_PER_ROOM 48
#define MAX_LIGHTS_PER_ROOM 48
#define MAX_LIGHTS_PER_ITEM 8
#define MAX_FOG_BULBS   32
#define SPEC_FACTOR 64

#define MAX_BONES 32
#define MAX_BONE_WEIGHTS 4

struct ShaderLight
{
    vec3 Position;
    uint Type;
    vec3 Color;
    float Intensity;
    vec3 Direction;
    float In;
    float Out;
    float InRange;
    float OutRange;
    int ShaderLight_Padding0;
};

struct ShaderFogBulb
{
    vec3 Position;
    float Density;
    vec3 Color;
    float SquaredRadius;
    vec3 FogBulbToCameraVector;
    float SquaredCameraToFogBulbDistance;
    vec4 ShaderFogBulb_Padding0;
};

struct ShaderDecal
{
    vec3 Position;
    uint Pattern;
    //----------
    float Radius;
    float Opacity;
    int ShaderDecal_Padding0;
    int ShaderDecal_Padding1;
};

float Luma(vec3 color)
{
    // Use Rec.709 trichromat formula to get perceptive luma value.
    return (color.x * 0.2126) + (color.y * 0.7152) + (color.z * 0.0722);
}

vec3 Screen(vec3 ambient, vec3 tint)
{
    float luma = Luma(tint);

    vec3 multiplicative = ambient * tint;
    vec3 additive = ambient + tint;

    float r = mix(multiplicative.x, additive.x, luma);
    float g = mix(multiplicative.y, additive.y, luma);
    float b = mix(multiplicative.z, additive.z, luma);

    return vec3(r, g, b);
}

float LinearizeDepth(float depth, float nearPlane, float farPlane)
{
    return ((nearPlane * 2.0) / (farPlane + nearPlane - (depth * (farPlane - nearPlane))));
}

vec3 Mod289_vec3(vec3 x)
{
    return (x - floor(x * (1.0 / 289.0)) * 289.0);
}

vec4 Mod289(vec4 x)
{
    return (x - floor(x * (1.0 / 289.0)) * 289.0);
}

vec4 Permute(vec4 x)
{
    return Mod289(((x * 34.0) + 10.0) * x);
}

vec4 TaylorInvSqrt(vec4 r)
{
    return (1.79284291400159 - 0.85373472095314 * r);
}

float SimplexNoise(vec3 v)
{
    const vec2 C = vec2(1.0 / 6.0, 1.0 / 3.0);
    const vec4 D = vec4(0.0, 0.5, 1.0, 2.0);

    // First corner.
    vec3 i = floor(v + dot(v, C.yyy));
    vec3 x0 = v - i + dot(i, C.xxx);

    // Other corners.
    vec3 g = step(x0.yzx, x0.xyz);
    vec3 l = 1.0 - g;
    vec3 i1 = min(g.xyz, l.zxy);
    vec3 i2 = max(g.xyz, l.zxy);

    // x0 = x0 - 0.0 + 0.0 * C.xxx;
    // x1 = x0 - i1  + 1.0 * C.xxx;
    // x2 = x0 - i2  + 2.0 * C.xxx;
    // x3 = x0 - 1.0 + 3.0 * C.xxx;
    vec3 x1 = x0 - i1 + C.xxx;
    vec3 x2 = x0 - i2 + C.yyy; // 2.0 * C.x = 1 / 3 = C.y
    vec3 x3 = x0 - D.yyy;      // -1.0 + 3.0 * C.x = -0.5 = -D.y

    // Permutations.
    i = Mod289_vec3(i);
    vec4 p = Permute(
        Permute(
            Permute(
                i.z + vec4(0.0, i1.z, i2.z, 1.0)) +
            i.y + vec4(0.0, i1.y, i2.y, 1.0)) +
        i.x + vec4(0.0, i1.x, i2.x, 1.0));

    // Gradients: 7x7 points over square, mapped onto octahedron.
    // Ring size 17*17 = 289 is close to multiple of 49 (49 * 6 = 294).
    float n_ = 0.142857142857; // 1.0/7.0
    vec3 ns = n_ * D.wyz - D.xzx;

    vec4 j = p - 49.0 * floor(p * ns.z * ns.z); // mod(p, 7 * 7)

    vec4 x_ = floor(j * ns.z);
    vec4 y_ = floor(j - 7.0 * x_); // mod(j, N)

    vec4 x = x_ * ns.x + ns.yyyy;
    vec4 y = y_ * ns.x + ns.yyyy;
    vec4 h = 1.0 - abs(x) - abs(y);

    vec4 b0 = vec4(x.x, x.y, y.x, y.y);
    vec4 b1 = vec4(x.z, x.w, y.z, y.w);

    vec4 s0 = floor(b0) * 2.0 + 1.0;
    vec4 s1 = floor(b1) * 2.0 + 1.0;
    vec4 sh = -step(h, vec4(0.0, 0.0, 0.0, 0.0));

    vec4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
    vec4 a1 = b1.xzyw + s1.xzyw * sh.zzww;

    vec3 p0 = vec3(a0.xy, h.x);
    vec3 p1 = vec3(a0.zw, h.y);
    vec3 p2 = vec3(a1.xy, h.z);
    vec3 p3 = vec3(a1.zw, h.w);

    // Normalize gradients.
    vec4 norm = TaylorInvSqrt(vec4(dot(p0, p0), dot(p1, p1), dot(p2, p2), dot(p3, p3)));
    p0 *= norm.x;
    p1 *= norm.y;
    p2 *= norm.z;
    p3 *= norm.w;

    // Mix final noise value.
    vec4 m = max(0.5 - vec4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
    m = m * m;

    return 105.0 * dot(m * m, vec4(dot(p0, x0), dot(p1, x1), dot(p2, x2), dot(p3, x3)));
}

vec2 Gradient2D(vec2 p)
{
    float angle = 2.0 * 3.14159265359 * fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
    return normalize(vec2(cos(angle), sin(angle)));
}

float NebularNoise(vec2 uv, float frequency, float amplitude, float persistence)
{
    float noiseValue = 0.0;
    float scale = 1.0;
    float range = 1.0;
    vec2 p = uv * frequency;

    vec2 floorP = floor(p);
    vec2 fracP = fract(p);
    vec4 v = vec4(0.0, 1.0, 0.0, 1.0);

    vec2 bl = floorP;
    vec2 br = bl + vec2(1.0, 0.0);
    vec2 tl = bl + vec2(0.0, 1.0);
    vec2 tr = bl + vec2(1.0, 1.0);

    vec2 v1 = fracP;
    vec2 v2 = v1 - vec2(1.0, 0.0);
    vec2 v3 = v1 - vec2(0.0, 1.0);
    vec2 v4 = v1 - vec2(1.0, 1.0);

    float dot1 = dot(Gradient2D(bl), v1);
    float dot2 = dot(Gradient2D(br), v2);
    float dot3 = dot(Gradient2D(tl), v3);
    float dot4 = dot(Gradient2D(tr), v4);

    float u = smoothstep(0.0, 1.0, v1.x);
    vec2 a = mix(vec2(dot1, dot3), vec2(dot2, dot4), u);

    float k = smoothstep(0.0, 1.0, v1.y);
    float b = mix(a.x, a.y, k);

    noiseValue += b * scale / range;

    range += amplitude;
    scale *= persistence;
    p *= 2.0;

    return noiseValue;
}

float Noise(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

float PerlinNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);

    float a = Noise(i);
    float b = Noise(i + vec2(1.0, 0.0));
    float c = Noise(i + vec2(0.0, 1.0));
    float d = Noise(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

vec4 AnimatedNebula(vec2 uv, float time)
{
    // Scale UV coordinates.
    vec2 scaledUV = uv;

    // Apply horizontal mirroring.
    scaledUV.y = abs(scaledUV.y);

    // Generate noise value using Perlin noise.
    float noiseValue = PerlinNoise(scaledUV.xy + time);

    // Apply turbulence to noise value.
    float turbulence = noiseValue * 0.9;
    vec2 distortedUV = scaledUV + turbulence;

    // Interpolate between two main colors based on distorted UV coordinates.
    vec4 color1 = vec4(0.2, 0.4, 0.8, 0.0);
    vec4 color2 = vec4(0.6, 0.1, 0.5, 0.0);
    vec4 interpolatedColor = mix(color1, color2, clamp(distortedUV.y, 0.0, 1.0));

    // Add some brightness flickering based on noise value.
    interpolatedColor *= 1.0 + noiseValue * 1.3;
    return interpolatedColor;
}

float RandomValue(vec2 input_val)
{
    return fract(sin(dot(input_val, vec2(12.9898, 78.233))) * 43758.5453123);
}

float Noise2D(vec2 input_val)
{
    vec2 i = floor(input_val);
    vec2 f = fract(input_val);

    // Four corners in 2D of a tile
    float a = RandomValue(i);
    float b = RandomValue(i + vec2(1.0, 0.0));
    float c = RandomValue(i + vec2(0.0, 1.0));
    float d = RandomValue(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(a, b, u.x) +
        (c - a) * u.y * (1.0 - u.x) +
        (d - b) * u.x * u.y;
}

float FractalNoise(vec2 input_val)
{
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 0.0;

    // Octave loop.
    for (int i = 0; i < OCTAVES; i++)
    {
        value += amplitude * Noise2D(input_val);
        input_val *= 2.0;
        amplitude *= 0.5;
    }

    return value;
}

// Matrix (3x3) to Quaternion
vec4 RotationMatrixToQuaternion(mat3 m)
{
    vec4 q;
    float trace = m[0].x + m[1].y + m[2].z;

    bool tracePositive = trace > 0.0;
    float s0 = sqrt(trace + 1.0) * 2.0;
    vec4 q0 = vec4(
        (m[2].y - m[1].z) / s0,
        (m[0].z - m[2].x) / s0,
        (m[1].x - m[0].y) / s0,
        0.25 * s0
    );

    bool cond1 = (m[0].x > m[1].y) && (m[0].x > m[2].z);
    float s1 = sqrt(1.0 + m[0].x - m[1].y - m[2].z) * 2.0;
    vec4 q1 = vec4(
        0.25 * s1,
        (m[0].y + m[1].x) / s1,
        (m[0].z + m[2].x) / s1,
        (m[2].y - m[1].z) / s1
    );

    bool cond2 = m[1].y > m[2].z;
    float s2 = sqrt(1.0 + m[1].y - m[0].x - m[2].z) * 2.0;
    vec4 q2 = vec4(
        (m[0].y + m[1].x) / s2,
        0.25 * s2,
        (m[1].z + m[2].y) / s2,
        (m[0].z - m[2].x) / s2
    );

    float s3 = sqrt(1.0 + m[2].z - m[0].x - m[1].y) * 2.0;
    vec4 q3 = vec4(
        (m[0].z + m[2].x) / s3,
        (m[1].z + m[2].y) / s3,
        0.25 * s3,
        (m[1].x - m[0].y) / s3
    );

    q = tracePositive ? q0 : (cond1 ? q1 : (cond2 ? q2 : q3));
    return normalize(q);
}

// Quaternion to Matrix (3x3)
mat3 QuaternionToRotationMatrix(vec4 q)
{
    float x2 = q.x + q.x, y2 = q.y + q.y, z2 = q.z + q.z;
    float xx = q.x * x2,   yy = q.y * y2,  zz = q.z * z2;
    float xy = q.x * y2,   xz = q.x * z2,  yz = q.y * z2;
    float wx = q.w * x2,   wy = q.w * y2,  wz = q.w * z2;

    mat3 m;
    m[0] = vec3(1.0 - (yy + zz), xy - wz, xz + wy);
    m[1] = vec3(xy + wz, 1.0 - (xx + zz), yz - wx);
    m[2] = vec3(xz - wy, yz + wx, 1.0 - (xx + yy));
    return m;
}

// Note: BlendBoneMatrices requires vertex attributes and bone uniform data.
// In GLSL, vertex attributes are passed as individual inputs, not a struct.
// This function takes the bone indices/weights directly.
mat4 BlendBoneMatrices(uvec4 boneIndex, uvec4 boneWeight, mat4 bones[MAX_BONES], bool onlyRotation)
{
    if (onlyRotation)
    {
        vec4 blendedQuat = vec4(0, 0, 0, 0);
        vec3 blendedTranslation = vec3(0, 0, 0);

        for (int i = 0; i < MAX_BONE_WEIGHTS; ++i)
        {
            float w = float(boneWeight[i]) / 255.0;
            int index = int(boneIndex[i]);
            mat4 bone = bones[index];

            mat3 rot = mat3(bone);
            vec4 q = RotationMatrixToQuaternion(rot);

            // Ensure shortest path for interpolation (flip if dot < 0)
            float dotPrev = dot(blendedQuat, q);
            q *= sign(dotPrev + 1e-5);

            blendedQuat += q * w;
        }

        blendedQuat = normalize(blendedQuat);
        mat3 finalRot = QuaternionToRotationMatrix(blendedQuat);

        mat4 result = mat4(
            vec4(finalRot[0], 0.0),
            vec4(finalRot[1], 0.0),
            vec4(finalRot[2], 0.0),
            vec4(bones[boneIndex[0]][3].xyz, 1.0)
        );

        return result;
    }
    else
    {
        float totalWeight = 0.0;
        for (int i = 0; i < MAX_BONE_WEIGHTS; ++i)
            totalWeight += float(boneWeight[i]) / 255.0;

        // Avoid divide-by-zero and excessive weights
        if (totalWeight < EPSILON)
            return bones[boneIndex[0]];

        mat4 blendedMatrix = mat4(0.0);

        for (int i = 0; i < MAX_BONE_WEIGHTS; ++i)
        {
            float w = (float(boneWeight[i]) / 255.0) / totalWeight; // Normalize weights
            blendedMatrix += bones[boneIndex[i]] * w;
        }

        // Remove artifacts
        blendedMatrix[0].w = 0.0;
        blendedMatrix[1].w = 0.0;
        blendedMatrix[2].w = 0.0;
        blendedMatrix[3].w = 1.0;

        return blendedMatrix;
    }
}

vec3 UnpackNormalMap(vec4 n)
{
    n = n * 2.0 - 1.0;
    n.z = clamp(1.0 - dot(n.xy, n.xy), 0.0, 1.0);
    return n.xyz;
}

float Gaussian(float x, float sigma)
{
    // exp( -x^2 / (2*sigma^2) )
    return exp(-(x * x) / (2.0 * sigma * sigma));
}

vec3 SafeNormalize(vec3 v)
{
    float l2 = dot(v, v);
    float invLen = inversesqrt(max(l2, EPSILON));
    float mask = clamp(l2 / (l2 + EPSILON), 0.0, 1.0);
    return v * invLen * mask;
}

vec2 GetSamplePosition(vec4 projectedPosition)
{
    vec2 samplePosition;
    samplePosition = projectedPosition.xy / projectedPosition.w;
    samplePosition = samplePosition * 0.5 + 0.5;
    return samplePosition;
}

#endif // MATH
