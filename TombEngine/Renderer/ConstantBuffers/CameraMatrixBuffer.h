#pragma once
#include <SimpleMath.h>
#include "Renderer/ConstantBuffers/ShaderLight.h"
#include "Renderer/ConstantBuffers/ShaderFogBulb.h"
#include "Renderer/RendererEnums.h"

namespace TEN::Renderer::ConstantBuffers
{
	struct CCameraMatrixBuffer
	{
		Matrix ViewProjection;
		//--
		Matrix View;
		//--
		Matrix Projection;
		//--
		Matrix InverseProjection;
		//--
		Matrix DualParaboloidView;
		//--
		Vector4 CamPositionWS;
		//--
		Vector4 CamDirectionWS;
		//--
		Vector2 ViewSize;
		Vector2 InvViewSize;
		//--
		unsigned int Frame;
		unsigned int RoomNumber;
		unsigned int CameraUnderwater;
		int Emisphere;
		//--
		int AmbientOcclusion;
		int AmbientOcclusionExponent;
		float AspectRatio;
		float TanHalfFOV;
		//--
		Vector4 FogColor;
		//--
		float FogMinDistance;
		float FogMaxDistance;
		float NearPlane;
		float FarPlane;
		//--
		int RefreshRate;
		int NumFogBulbs;
		Vector2 Padding2;
		//--
		ShaderFogBulb FogBulbs[MAX_FOG_BULBS_DRAW];
	};
}

