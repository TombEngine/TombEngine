#include "./CBCamera.hlsli"
#include "./VertexInput.hlsli"
#include "./VertexEffects.hlsli"
#include "./Blending.hlsli"
#include "./Math.hlsli"
#include "./AnimatedTextures.hlsli"
#include "./Shadows.hlsli"
#include "./ShaderLight.hlsli"

#define ROOM_LIGHT_COEFF 0.7f

cbuffer RoomBuffer : register(b5)
{
	int Water;
	int Caustics;
	int NumRoomLights;
	int Padding;
	float2 CausticsStartUV;
    float2 CausticsSize;
	float4 AmbientColor;
	ShaderLight RoomLights[MAX_LIGHTS_PER_ROOM];
};

struct PixelShaderInput
{
	float4 Position: SV_POSITION;
	float3 WorldPosition: POSITION0;
	float3 Normal: NORMAL;
	float2 UV: TEXCOORD0;
	float4 Color: COLOR;
	float Sheen : SHEEN;
	float4 PositionCopy : TEXCOORD1;
	float4 FogBulbs : TEXCOORD2;
	float DistanceFog : FOG;
	float3 Tangent: TANGENT;
	float3 Binormal: BINORMAL;
};

Texture2D Texture : register(t0);
SamplerState Sampler : register(s0);

Texture2D NormalTexture : register(t1);
SamplerState NormalTextureSampler : register(s1);

Texture2D CausticsTexture : register(t2);
SamplerState CausticsTextureSampler : register(s2);

Texture2D SSAOTexture : register(t9);
SamplerState SSAOSampler : register(s9);

struct PixelShaderOutput
{
	float4 Color: SV_TARGET0;
};

PixelShaderInput VS(VertexShaderInput input)
{
	PixelShaderInput output;

	// Setting effect weight on TE side prevents portal vertices from moving.
	// Here we just read weight and decide if we should apply refraction or movement effect.
	float weight = input.Effects.z;

	// Calculate vertex effects
	float wibble = Wibble(input.Effects.xyz, input.Hash);
	float3 pos = Move(input.Position, input.Effects.xyz * weight, wibble);
	float3 col = Glow(input.Color.xyz, input.Effects.xyz, wibble);

	// Refraction
	float4 screenPos = mul(float4(pos, 1.0f), ViewProjection);
	float2 clipPos = screenPos.xy / screenPos.w;

	if (CameraUnderwater != Water)
	{
		float factor = (Frame + clipPos.x * 320);
		float xOffset = (sin(factor * PI / 20.0f)) * (screenPos.z / 1024) * 4;
		float yOffset = (cos(factor * PI / 20.0f)) * (screenPos.z / 1024) * 4;
		screenPos.x += xOffset * weight;
		screenPos.y += yOffset * weight;
	}
	
	output.Position = screenPos;
	output.Normal = input.Normal;
	output.Color = float4(col, input.Color.w);
	output.PositionCopy = screenPos;

#ifdef ANIMATED

	if (Type == 0)
		output.UV = GetFrame(input.PolyIndex, input.AnimationFrameOffset);
	else
		output.UV = input.UV; // TODO: true UVRotate in future?
#else
	output.UV = input.UV;
#endif
	
	output.WorldPosition = pos;
	output.Tangent = input.Tangent;
	output.Binormal = input.Binormal;

	output.FogBulbs = DoFogBulbsForVertex(output.WorldPosition);
	output.DistanceFog = DoDistanceFogForVertex(output.WorldPosition);

	return output;
}

float3 UnpackNormalMap(float4 n)
{
	n = n * 2.0f - 1.0f;
	n.z = saturate(1.0f - dot(n.xy, n.xy));
	return n.xyz;
}

float3 PackNormal(float3 n)
{
	n = (n + 1.0f) * 0.5f;
	n.z = 0;
	return n.xyz;
}

PixelShaderOutput PS(PixelShaderInput input)
{
	PixelShaderOutput output;

	output.Color = Texture.Sample(Sampler, input.UV);

	DoAlphaTest(output.Color);

	float3x3 TBN = float3x3(input.Tangent, input.Binormal, input.Normal);
	float3 normal = UnpackNormalMap(NormalTexture.Sample(NormalTextureSampler, input.UV));
	normal = normalize(mul(normal, TBN));

	float3 lighting = input.Color.xyz;
	bool doLights = true;

	float occlusion = 1.0f;
	if (AmbientOcclusion == 1)
	{
		float2 samplePosition;
		samplePosition = input.PositionCopy.xy / input.PositionCopy.w; // Perspective divide
		samplePosition = samplePosition * 0.5f + 0.5f; // transform to range 0.0 - 1.0  
		samplePosition.y = 1.0f - samplePosition.y;
		occlusion = pow(SSAOTexture.Sample(SSAOSampler, samplePosition).x, AmbientOcclusionExponent);
		
		if (BlendMode == BLENDMODE_ALPHABLEND)
			occlusion = lerp(occlusion, 1.0f, output.Color.w);
	}

	lighting = DoShadow(input.WorldPosition, normal, lighting, -2.5f);
	lighting = DoBlobShadows(input.WorldPosition, lighting);

	bool onlyPointLights = (NumRoomLights & ~LT_MASK) == LT_MASK_POINT;
	int numLights = NumRoomLights & LT_MASK;

	for (int i = 0; i < numLights; i++)
	{
		if (onlyPointLights)
		{
			lighting += DoPointLight(input.WorldPosition, normal, RoomLights[i]) * ROOM_LIGHT_COEFF;
		}
		else
		{
			// Room dynamic lights can only be spot or point, so we use simplified function for that.

			float isPoint = step(0.5f, RoomLights[i].Type == LT_POINT);
			float isSpot  = step(0.5f, RoomLights[i].Type == LT_SPOT);

			float3 pointLight = float3(0.0f, 0.0f, 0.0f);
			float3 spotLight  = float3(0.0f, 0.0f, 0.0f);
			DoPointAndSpotLight(input.WorldPosition, normal, RoomLights[i], pointLight, spotLight);
			
			lighting += pointLight * isPoint * ROOM_LIGHT_COEFF + spotLight  * isSpot * ROOM_LIGHT_COEFF;
		}
	}

    if (Caustics)
    {
        float attenuation = saturate(dot(float3(0.0f, -1.0f, 0.0f), normal));

        float3 blending = abs(normal);
        blending = normalize(max(blending, 0.00001f));
        float b = (blending.x + blending.y + blending.z);
        blending /= float3(b, b, b);

        float3 p = frac(input.WorldPosition.xyz / 2048.0f);
	
		float2 uv_x = CausticsStartUV + float2(p.z, p.y) * CausticsSize;
        float2 uv_y = CausticsStartUV + float2(p.z, p.x) * CausticsSize;
        float2 uv_z = CausticsStartUV + float2(p.y, p.x) * CausticsSize;

        float3 xaxis = CausticsTexture.SampleLevel(CausticsTextureSampler, uv_x, 0).xyz;
        float3 yaxis = CausticsTexture.SampleLevel(CausticsTextureSampler, uv_y, 0).xyz;
        float3 zaxis = CausticsTexture.SampleLevel(CausticsTextureSampler, uv_z, 0).xyz;

        float3 xc = xaxis * blending.x;
        float3 yc = yaxis * blending.y;
        float3 zc = zaxis * blending.z;

        float3 caustics = xc + yc + zc;

        lighting += (caustics * attenuation * 2.0f);
    }

	lighting -= float3(input.FogBulbs.w, input.FogBulbs.w, input.FogBulbs.w);
	output.Color.xyz = output.Color.xyz * lighting * occlusion;
	output.Color.xyz = saturate(output.Color.xyz);

	output.Color = DoFogBulbsForPixel(output.Color, float4(input.FogBulbs.xyz, 1.0f));
	output.Color = DoDistanceFogForPixel(output.Color, FogColor, input.DistanceFog);

	return output;
}
