#include "./CBPostProcess.hlsli"
#include "./CBCamera.hlsli"
#include "./Materials.hlsli"
#include "./Math.hlsli"

#define MAX_BLUR_RADIUS 100
#define USE_FAST_BILINEAR_BLUR 1

struct PostProcessVertexShaderInput
{
    float3 Position: POSITION0;
    float2 UV: TEXCOORD0;
    float4 Color: COLOR0;
};

struct PixelShaderInput
{
    float4 Position: SV_POSITION;
    float2 UV: TEXCOORD0;
    float4 Color: COLOR0;
    float4 PositionCopy: TEXCOORD1;
};

Texture2D ColorTexture : register(t0);
SamplerState ColorSampler : register(s0);

Texture2D DepthTexture : register(t1);
SamplerState DepthSampler : register(s1);

Texture2D NormalsTexture : register(t2);
SamplerState NormalsSampler : register(s2);

Texture2D GlowTexture : register(t3);
SamplerState GlowSampler : register(s3);

Texture2D LegacyEnvironmentTexture : register(t4);
SamplerState LegacyEnvironmentSampler : register(s4);

Texture2D EmissiveAndSpecularTexture : register(t5);
SamplerState EmissiveAndSpecularSampler : register(s5);

PixelShaderInput VS(PostProcessVertexShaderInput input)
{
    PixelShaderInput output;

    output.Position = float4(input.Position, 1.0f);
    output.UV = input.UV;
    output.Color = input.Color;
    output.PositionCopy = output.Position;

    return output;
}

float4 PS(PixelShaderInput input) : SV_Target
{
    return ColorTexture.Sample(ColorSampler, input.UV);
}

float4 PSMonochrome(PixelShaderInput input) : SV_Target
{
    float4 color = ColorTexture.Sample(ColorSampler, input.UV);

    float luma = Luma(color.rgb);
    float3 output = lerp(color.rgb, float3(luma, luma, luma), EffectStrength);

    return float4(output, color.a);
}

float4 PSNegative(PixelShaderInput input) : SV_Target
{
	float4 color = ColorTexture.Sample(ColorSampler, input.UV);

	float luma = Luma(1.0f - color);
	float3 output = lerp(color.rgb, float3(luma, luma, luma), EffectStrength);

	return float4(output, color.a);
}

float4 PSExclusion(PixelShaderInput input) : SV_Target
{
	float4 color = ColorTexture.Sample(ColorSampler, input.UV);

	float3 exColor = color.xyz + (1.0f - color.xyz) - 2.0f * color.xyz * (1.0f - color.xyz);
	float3 output = lerp(color.rgb, exColor, EffectStrength);

	return float4(output, color.a);
}

float4 PSFinalPass(PixelShaderInput input) : SV_TARGET
{
    float4 output = ColorTexture.Sample(ColorSampler, input.UV);

    float3 colorMul = min(input.Color.xyz, 1.0f);

    float y = input.Position.y / ViewportSize.y;

    if (y > 1.0f - CinematicBarsHeight ||
        y < 0.0f + CinematicBarsHeight)
    {
        output = float4(0, 0, 0, 1);
    }
    else
    {
        output.xyz = output.xyz * colorMul.xyz * ScreenFadeFactor;
        output.w = 1.0f;
    }

	output.xyz = output.xyz * Tint;

    return output;
}

float3 LensFlare(float2 uv, float2 pos)
{
	float intensity = 0.5f;

	float2 main = uv - pos;
	float2 uvd = uv * length(uv);

	// Angular and distance calculations
	float ang = atan2(main.y, main.x);
	float dist = length(main);
	dist = pow(dist, 0.1f);

	// Sunflare effect with rotation and ray length variation
	float f0 = 1.0f / (length(uv - pos) * 35.0f + 1.0f);
	f0 = pow(f0, 1.5f);

	// Calculate position-based rotation offset
	float rotationOffset = dot(pos, float2(0.5f, 0.5f)) * (PI / 2.0f);

	// Modify starburst pattern with rotation and variation
	f0 += f0 * (sin((ang + rotationOffset + 1.0f / 18.0f) * 8.0f) * 0.2f + dist * 0.1f + 0.2f);

	// Lensflare glow components
	float f2  = max(1.0f / (1.0f + 32.0f * pow(length(uvd + 0.8f  * pos), 1.0f)), 0.0f) * 0.25f;
	float f22 = max(1.0f / (1.0f + 32.0f * pow(length(uvd + 0.85f * pos), 1.0f)), 0.0f) * 0.23f;
	float f23 = max(1.0f / (1.0f + 32.0f * pow(length(uvd + 0.9f  * pos), 1.0f)), 0.0f) * 0.21f;

	// Circular lens artifacts
	float2 uvx = lerp(uv, uvd, -0.5f);
	float f4  = max(0.01f - pow(length(uvx + 0.4f  * pos), 2.4f), 0.0f) * 9.0f;
	float f42 = max(0.01f - pow(length(uvx + 0.45f * pos), 2.4f), 0.0f) * 7.0f;
	float f43 = max(0.01f - pow(length(uvx + 0.5f  * pos), 2.4f), 0.0f) * 6.0f;

	// Smaller lens artifacts
	uvx = lerp(uv, uvd, -0.4f);
	float f5  = max(0.01f - pow(length(uvx + 0.2f * pos), 5.5f), 0.0f) * 2.0f;
	float f52 = max(0.01f - pow(length(uvx + 0.4f * pos), 5.5f), 0.0f) * 2.0f;
	float f53 = max(0.01f - pow(length(uvx + 0.6f * pos), 5.5f), 0.0f) * 2.0f;

	// Symmetric artifacts
	uvx = lerp(uv, uvd, -0.5f);
	float f6  = max(0.01f - pow(length(uvx - 0.3f   * pos), 1.6f), 0.0f) * 9.0f;
	float f62 = max(0.01f - pow(length(uvx - 0.325f * pos), 1.6f), 0.0f) * 6.0f;
	float f63 = max(0.01f - pow(length(uvx - 0.35f  * pos), 1.6f), 0.0f) * 7.0f;

	// Sunflare and lensflare outputs
	float3 sunflare = float3(f0, f0, f0);
	float3 lensflare = float3(
	    f2 + f4 + f5 + f6,
	    f22 + f42 + f52 + f62,
	    f23 + f43 + f53 + f63
	);
	
    // Subtle anamorphic glare
    float anamorphicScaleX = 1.2f; // Horizontal stretch factor
    float anamorphicScaleY = 35.0f;  // Vertical compression factor
	float2 anamorphicOffset = main; // Center at the lensflare source
    float glare = exp(-pow(anamorphicOffset.x * anamorphicScaleX, 2.0f) - pow(anamorphicOffset.y * anamorphicScaleY, 2.0f));
    float3 anamorphicGlare = float3(glare * 0.6f, glare * 0.5f, glare * 1.0f) * 0.05f;

    // Combine the effects and adjust intensity
    float3 c = saturate(sunflare) * 0.5f + lensflare + anamorphicGlare;
    c = c * 1.3f - float3(length(uvd) * 0.05f, length(uvd) * 0.05f, length(uvd) * 0.05f);

    return c * intensity;
}

float3 LensFlareColorCorrection(float3 color, float factor,float factor2) 
{
	float w = color.x + color.y + color.z;
	return lerp(color, float3(w, w, w) * factor, w * factor2);
}

float4 PSLensFlare(PixelShaderInput input) : SV_Target
{
	float4 color = ColorTexture.Sample(ColorSampler, input.UV);
	
	float4 position = input.PositionCopy;
	float3 totalLensFlareColor = float3(0.0f, 0.0f, 0.0f);

	for (int i = 0; i < NumLensFlares; i++)
	{
		float4 lensFlarePosition = float4(LensFlares[i].Position, 1.0f);
        lensFlarePosition = mul(mul(lensFlarePosition, View), Projection); 
		lensFlarePosition.xyz /= lensFlarePosition.w;

		float3 lensFlareColor = max(float3(0.0f, 0.0f, 0.0f),
			LensFlares[i].Color *
			float3(4.5f, 3.6f, 3.6f) * 
			LensFlare(position.xy, lensFlarePosition.xy));
		lensFlareColor = LensFlareColorCorrection(lensFlareColor, 0.5f, 0.1f);
		totalLensFlareColor += lensFlareColor;
	}

	color.xyz = lerp(color.xyz, color.xyz + totalLensFlareColor, saturate(dot(totalLensFlareColor, float3(0.5f, 0.5f, 0.5f))));

	return color;
}

float4 PSBlurBilinear(PixelShaderInput input) : SV_Target
{
    int r = clamp(BlurRadius, 0, MAX_BLUR_RADIUS);
    
    float w0 = Gaussian(0.0, BlurSigma);
    float4 color = ColorTexture.Sample(ColorSampler, input.UV) * w0;
    float weightSum = w0;

    [unroll]
    for (int k = 1; k <= MAX_BLUR_RADIUS; ++k)
    {
        if (k > r)
            break;

        float wk = Gaussian((float) k, BlurSigma);

        float2 off = BlurDirection * TexelSize * k;
        float4 c1 = ColorTexture.Sample(ColorSampler, input.UV + off);
        float4 c2 = ColorTexture.Sample(ColorSampler, input.UV - off);

        color += (c1 + c2) * wk;
        weightSum += 2.0 * wk;
    }

    return color / weightSum;
}

float4 PSBlurFull(PixelShaderInput input) : SV_Target
{
    int r = clamp(BlurRadius, 0, MAX_BLUR_RADIUS);

    float4 color = 0;
    float weightSum = 0;

    [unroll]
    for (int k = -MAX_BLUR_RADIUS; k <= MAX_BLUR_RADIUS; ++k)
    {
        if (abs(k) > r)
            continue;

        float w = Gaussian((float) k, BlurSigma);
        float2 off = BlurDirection * TexelSize * k;
        color += ColorTexture.Sample(ColorSampler, input.UV + off) * w;
        weightSum += w;
    }

    return color / weightSum;
}

float4 PSBlur(PixelShaderInput input) : SV_Target
{
#if USE_FAST_BILINEAR_BLUR
    return PSBlurBilinear(input);
#else
    return PSBlurFull(input);
#endif
}

float4 PSDownscale(PixelShaderInput input) : SV_Target
{
    float2 halfDstUV = 0.5f / (ViewportSize / DownscaleFactor);

    const float wc = 0.5;
    const float w1 = 0.125;
    const float wd = 0.03125;

    float2 dx = float2(halfDstUV.x, 0);
    float2 dy = float2(0, halfDstUV.y);

    float3 c = 0;

    // center
    c += ColorTexture.SampleLevel(ColorSampler, input.UV, 0).rgb * wc;

    // N/S/E/W
    c += ColorTexture.SampleLevel(ColorSampler, input.UV + dx, 0).rgb * w1;
    c += ColorTexture.SampleLevel(ColorSampler, input.UV - dx, 0).rgb * w1;
    c += ColorTexture.SampleLevel(ColorSampler, input.UV + dy, 0).rgb * w1;
    c += ColorTexture.SampleLevel(ColorSampler, input.UV - dy, 0).rgb * w1;

    // diagonals
    c += ColorTexture.SampleLevel(ColorSampler, input.UV + dx + dy, 0).rgb * wd;
    c += ColorTexture.SampleLevel(ColorSampler, input.UV + dx - dy, 0).rgb * wd;
    c += ColorTexture.SampleLevel(ColorSampler, input.UV - dx + dy, 0).rgb * wd;
    c += ColorTexture.SampleLevel(ColorSampler, input.UV - dx - dy, 0).rgb * wd;

    return float4(c, 1);
}

float3 SoftAddBlend(float3 base, float3 add)
{
    return 1.0 - (1.0 - base) * (1.0 - add);
}

float4 PSGlowCombine(PixelShaderInput input) : SV_Target
{
    float3 base = ColorTexture.Sample(ColorSampler, input.UV).rgb;
    float3 glow = GlowTexture.Sample(GlowSampler, input.UV).rgb * GlowIntensity;

    float3 outc = (GlowSoftAdd != 0) ? SoftAddBlend(base, glow)
                                 : saturate(base + glow);

    return float4(outc, 1);
}
