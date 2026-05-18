// CloudNoiseTexture.cpp — CPU-side generation of tileable 3D/2D noise textures
// for the volumetric cloud shader.
//
// The noise algorithms here are functionally equivalent to the procedural
// functions in VolumetricClouds.hlsl (Hash31, ValueNoise3D, PerlinNoise3D,
// CurlNoise2D, WorleyNoise2D) but operate on tile-wrapped integer grids so
// the resulting textures tile seamlessly under WRAP addressing.
//
// Generated once at init time.  Runtime cost: zero.

#include "framework.h"
#include "Renderer/VolumetricCloud/CloudNoiseTexture.h"
#include "Renderer/Graphics/IGraphicsDevice.h"
#include "Renderer/RendererEnums.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>

namespace TEN::Renderer::VolumetricCloud
{

	// ========================================================================
	// Constants
	// ========================================================================

	static constexpr int NOISE_3D_RES   = 128;   // Texels per axis for 3D noise.
	static constexpr int NOISE_TILE     = 16;    // Noise cells per UV tile.
	static constexpr int WORLEY_2D_RES  = 256;   // Texels per axis for 2D Worley.
	static constexpr int WORLEY_TILE    = 16;    // Worley cells per UV tile.

	// Cloud noise binding slots (re-used during the cloud pass).
	static constexpr TextureRegister      NOISE_3D_REGISTER  = TextureRegister::Hud;              // t5
	static constexpr TextureRegister      WORLEY_2D_REGISTER = TextureRegister::GBufferDepthMap;  // t6
	static constexpr SamplerStateRegister NOISE_SAMPLER      = SamplerStateRegister::LinearWrap;

	// ========================================================================
	// Math helpers
	// ========================================================================

	static float Frac(float x)
	{
		return x - std::floor(x);
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
		px = Frac(px * 0.1031f);
		py = Frac(py * 0.1030f);
		pz = Frac(pz * 0.0973f);
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

		return std::clamp((n0 + (n1 - n0) * uz) + 0.5f, 0.0f, 1.0f);
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

			float wcx = (float)PosMod(cx, tile);
			float wcy = (float)PosMod(cy, tile);

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

		return std::clamp(minDist, 0.0f, 1.0f);
	}

	// ========================================================================
	// Texture generation
	// ========================================================================

	void CloudNoiseTextures::Initialize(IGraphicsDevice* device)
	{
		// ----------------------------------------------------------------
		// 1) Generate 3D noise data (128^3, RGBA8).
		// ----------------------------------------------------------------
		const int res3 = NOISE_3D_RES;
		const int tile3 = NOISE_TILE;
		const float invRes = (float)tile3 / (float)res3;
		const float curlEps = 0.5f;

		std::vector<uint8_t> data3D(res3 * res3 * res3 * 4);

		for (int iz = 0; iz < res3; iz++)
		for (int iy = 0; iy < res3; iy++)
		for (int ix = 0; ix < res3; ix++)
		{
			float nx = (float)ix * invRes;
			float ny = (float)iy * invRes;
			float nz = (float)iz * invRes;

			float perlin = PerlinNoise3D_Tiling(nx, ny, nz, tile3);
			float value  = ValueNoise3D_Tiling(nx, ny, nz, tile3);

			float nz0 = ValueNoise3D_Tiling(nx, ny, nz + curlEps, tile3);
			float nz1 = ValueNoise3D_Tiling(nx, ny, nz - curlEps, tile3);
			float curlX = (nz0 - nz1) / (2.0f * curlEps);

			float nx0 = ValueNoise3D_Tiling(nx + curlEps, ny, nz, tile3);
			float nx1 = ValueNoise3D_Tiling(nx - curlEps, ny, nz, tile3);
			float curlZ = -(nx0 - nx1) / (2.0f * curlEps);

			int idx = ((iz * res3 + iy) * res3 + ix) * 4;
			data3D[idx + 0] = (uint8_t)(std::clamp(perlin, 0.0f, 1.0f) * 255.0f + 0.5f);
			data3D[idx + 1] = (uint8_t)(std::clamp(value, 0.0f, 1.0f)  * 255.0f + 0.5f);
			data3D[idx + 2] = (uint8_t)(std::clamp(curlX * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f + 0.5f);
			data3D[idx + 3] = (uint8_t)(std::clamp(curlZ * 0.5f + 0.5f, 0.0f, 1.0f) * 255.0f + 0.5f);
		}

		Noise3D = device->CreateTexture3D(res3, res3, res3,
			SurfaceFormat::SF_RGBA8_Unorm, data3D.data());

		// ----------------------------------------------------------------
		// 2) Generate 2D Worley data (256^2, RG8).
		// ----------------------------------------------------------------
		const int res2     = WORLEY_2D_RES;
		const int tile2    = WORLEY_TILE;
		const float invRes2 = (float)tile2 / (float)res2;

		std::vector<uint8_t> data2D(res2 * res2 * 2);

		for (int iy = 0; iy < res2; iy++)
		for (int ix = 0; ix < res2; ix++)
		{
			float wx = (float)ix * invRes2;
			float wy = (float)iy * invRes2;

			float w1 = WorleyNoise2D_Tiling(wx, wy, tile2);
			float w2 = WorleyNoise2D_Tiling(wx + 7.31f, wy + 3.17f, tile2);

			int idx = (iy * res2 + ix) * 2;
			data2D[idx + 0] = (uint8_t)(std::clamp(w1, 0.0f, 1.0f) * 255.0f + 0.5f);
			data2D[idx + 1] = (uint8_t)(std::clamp(w2, 0.0f, 1.0f) * 255.0f + 0.5f);
		}

		Worley2D = device->CreateTexture2D(res2, res2,
			SurfaceFormat::SF_RG8_Unorm, data2D.data(), false);
	}

	void CloudNoiseTextures::Bind(IGraphicsDevice* device) const
	{
		device->BindTexture(NOISE_3D_REGISTER,  Noise3D.get(),  NOISE_SAMPLER);
		device->BindTexture(WORLEY_2D_REGISTER, Worley2D.get(), NOISE_SAMPLER);
	}

	void CloudNoiseTextures::Unbind(IGraphicsDevice* device) const
	{
		device->BindTexture(NOISE_3D_REGISTER,  nullptr, NOISE_SAMPLER);
		device->BindTexture(WORLEY_2D_REGISTER, nullptr, NOISE_SAMPLER);
	}
}
