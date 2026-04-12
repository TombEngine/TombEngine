// CloudNoiseTexture.cpp — CPU-side generation of tileable 3D/2D noise textures
// for the volumetric cloud shader.
//
// The noise algorithms here are functionally equivalent to the procedural
// functions in VolumetricClouds.hlsl (Hash31, ValueNoise3D, PerlinNoise3D,
// CurlNoise2D, WorleyNoise2D) but operate on tile-wrapped integer grids so
// the resulting textures tile seamlessly when sampled with D3D11_TEXTURE_ADDRESS_WRAP.
//
// Generated once at init time.  Runtime cost: zero.

#include "framework.h"
#include "Renderer/VolumetricCloud/CloudNoiseTexture.h"
#include "Renderer/RendererUtils.h"

#include <cmath>
#include <cstring>
#include <vector>
#include <algorithm>

namespace TEN::Renderer::VolumetricCloud
{
	// ========================================================================
	// Constants
	// ========================================================================

	static constexpr int NOISE_3D_RES     = 128;   // Texels per axis for 3D noise.
	static constexpr int NOISE_TILE       = 16;    // Noise cells per UV tile.
	static constexpr int WORLEY_2D_RES    = 256;   // Texels per axis for 2D Worley.
	static constexpr int WORLEY_TILE      = 16;    // Worley cells per UV tile.

	// ========================================================================
	// Math helpers
	// ========================================================================

	static float Frac(float x)
	{
		return x - std::floor(x);
	}

	static float Lerp(float a, float b, float t)
	{
		return a + (a - a) * 0.0f + (b - a) * t; // simple a+(b-a)*t
	}

	static float Saturate(float x)
	{
		return (x < 0.0f) ? 0.0f : (x > 1.0f) ? 1.0f : x;
	}

	// Quintic Hermite (same as shader).
	static float QuinticInterp(float t)
	{
		return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
	}

	// Positive modulo: always returns value in [0, m).
	static int PosMod(int x, int m)
	{
		return ((x % m) + m) % m;
	}

	// Simple 3D→1D hash matching the shader's Hash31.
	static float Hash31(float px, float py, float pz)
	{
		// frac(p * float3(0.1031, 0.1030, 0.0973))
		px = Frac(px * 0.1031f);
		py = Frac(py * 0.1030f);
		pz = Frac(pz * 0.0973f);
		// p += dot(p, p.yzx + 33.33)
		float d = px * py + py * pz + pz * px + (px + py + pz) * 33.33f;
		// Exact shader logic:
		// p += dot(p, p.yzx + 33.33)  =>  each component += dot
		float dotVal = px * (py + 33.33f) + py * (pz + 33.33f) + pz * (px + 33.33f);
		px += dotVal;
		py += dotVal;
		pz += dotVal;
		return Frac((px + py) * pz);
	}

	// Tiling value noise.  Wraps grid coordinates modulo `tile`.
	static float ValueNoise3D_Tiling(float x, float y, float z, int tile)
	{
		int ix = (int)std::floor(x);
		int iy = (int)std::floor(y);
		int iz = (int)std::floor(z);
		float fx = x - std::floor(x);
		float fy = y - std::floor(y);
		float fz = z - std::floor(z);

		float ux = QuinticInterp(fx);
		float uy = QuinticInterp(fy);
		float uz = QuinticInterp(fz);

		auto H = [&](int cx, int cy, int cz) -> float
		{
			float wx = (float)PosMod(cx, tile);
			float wy = (float)PosMod(cy, tile);
			float wz = (float)PosMod(cz, tile);
			return Hash31(wx, wy, wz);
		};

		float n000 = H(ix,     iy,     iz);
		float n100 = H(ix + 1, iy,     iz);
		float n010 = H(ix,     iy + 1, iz);
		float n110 = H(ix + 1, iy + 1, iz);
		float n001 = H(ix,     iy,     iz + 1);
		float n101 = H(ix + 1, iy,     iz + 1);
		float n011 = H(ix,     iy + 1, iz + 1);
		float n111 = H(ix + 1, iy + 1, iz + 1);

		float n00 = n000 + (n100 - n000) * ux;
		float n01 = n001 + (n101 - n001) * ux;
		float n10 = n010 + (n110 - n010) * ux;
		float n11 = n011 + (n111 - n011) * ux;
		float n0  = n00  + (n10  - n00) * uy;
		float n1  = n01  + (n11  - n01) * uy;
		return n0 + (n1 - n0) * uz;
	}

	// Gradient hash for Perlin noise (tiling version).
	struct Vec3 { float x, y, z; };

	static Vec3 GradHash33_Tiling(int ix, int iy, int iz, int tile)
	{
		float px = (float)PosMod(ix, tile);
		float py = (float)PosMod(iy, tile);
		float pz = (float)PosMod(iz, tile);

		// Same hash chain as shader's GradHash33.
		px = Frac(px * 0.1031f);
		py = Frac(py * 0.1030f);
		pz = Frac(pz * 0.0973f);
		float dotVal = px * (py + 33.33f) + py * (pz + 33.33f) + pz * (px + 33.33f);
		px += dotVal;
		py += dotVal;
		pz += dotVal;

		Vec3 h;
		h.x = Frac((px + py) * pz) * 2.0f - 1.0f;
		h.y = Frac((px + pz) * py) * 2.0f - 1.0f;
		h.z = Frac((py + pz) * px) * 2.0f - 1.0f;
		// Normalize.
		float len = std::sqrt(h.x * h.x + h.y * h.y + h.z * h.z);
		if (len < 1e-6f) { h.x = 1.0f; h.y = 0.0f; h.z = 0.0f; }
		else { h.x /= len; h.y /= len; h.z /= len; }
		return h;
	}

	// Tiling Perlin gradient noise, output in [0,1].
	static float PerlinNoise3D_Tiling(float x, float y, float z, int tile)
	{
		int ix = (int)std::floor(x);
		int iy = (int)std::floor(y);
		int iz = (int)std::floor(z);
		float fx = x - std::floor(x);
		float fy = y - std::floor(y);
		float fz = z - std::floor(z);

		float ux = QuinticInterp(fx);
		float uy = QuinticInterp(fy);
		float uz = QuinticInterp(fz);

		auto Dot = [&](int cx, int cy, int cz, float ox, float oy, float oz) -> float
		{
			Vec3 g = GradHash33_Tiling(cx, cy, cz, tile);
			return g.x * ox + g.y * oy + g.z * oz;
		};

		float n000 = Dot(ix,     iy,     iz,     fx,        fy,        fz);
		float n100 = Dot(ix + 1, iy,     iz,     fx - 1.0f, fy,        fz);
		float n010 = Dot(ix,     iy + 1, iz,     fx,        fy - 1.0f, fz);
		float n110 = Dot(ix + 1, iy + 1, iz,     fx - 1.0f, fy - 1.0f, fz);
		float n001 = Dot(ix,     iy,     iz + 1, fx,        fy,        fz - 1.0f);
		float n101 = Dot(ix + 1, iy,     iz + 1, fx - 1.0f, fy,        fz - 1.0f);
		float n011 = Dot(ix,     iy + 1, iz + 1, fx,        fy - 1.0f, fz - 1.0f);
		float n111 = Dot(ix + 1, iy + 1, iz + 1, fx - 1.0f, fy - 1.0f, fz - 1.0f);

		float n00 = n000 + (n100 - n000) * ux;
		float n01 = n001 + (n101 - n001) * ux;
		float n10 = n010 + (n110 - n010) * ux;
		float n11 = n011 + (n111 - n011) * ux;
		float n0  = n00  + (n10  - n00) * uy;
		float n1  = n01  + (n11  - n01) * uy;

		return Saturate((n0 + (n1 - n0) * uz) + 0.5f);
	}

	// Tiling Worley 2D noise (F1 = nearest cell distance), output in [0,1].
	static float WorleyNoise2D_Tiling(float px, float py, int tile)
	{
		int ix = (int)std::floor(px);
		int iy = (int)std::floor(py);

		float minDist = 8.0f;

		for (int dy = -1; dy <= 1; dy++)
		for (int dx = -1; dx <= 1; dx++)
		{
			int cx = ix + dx;
			int cy = iy + dy;

			// Wrap cell coordinates for tiling.
			float wcx = (float)PosMod(cx, tile);
			float wcy = (float)PosMod(cy, tile);

			// Same hash as shader: frac(sin(dot(...)) * 43758.5453123)
			float d1 = wcx * 127.1f + wcy * 311.7f;
			float d2 = wcx * 269.5f + wcy * 183.3f;
			float ptx = (float)cx + Frac(std::sin(d1) * 43758.5453123f);
			float pty = (float)cy + Frac(std::sin(d2) * 43758.5453123f);

			float ddx = px - ptx;
			float ddy = py - pty;
			float dist = std::sqrt(ddx * ddx + ddy * ddy);
			if (dist < minDist)
				minDist = dist;
		}

		return Saturate(minDist);
	}

	// ========================================================================
	// Texture generation
	// ========================================================================

	void CloudNoiseTextures::Initialize(ID3D11Device* device)
	{
		// ----------------------------------------------------------------
		// 1) Generate 3D noise data (128^3, RGBA8).
		// ----------------------------------------------------------------
		const int res3 = NOISE_3D_RES;
		const int tile3 = NOISE_TILE;
		const float invRes = (float)tile3 / (float)res3;   // Maps texel → noise coord.
		const float curlEps = 0.5f;                        // Finite-diff step for curl.

		std::vector<uint8_t> data3D(res3 * res3 * res3 * 4);

		for (int iz = 0; iz < res3; iz++)
		for (int iy = 0; iy < res3; iy++)
		for (int ix = 0; ix < res3; ix++)
		{
			// Noise-space coordinate for this texel.
			float nx = (float)ix * invRes;
			float ny = (float)iy * invRes;
			float nz = (float)iz * invRes;

			// R: Perlin gradient noise [0,1].
			float perlin = PerlinNoise3D_Tiling(nx, ny, nz, tile3);

			// G: Value noise [0,1].
			float value = ValueNoise3D_Tiling(nx, ny, nz, tile3);

			// B: Curl X component = dN/dz of value noise.
			float nz0 = ValueNoise3D_Tiling(nx, ny, nz + curlEps, tile3);
			float nz1 = ValueNoise3D_Tiling(nx, ny, nz - curlEps, tile3);
			float curlX = (nz0 - nz1) / (2.0f * curlEps);

			// A: Curl Z component = -dN/dx of value noise.
			float nx0 = ValueNoise3D_Tiling(nx + curlEps, ny, nz, tile3);
			float nx1 = ValueNoise3D_Tiling(nx - curlEps, ny, nz, tile3);
			float curlZ = -(nx0 - nx1) / (2.0f * curlEps);

			int idx = ((iz * res3 + iy) * res3 + ix) * 4;
			data3D[idx + 0] = (uint8_t)(Saturate(perlin) * 255.0f + 0.5f);
			data3D[idx + 1] = (uint8_t)(Saturate(value)  * 255.0f + 0.5f);
			data3D[idx + 2] = (uint8_t)(Saturate(curlX * 0.5f + 0.5f) * 255.0f + 0.5f);
			data3D[idx + 3] = (uint8_t)(Saturate(curlZ * 0.5f + 0.5f) * 255.0f + 0.5f);
		}

		// Create ID3D11Texture3D.
		D3D11_TEXTURE3D_DESC desc3D = {};
		desc3D.Width     = res3;
		desc3D.Height    = res3;
		desc3D.Depth     = res3;
		desc3D.MipLevels = 1;
		desc3D.Format    = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc3D.Usage     = D3D11_USAGE_IMMUTABLE;
		desc3D.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA initData3D = {};
		initData3D.pSysMem          = data3D.data();
		initData3D.SysMemPitch      = res3 * 4;            // bytes per row (one XY-row).
		initData3D.SysMemSlicePitch = res3 * res3 * 4;     // bytes per slice (one Z-slice).

		Utils::throwIfFailed(
			device->CreateTexture3D(&desc3D, &initData3D, Noise3D.GetAddressOf()),
			"CloudNoiseTextures: CreateTexture3D failed");

		// Create SRV.
		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc3D = {};
		srvDesc3D.Format                    = desc3D.Format;
		srvDesc3D.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE3D;
		srvDesc3D.Texture3D.MipLevels       = 1;
		srvDesc3D.Texture3D.MostDetailedMip = 0;

		Utils::throwIfFailed(
			device->CreateShaderResourceView(Noise3D.Get(), &srvDesc3D, Noise3DSRV.GetAddressOf()),
			"CloudNoiseTextures: Noise3D SRV failed");

		// ----------------------------------------------------------------
		// 2) Generate 2D Worley data (256^2, RG8).
		// ----------------------------------------------------------------
		const int res2   = WORLEY_2D_RES;
		const int tile2  = WORLEY_TILE;
		const float invRes2 = (float)tile2 / (float)res2;

		std::vector<uint8_t> data2D(res2 * res2 * 2);

		for (int iy = 0; iy < res2; iy++)
		for (int ix = 0; ix < res2; ix++)
		{
			float wx = (float)ix * invRes2;
			float wy = (float)iy * invRes2;

			// R: Worley F1 at base scale.
			float w1 = WorleyNoise2D_Tiling(wx, wy, tile2);

			// G: Worley F1 at offset seed (shift by half tile for a different pattern).
			float w2 = WorleyNoise2D_Tiling(wx + 7.31f, wy + 3.17f, tile2);

			int idx = (iy * res2 + ix) * 2;
			data2D[idx + 0] = (uint8_t)(Saturate(w1) * 255.0f + 0.5f);
			data2D[idx + 1] = (uint8_t)(Saturate(w2) * 255.0f + 0.5f);
		}

		D3D11_TEXTURE2D_DESC desc2D = {};
		desc2D.Width            = res2;
		desc2D.Height           = res2;
		desc2D.MipLevels        = 1;
		desc2D.ArraySize        = 1;
		desc2D.Format           = DXGI_FORMAT_R8G8_UNORM;
		desc2D.SampleDesc.Count = 1;
		desc2D.Usage            = D3D11_USAGE_IMMUTABLE;
		desc2D.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA initData2D = {};
		initData2D.pSysMem     = data2D.data();
		initData2D.SysMemPitch = res2 * 2;

		Utils::throwIfFailed(
			device->CreateTexture2D(&desc2D, &initData2D, Worley2D.GetAddressOf()),
			"CloudNoiseTextures: CreateTexture2D failed");

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc2D = {};
		srvDesc2D.Format                    = desc2D.Format;
		srvDesc2D.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc2D.Texture2D.MipLevels       = 1;
		srvDesc2D.Texture2D.MostDetailedMip = 0;

		Utils::throwIfFailed(
			device->CreateShaderResourceView(Worley2D.Get(), &srvDesc2D, Worley2DSRV.GetAddressOf()),
			"CloudNoiseTextures: Worley2D SRV failed");

		// ----------------------------------------------------------------
		// 3) Create a LinearWrap sampler for noise texture tiling.
		// ----------------------------------------------------------------
		// ClearState() resets all sampler slots to D3D11's default (LinearClamp).
		// The cloud noise textures MUST tile via WRAP addressing, so we create
		// and explicitly bind a LinearWrap sampler at s2 in Bind().
		D3D11_SAMPLER_DESC sampDesc = {};
		sampDesc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampDesc.AddressU       = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressV       = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressW       = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		sampDesc.MaxAnisotropy  = 1;
		sampDesc.MinLOD         = 0;
		sampDesc.MaxLOD         = D3D11_FLOAT32_MAX;

		Utils::throwIfFailed(
			device->CreateSamplerState(&sampDesc, LinearWrapSampler.GetAddressOf()),
			"CloudNoiseTextures: LinearWrap sampler creation failed");
	}

	void CloudNoiseTextures::Bind(ID3D11DeviceContext* context) const
	{
		// t5 = Noise3D, t6 = Worley2D.
		ID3D11ShaderResourceView* srvs[2] = { Noise3DSRV.Get(), Worley2DSRV.Get() };
		context->PSSetShaderResources(5, 2, srvs);

		// Ensure s2 (LinearSamp in the shader) is LinearWrap for noise tiling.
		// ClearState() at end of frame resets all samplers to default (LinearClamp),
		// and no other code sets s2 before the cloud pass.
		ID3D11SamplerState* sampler = LinearWrapSampler.Get();
		context->PSSetSamplers(2, 1, &sampler);
	}

	void CloudNoiseTextures::Unbind(ID3D11DeviceContext* context) const
	{
		ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
		context->PSSetShaderResources(5, 2, nullSRVs);
	}
}
