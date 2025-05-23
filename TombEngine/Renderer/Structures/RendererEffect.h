#pragma once

#include "Renderer/Structures/RendererLight.h"
#include "Renderer/Structures/RendererMesh.h"

namespace TEN::Renderer::Structures
{
	struct RendererEffect
	{
		int ObjectID   = 0;
		int RoomNumber = 0;

		Vector3 Position	 = Vector3::Zero;
		Matrix	World		 = Matrix::Identity;
		Matrix	Translation	 = Matrix::Identity;
		Matrix	Rotation	 = Matrix::Identity;
		Matrix	Scale		 = Matrix::Identity;
		Vector4 Color		 = Vector4::Zero;
		Vector4 AmbientLight = Vector4::Zero;

		RendererMesh* Mesh = nullptr;
		std::vector<RendererLight*> LightsToDraw = {};

		Vector3 InterpolatedPosition	= Vector3::Zero;
		Matrix	InterpolatedWorld		= Matrix::Identity;
		Matrix	InterpolatedTranslation = Matrix::Identity;
		Matrix	InterpolatedRotation	= Matrix::Identity;
		Matrix	InterpolatedScale		= Matrix::Identity;

		Vector3 PrevPosition	= Vector3::Zero;
		Matrix	PrevWorld		= Matrix::Identity;
		Matrix	PrevTranslation = Matrix::Identity;
		Matrix	PrevRotation	= Matrix::Identity;
		Matrix	PrevScale		= Matrix::Identity;
	};
}
