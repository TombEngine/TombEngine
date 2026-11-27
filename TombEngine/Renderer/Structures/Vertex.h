#pragma once
#include "Math/SimpleMath.h"

namespace TEN::Renderer::Structures
{
	// Vertex of the level geometry. Normal, Tangent and FaceNormal are packed signed bytes (PackVector3); Effects packs
	// the vertex effects and the index in polygon (PackEffectsAndIndexInPoly).
	struct Vertex 
	{
		Vector3 Position			= Vector3::Zero;
		unsigned int Normal			= 0;
		Vector2 UV					= Vector2::Zero;
		unsigned int Color			= 0;
		unsigned int Tangent		= 0;
		unsigned int FaceNormal		= 0;

		// Up to four bone influences; weights are unsigned bytes (255 is full weight).
		std::array<unsigned char, 4> BoneIndex  = { 0, 0, 0, 0 };
		std::array<unsigned char, 4> BoneWeight = { 255, 0, 0, 0 };
		
		unsigned int Effects						= 0;
		// Packed by PackAnimationFrameOffsetIndexHash: hash (bits 0-7), original vertex index (bits 8-23), animation frame offset (bits 24-31).
		unsigned int AnimationFrameOffsetIndexHash	= 0;
	};
}
