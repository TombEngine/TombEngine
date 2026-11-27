#pragma once

#include "Game/camera.h"
#include "Renderer/Scene/Frustum.h"
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
	using namespace TEN::Renderer::Structures;

	// Camera of a render view: matrices, frustum and viewport size.
	struct RenderViewCamera
	{
		// Matrices use the row-vector convention: ViewProjection = View * Projection. Positions and sizes are in world units and pixels.
		Matrix ViewProjection;
		Matrix View;
		Matrix Projection;
		Vector3 WorldPosition;
		Vector3 WorldDirection;
		// Viewport size in pixels and its reciprocal.
		Vector2 ViewSize;
		Vector2 InvViewSize;
		// Room containing the camera: the starting point of the portal traversal.
		int RoomNumber;
		Frustum Frustum;
		float NearPlane;
		float FarPlane;
		float FOV;

		// Builds the camera from the game camera.
		RenderViewCamera(CAMERA_INFO* cam, float roll, float fov, float n, float f, int w, int h);
		// Builds the camera from an explicit position and direction. Note the argument order differs from the RenderView constructors.
		RenderViewCamera(const Vector3& pos, const Vector3& dir, const Vector3& up, int room, int width, int height, float fov, float n, float f);
	};

	// Everything collected by the frontend for one frame of a camera: visible rooms (in portal traversal order),
	// lights, fog bulbs, sprites, statics grouped by ID, transparent objects with their polygons and sort keys,
	// lens flares and mirrors. The scene renderer draws the frame from it.
	struct RenderView
	{
		RenderViewCamera Camera;

		// Lists are cleared by Clear at the start of each frame and refilled by the collection code of the renderer; the pointers refer to
		// renderer owned rooms, lights and statics, so a view must not outlive them.
		std::vector<RendererRoom*>					RoomsToDraw				 = {};
		std::vector<RendererLight*>					LightsToDraw			 = {};
		std::vector<RendererFogBulb>				FogBulbsToDraw			 = {};
		std::vector<RendererSpriteToDraw>			SpritesToDraw			 = {};
		std::vector<RendererDisplaySpriteToDraw>	DisplaySpritesToDraw	 = {};
		std::map<int, std::vector<RendererStatic*>> SortedStaticsToDraw		 = {};
		std::vector<RendererSortableObject>			TransparentObjectsToDraw  = {};
		std::vector<RendererSortablePolygon>		TransparentPolygonsToDraw = {};
		std::vector<RendererSortKey>				TransparentSortKeys		  = {};
		std::vector<RendererLensFlare>				LensFlaresToDraw		 = {};
		std::vector<RendererMirror>					Mirrors					 = {};

		// View of the game camera.
		RenderView(CAMERA_INFO* cam, float roll, float fov, float nearPlane, float farPlane, int w, int h);
		// View from an explicit camera; used by the renderer for its placeholder views.
		RenderView(const Vector3& pos, const Vector3& dir, const Vector3& up, int w, int h, int room, float nearPlane, float farPlane, float fov);
		
		// Empties the per-frame lists but keeps the camera.
		void Clear();
	};
}
