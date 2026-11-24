#pragma once
#include "Renderer/RendererEnums.h"
		
namespace TEN::Renderer::Structures
{
	// Lens flare of a view. Direction is the normalized vector from the camera to the flare; Distance is zero for global (sun) flares.
	struct RendererLensFlare
	{
		int SpriteID = 0;

		Vector3 Position  = Vector3::Zero;
		Vector3 Direction = Vector3::Zero;
		Color	Color	  = {};

		float Distance = 0.0f;
		bool  IsGlobal = false;
		bool  EffectsEnabled = true;
	};
}
