#pragma once
namespace TEN::Renderer::ConstantBuffers
{
	struct alignas(16) ShaderFogBulb
	{
		Vector3 Position;
		float Density;
		// --------------------------
		Vector3 Color;
		float SquaredRadius;
		// --------------------------
		Vector3 FogBulbToCameraVector;
		float SquaredCameraToFogBulbDistance;
		// --------------------------
		Vector4 ShaderFogBulb_Padding0;
	};
}