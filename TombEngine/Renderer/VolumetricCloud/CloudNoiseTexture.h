#pragma once

#include <memory>
#include "Renderer/Graphics/ITexture2D.h"
#include "Renderer/Graphics/ITexture3D.h"

namespace TEN::Renderer::Graphics
{
	class IGraphicsDevice;
}

namespace TEN::Renderer::VolumetricCloud
{
	using TEN::Renderer::Graphics::IGraphicsDevice;
	using TEN::Renderer::Graphics::ITexture2D;
	using TEN::Renderer::Graphics::ITexture3D;

	// Pre-computed 3D and 2D noise textures that replace per-sample procedural
	// noise in VolumetricClouds.hlsl.  Generated once at init time on the CPU
	// using tileable noise algorithms; the shader fetches them with a WRAP sampler.
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
	struct CloudNoiseTextures
	{
		std::unique_ptr<ITexture3D> Noise3D;
		std::unique_ptr<ITexture2D> Worley2D;

		// Generate textures via the abstract device. Called once from
		// Renderer::InitializeVolumetricClouds().
		void Initialize(IGraphicsDevice* device);

		// Bind to the cloud noise slots with LinearWrap sampling.
		void Bind(IGraphicsDevice* device) const;

		// Unbind the cloud noise slots.
		void Unbind(IGraphicsDevice* device) const;
	};
}
