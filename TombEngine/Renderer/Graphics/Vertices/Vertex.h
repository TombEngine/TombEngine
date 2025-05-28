#pragma once
#include <SimpleMath.h>

namespace TEN::Renderer::Graphics::Vertices
{
	struct Vertex 
	{
		Vector3 Position = Vector3::Zero;
		Vector3 Normal	 = Vector3::Zero;
		Vector2 UV		 = Vector2::Zero;
		Vector4 Color	 = Vector4::Zero;
		Vector3 Tangent	 = Vector3::Zero;
		Vector3 Binormal = Vector3::Zero;

		std::array<unsigned char, 4> BoneIndex  = { 0, 0, 0, 0 };
		std::array<unsigned char, 4> BoneWeight = { 255, 0, 0, 0 };
		
		unsigned int AnimationFrameOffset = 0;
		Vector4		 Effects			  = Vector4::Zero;
		unsigned int IndexInPoly		  = 0;
		unsigned int OriginalIndex		  = 0;
		unsigned int Hash				  = 0;
	};
}
