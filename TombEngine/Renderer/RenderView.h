#pragma once
#include "Game/camera.h"
#include "Renderer/ConstantBuffers/CameraMatrixBuffer.h"
#include "Renderer/Frustum.h"
#include "Renderer/RendererEnums.h"
#include "Specific/memory/LinearArrayBuffer.h"
#include "Renderer/Structures/RendererSprite2D.h"
#include "Renderer/Structures/RendererSprite.h"
#include "Renderer/Structures/RendererFogBulb.h"
#include "Renderer/Structures/RendererStatic.h"
#include "Renderer/Structures/RendererItem.h"
#include "Renderer/Structures/RendererLight.h"
#include "Renderer/Structures/RendererEffect.h"
#include "Renderer/Structures/RendererRoom.h"
#include "Renderer/Structures/RendererSortableObject.h"
#include "Renderer/Structures/RendererSpriteToDraw.h"
#include "Renderer/Structures/RendererLensFlare.h"
#include "Renderer/Structures/RendererMirror.h"

namespace TEN::Renderer 
{
	using namespace TEN::Renderer::ConstantBuffers;
	using namespace TEN::Renderer::Structures;

	struct RenderViewCamera
	{
		Matrix ViewProjection;
		Matrix View;
		Matrix Projection;
		Vector3 WorldPosition;
		Vector3 WorldDirection;
		Vector2 ViewSize;
		Vector2 InvViewSize;
		int RoomNumber;
		Frustum Frustum;
		float NearPlane;
		float FarPlane;
		float FOV;

		RenderViewCamera(CAMERA_INFO* cam, float roll, float fov, float n, float f, int w, int h);
		RenderViewCamera(const Vector3& pos, const Vector3& dir, const Vector3& up, int room, int width, int height, float fov, float n, float f);
	};

	struct RenderView
	{
		RenderViewCamera Camera;
		D3D11_VIEWPORT	 Viewport;

		std::vector<RendererRoom*>					RoomsToDraw				 = {};
		std::vector<RendererLight*>					LightsToDraw			 = {};
		std::vector<RendererFogBulb>				FogBulbsToDraw			 = {};
		std::vector<RendererSpriteToDraw>			SpritesToDraw			 = {};
		std::vector<RendererDisplaySpriteToDraw>	DisplaySpritesToDraw	 = {};
		std::map<int, std::vector<RendererStatic*>> SortedStaticsToDraw		 = {};
		std::vector<RendererSortableObject>			TransparentObjectsToDraw = {};
		std::vector<RendererLensFlare>				LensFlaresToDraw		 = {};
		std::vector<RendererMirror>					Mirrors					 = {};

		RenderView(CAMERA_INFO* cam, float roll, float fov, float nearPlane, float farPlane, int w, int h);
		RenderView(const Vector3& pos, const Vector3& dir, const Vector3& up, int w, int h, int room, float nearPlane, float farPlane, float fov);
		
		void FillConstantBuffer(CCameraMatrixBuffer& bufferToFill);
		void Clear();
	};
}
