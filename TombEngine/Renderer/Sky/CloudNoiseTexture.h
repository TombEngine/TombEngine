#pragma once

#include <vector>

namespace TEN::Renderer::VolumetricCloud
{
	// Pre-computed 3D and 2D noise textures that replace per-sample procedural
	// noise in VolumetricClouds.hlsl. Generated once at init time on the CPU using
	// tileable noise algorithms; the scene renderer uploads them and samples them with wrapping.
	//
	// Texture layout:
	//   Noise3D  (128^3 RGBA8_UNORM):
	//       R = Perlin gradient noise  [0,1]
	//       G = Value noise            [0,1]
	//       B = Curl X  (dN/dz)        [0,1] remapped from [-1,1]
	//       A = Curl Z  (-dN/dx)       [0,1] remapped from [-1,1]
	//
	//   Worley2D (256^2 RG8_UNORM):
	//       R = Worley F1 distance     [0,1]  (for worleyA)
	//       G = Worley F1 at 1.7x freq [0,1]  (for worleyB, different cell pattern)
	struct CloudNoiseTextures
	{
		std::vector<unsigned char> Noise3DData        = {};
		std::vector<unsigned char> Worley2DData       = {};
		int                        Noise3DResolution  = 0;
		int                        Worley2DResolution = 0;

		// Called once from Renderer::InitializeVolumetricClouds().
		void Initialize();
	};
}
