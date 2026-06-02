#pragma once

#include "Game/camera.h"
#include "Renderer/ConstantBuffers/CameraMatrixBuffer.h"
#include "Renderer/Frustum.h"
#include "Renderer/RendererEnums.h"
#include "Renderer/Structures/RendererEffect.h"
#include "Renderer/Structures/RendererItem.h"
#include "Renderer/Structures/RendererFogBulb.h"
#include "Renderer/Structures/RendererLight.h"
#include "Renderer/Structures/RendererRoom.h"
#include "Renderer/Structures/RendererSortableObject.h"
#include "Renderer/Structures/RendererSprite.h"
#include "Renderer/Structures/RendererSprite2D.h"
#include "Renderer/Structures/RendererSpriteToDraw.h"
#include "Renderer/Structures/RendererLensFlare.h"
#include "Renderer/Structures/RendererMirror.h"
#include "Renderer/Structures/RendererViewport.h"

namespace TEN::Renderer 
{
	using namespace TEN::Renderer::ConstantBuffers;
	using namespace TEN::Renderer::Structures;

	struct RenderViewCamera
	{
		Matrix ViewProjection = Matrix::Identity;
		Matrix View			  = Matrix::Identity;
		Matrix Projection	  = Matrix::Identity;

		Vector3 WorldPosition  = Vector3::Zero;
		Vector3 WorldDirection = Vector3::Zero;
		Vector2 ViewSize	   = Vector2::Zero;
		Vector2 InvViewSize	   = Vector2::Zero;

		Frustum Frustum	   = {};
		int		RoomNumber = 0;
		float	NearPlane  = 0.0f;
		float	FarPlane   = 0.0f;
		float	FOV		   = 0.0f;

		RenderViewCamera(const CameraInfo& camera, float roll, float fov, float nearView, float farView, float width, float height);
		RenderViewCamera(const Vector3& pos, const Vector3& dir, const Vector3& up, int roomNumber, float width, float height, float fov, float nearView, float farView);
	};

	struct RenderView
	{
		RenderViewCamera Camera;
		RendererViewport Viewport;

		std::vector<RendererRoom*>					RoomsToDraw				 = {};
		std::vector<RendererLight*>					LightsToDraw			 = {};
		std::vector<RendererFogBulb>				FogBulbsToDraw			 = {};
		std::vector<RendererSpriteToDraw>			SpritesToDraw			 = {};
		std::vector<RendererDisplaySpriteToDraw>	DisplaySpritesToDraw	 = {};
		std::map<int, std::vector<RendererStatic*>> SortedStaticsToDraw		 = {};
		std::vector<RendererSortableObject>			TransparentObjectsToDraw = {};
		std::vector<RendererLensFlare>				LensFlaresToDraw		 = {};
		std::vector<RendererMirror>					Mirrors					 = {};

		RenderView(const CameraInfo& camera, float roll, float fov, float nearPlane, float farPlane, float width, float height);
		RenderView(const Vector3& pos, const Vector3& dir, const Vector3& up, float width, float height, int roomNumber, float nearPlane, float farPlane, float fov);
		
		void FillConstantBuffer(CCameraMatrixBuffer& bufferToFill);
		void Clear();
	};
}
