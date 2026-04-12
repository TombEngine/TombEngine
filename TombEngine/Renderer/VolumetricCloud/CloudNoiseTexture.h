#pragma once

#include <d3d11.h>
#include <wrl/client.h>

namespace TEN::Renderer::VolumetricCloud
{
	using Microsoft::WRL::ComPtr;

	// Pre-computed 3D and 2D noise textures that replace per-sample procedural
	// noise in VolumetricClouds.hlsl.  Generated once at init time on the CPU
	// using tileable noise algorithms; the shader fetches them via tex3D/tex2D
	// with a WRAP sampler.
	//
	// Texture layout:
	//   Noise3D  (128^3 RGBA8_UNORM, register t5):
	//       R = Perlin gradient noise  [0,1]
	//       G = Value noise            [0,1]
	//       B = Curl X  (dN/dz)        [0,1] remapped from [-1,1]
	//       A = Curl Z  (-dN/dx)       [0,1] remapped from [-1,1]
	//
	//   Worley2D (256^2 RG8_UNORM, register t6):
	//       R = Worley F1 distance     [0,1]  (for worleyA)
	//       G = Worley F1 at 1.7x freq [0,1]  (for worleyB, different cell pattern)
	//
	// Tile periods (noise cells per UV tile):
	//   Noise3D:  16 cells  (128 / 16 = 8 texels per cell)
	//   Worley2D: 16 cells  (256 / 16 = 16 texels per cell)

	struct CloudNoiseTextures
	{
		// 3D noise: Perlin + Value + Curl.
		ComPtr<ID3D11Texture3D>          Noise3D;
		ComPtr<ID3D11ShaderResourceView> Noise3DSRV;

		// 2D Worley noise.
		ComPtr<ID3D11Texture2D>          Worley2D;
		ComPtr<ID3D11ShaderResourceView> Worley2DSRV;

		// LinearWrap sampler for noise tiling (bound at s2).
		ComPtr<ID3D11SamplerState>       LinearWrapSampler;

		// Create textures on the given device.  Called once from
		// Renderer::InitializeVolumetricClouds().
		void Initialize(ID3D11Device* device);

		// Bind to pixel-shader slots t5, t6 and set LinearWrap sampler at s2.
		void Bind(ID3D11DeviceContext* context) const;

		// Unbind t5, t6.
		void Unbind(ID3D11DeviceContext* context) const;
	};
}
