#ifndef MATH
#define MATH

#include "./VertexInput.hlsli"

#define PI		3.1415926535897932384626433832795028841971693993751058209749445923
#define PI2		6.2831853071795864769252867665590057683943387987502116419498891846
#define EPSILON 1e-38
#define OCTAVES 6

#define LT_SUN			0
#define LT_POINT		1
#define LT_SPOT			2
#define LT_SHADOW		3

#define LT_MASK			0xFFFF
#define LT_MASK_SUN		(1 << (31 - LT_SUN))
#define LT_MASK_POINT	(1 << (31 - LT_POINT))
#define LT_MASK_SPOT	(1 << (31 - LT_SPOT))
#define LT_MASK_SHADOW	(1 << (31 - LT_SHADOW))

#define SHADOWABLE_MASK (1 << 16)

#define MAX_LIGHTS_PER_ROOM	48
#define MAX_LIGHTS_PER_ITEM	8
#define MAX_FOG_BULBS	32
#define SPEC_FACTOR 64

#define MAX_BONES 32
#define MAX_BONE_WEIGHTS 4

struct ShaderLight
{
	float3 Position;
	unsigned int Type;
	float3 Color;
	float Intensity;
	float3 Direction;
	float In;
	float Out;
	float InRange;
	float OutRange;
	float Padding;
};

struct ShaderFogBulb
{
	float3 Position;
	float Density;
	float3 Color;
	float SquaredRadius;
	float3 FogBulbToCameraVector;
	float SquaredCameraToFogBulbDistance;
	float4 Padding2;
};

float Luma(float3 color)
{
	// Use Rec.709 trichromat formula to get perceptive luma value.
	return float((color.x * 0.2126f) + (color.y * 0.7152f) + (color.z * 0.0722f));
}

float3 Screen(float3 ambient, float3 tint)
{
	float luma = Luma(tint);

	float3 multiplicative = ambient * tint;
	float3 additive = ambient + tint;

	float r = lerp(multiplicative.x, additive.x, luma);
	float g = lerp(multiplicative.y, additive.y, luma);
	float b = lerp(multiplicative.z, additive.z, luma);

	return float3(r, g, b);
}

float LinearizeDepth(float depth, float nearPlane, float farPlane)
{
	return ((nearPlane * 2) / (farPlane + nearPlane - (depth * (farPlane - nearPlane))));
}

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
	float3 x3 = x0 - D.yyy;		// -1.0 + 3.0 * C.x = -0.5 = -D.y

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

float4 snoise(float3 v)
{ 

  float2 C = float2(1.0 / 6.0, 1.0 / 3.0);

    // First corner
    float3 i  = floor(v + dot(v, float3(C.yyy)));
    float3 x0 = v   - i + dot(i, float3(C.xxx));

    // Other corners
    float3 g = step(x0.yzx, x0.xyz);
    float3 l = 1.0 - g;
    float3 i1 = min(g.xyz, l.zxy);
    float3 i2 = max(g.xyz, l.zxy);

    float3 x1 = x0 - i1 + C.x;
    float3 x2 = x0 - i2 + C.y;
    float3 x3 = x0 - 0.5;

    // Permutations
    float4 p =
      Permute(Permute(Permute(i.z + float4(0.0, i1.z, i2.z, 1.0))
                            + i.y + float4(0.0, i1.y, i2.y, 1.0))
                            + i.x + float4(0.0, i1.x, i2.x, 1.0));

    // Gradients: 7x7 points over a square, mapped onto an octahedron.
    // The ring size 17*17 = 289 is close to a multiple of 49 (49*6 = 294)
    float4 j = p - 49.0 * floor(p / 49.0);  // mod(p,7*7)

    float4 x_ = floor(j / 7.0);
    float4 y_ = floor(j - 7.0 * x_); 

    float4 x = (x_ * 2.0 + 0.5) / 7.0 - 1.0;
    float4 y = (y_ * 2.0 + 0.5) / 7.0 - 1.0;

    float4 h = 1.0 - abs(x) - abs(y);

    float4 b0 = float4(x.xy, y.xy);
    float4 b1 = float4(x.zw, y.zw);

    float4 s0 = floor(b0) * 2.0 + 1.0;
    float4 s1 = floor(b1) * 2.0 + 1.0;
    float4 sh = -step(h, float4(0.0f, 0.0f, 0.0f, 0.0f));

    float4 a0 = b0.xzyw + s0.xzyw * sh.xxyy;
    float4 a1 = b1.xzyw + s1.xzyw * sh.zzww;

    float3 g0 = float3(a0.xy, h.x);
    float3 g1 = float3(a0.zw, h.y);
    float3 g2 = float3(a1.xy, h.z);
    float3 g3 = float3(a1.zw, h.w);

    // Compute noise and gradient at P
    float4 m = max(0.6 - float4(dot(x0, x0), dot(x1, x1), dot(x2, x2), dot(x3, x3)), 0.0);
    float4 m2 = m * m;
    float4 m3 = m2 * m;
    float4 m4 = m2 * m2;
    float3 grad =
      -6.0 * m3.x * x0 * dot(x0, g0) + m4.x * g0 +
      -6.0 * m3.y * x1 * dot(x1, g1) + m4.y * g1 +
      -6.0 * m3.z * x2 * dot(x2, g2) + m4.z * g2 +
      -6.0 * m3.w * x3 * dot(x3, g3) + m4.w * g3;
    float4 px = float4(dot(x0, g0), dot(x1, g1), dot(x2, g2), dot(x3, g3));
    return 42.0 * float4(grad, dot(m4, px));	
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

float3 NormalNoise(float3 v, float3 i, float3 n)
{
	// Resulting vector.
	float3 r = float3(.0f,.0f,.0f);

	// Interpolates lerp(v,i,dot(n,i))
	//v, which is the reference of the pixel and
	//i, a vector perturbed with SimpleNoise
	//the threshold dot(n,i) is based on a dot product between
	//n, the normal and each tuple from it (xy, xz and yz)
	//and the same for the vector from the noise
	r.x = lerp(v.x, i.x, dot(n.xy,i.xy))*.9f;
	r.y = lerp(v.y, i.y, dot(n.xz,i.xz))*.75f;
	r.z = lerp(v.z, i.z, dot(n.yz,i.yz))*.8f;

	// c = threshold based on tuples of reference pixel.
	float c = dot(v.xy,v.yz);

	// Return perturbed pixel based on reference pixel and resulting vector with threshold c attenuated by 2/5 times.
	return lerp(v, r, c * 0.3f);
}

// Matrix (3x3) to Quaternion
float4 RotationMatrixToQuaternion(float3x3 m)
{
	float4 q;
	float trace = m[0].x + m[1].y + m[2].z;

	bool tracePositive = trace > 0.0f;
	float s0 = sqrt(trace + 1.0f) * 2.0f;
	float4 q0 = float4(
		(m[2].y - m[1].z) / s0,
		(m[0].z - m[2].x) / s0,
		(m[1].x - m[0].y) / s0,
		0.25f * s0
	);

	bool cond1 = (m[0].x > m[1].y) && (m[0].x > m[2].z);
	float s1 = sqrt(1.0f + m[0].x - m[1].y - m[2].z) * 2.0f;
	float4 q1 = float4(
		0.25f * s1,
		(m[0].y + m[1].x) / s1,
		(m[0].z + m[2].x) / s1,
		(m[2].y - m[1].z) / s1
	);

	bool cond2 = m[1].y > m[2].z;
	float s2 = sqrt(1.0f + m[1].y - m[0].x - m[2].z) * 2.0f;
	float4 q2 = float4(
		(m[0].y + m[1].x) / s2,
		0.25f * s2,
		(m[1].z + m[2].y) / s2,
		(m[0].z - m[2].x) / s2
	);

	float s3 = sqrt(1.0f + m[2].z - m[0].x - m[1].y) * 2.0f;
	float4 q3 = float4(
		(m[0].z + m[2].x) / s3,
		(m[1].z + m[2].y) / s3,
		0.25f * s3,
		(m[1].x - m[0].y) / s3
	);

	q = tracePositive ? q0 : (cond1 ? q1 : (cond2 ? q2 : q3));
	return normalize(q);
}

// Quaternion to Matrix (3x3)
float3x3 QuaternionToRotationMatrix(float4 q)
{
	float x2 = q.x + q.x,  y2 = q.y + q.y,	z2 = q.z + q.z;
	float xx = q.x * x2,   yy = q.y * y2,	zz = q.z * z2;
	float xy = q.x * y2,   xz = q.x * z2,	yz = q.y * z2;
	float wx = q.w * x2,   wy = q.w * y2,	wz = q.w * z2;

	float3x3 m;
	m[0] = float3(1.0f - (yy + zz), xy - wz, xz + wy);
	m[1] = float3(xy + wz, 1.0f - (xx + zz), yz - wx);
	m[2] = float3(xz - wy, yz + wx, 1.0f - (xx + yy));
	return m;
}

float4x4 BlendBoneMatrices(VertexShaderInput input, float4x4 bones[MAX_BONES], bool onlyRotation)
{
	if (onlyRotation)
	{
		float4 blendedQuat = float4(0, 0, 0, 0);
		float3 blendedTranslation = float3(0, 0, 0);

		[unroll]
		for (int i = 0; i < MAX_BONE_WEIGHTS; ++i)
		{
			float w = float(input.BoneWeight[i]) / 255.0f;
			int index = input.BoneIndex[i];
			float4x4 bone = bones[index];

			float3x3 rot = (float3x3)bone;
			float4 q = RotationMatrixToQuaternion(rot);

			// Ensure shortest path for interpolation (flip if dot < 0)
			float dotPrev = dot(blendedQuat, q);
			q *= sign(dotPrev + 1e-5f); // Avoid zero dot product flip

			blendedQuat += q * w;
		}

		blendedQuat = normalize(blendedQuat);
		float3x3 finalRot = QuaternionToRotationMatrix(blendedQuat);

		float4x4 result = float4x4(
			float4(finalRot[0], 0.0f),
			float4(finalRot[1], 0.0f),
			float4(finalRot[2], 0.0f),
			float4(bones[input.BoneIndex[0]][3].xyz, 1.0f)
		);

		return result;
	}
	else
	{
		float totalWeight = 0.0f;
		[unroll]
		for (int i = 0; i < MAX_BONE_WEIGHTS; ++i)
			totalWeight += float(input.BoneWeight[i]) / 255.0f;

		// Avoid divide-by-zero and excessive weights
		if (totalWeight < EPSILON)
			return bones[input.BoneIndex[0]];

		float4x4 blendedMatrix = (float4x4)0;

		[unroll]
		for (int i = 0; i < MAX_BONE_WEIGHTS; ++i)
		{
			float w = (float(input.BoneWeight[i]) / 255.0f) / totalWeight; // Normalize weights
			blendedMatrix += bones[input.BoneIndex[i]] * w;
		}

		// Remove artifacts
		blendedMatrix[0].w = 0.0f;
		blendedMatrix[1].w = 0.0f;
		blendedMatrix[2].w = 0.0f;
		blendedMatrix[3].w = 1.0f;

		return blendedMatrix;
	}
}
#endif // MATH
