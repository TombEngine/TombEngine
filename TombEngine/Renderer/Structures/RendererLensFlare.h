#pragma once
#include "Renderer/RendererEnums.h"
		
namespace TEN::Renderer::Structures
{
	struct RendererLensFlare
	{
		int SpriteID = 0;

		Vector3 Position  = Vector3::Zero;
		Vector3 Direction = Vector3::Zero;
		Color	Color	  = {};

		float Distance = 0.0f;
		bool  IsGlobal = false;
	};
}
