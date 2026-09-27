#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::Graphics::Vertices
{
	struct Vertex 
	{
		Vector3 Position			= Vector3::Zero;
		unsigned int Normal			= 0;
		Vector2 UV					= Vector2::Zero;
		unsigned int Color			= 0;
		unsigned int Tangent		= 0;
		unsigned int FaceNormal		= 0;

		std::array<unsigned char, 4> BoneIndex  = { 0, 0, 0, 0 };
		std::array<unsigned char, 4> BoneWeight = { 255, 0, 0, 0 };
		
		unsigned int Effects						= 0;
		unsigned int AnimationFrameOffsetIndexHash	= 0;
	};
}
