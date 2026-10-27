#include "framework.h"
#include "Renderer/Renderer.h"

#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Game/Animation/Animation.h"
#include "Game/Animation/Skeleton.h"
#include "Game/camera.h"
#include "Game/collision/sphere.h"
#include "Game/control/control.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Objects/TR3/Vehicles/big_gun.h"
#include "Objects/TR3/Vehicles/big_gun_info.h"
#include "Objects/TR3/Vehicles/quad_bike.h"
#include "Objects/TR3/Vehicles/quad_bike_info.h"
#include "Objects/TR3/Vehicles/rubber_boat.h"
#include "Objects/TR3/Vehicles/rubber_boat_info.h"
#include "Objects/TR3/Vehicles/upv.h"
#include "Objects/TR3/Vehicles/upv_info.h"
#include "Objects/TR4/Vehicles/jeep.h"
#include "Objects/TR4/Vehicles/jeep_info.h"
#include "Objects/TR4/Vehicles/motorbike.h"
#include "Objects/TR4/Vehicles/motorbike_info.h"
#include "Math/Math.h"
#include "Renderer/RenderView.h"
#include "Specific/configuration.h"
#include "Specific/level.h"
#include "Specific/trutils.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Sphere;
using namespace TEN::Math;

extern GameConfiguration g_Configuration;
extern ScriptInterfaceFlowHandler *g_GameFlow;

namespace TEN::Renderer
{
    void Renderer::InvalidatePerDrawBuffer()
    {
        _perDrawBufferDirty = true;
        if (_primitiveBatchActive)
            FlushPerDrawBuffer();
    }

    void Renderer::FlushPerDrawBuffer()
    {
        if (!_perDrawBufferDirty)
            return;

        UpdateConstantBuffer(&_stPerDraw, _cbPerDraw.get());
        _perDrawBufferDirty = false;
        _numPerDrawUpdates++;
    }

    void Renderer::BeginPrimitiveBatch()
    {
        FlushPerDrawBuffer();
        _primitiveBatchActive = true;
        _primitiveBatch->Begin();
    }

    void Renderer::EndPrimitiveBatch()
    {
        _primitiveBatch->End();
        _primitiveBatchActive = false;
    }

    bool Renderer::IsBlendModeSupported(BlendMode blendMode, RendererPass rendererPass)
    {
        switch (rendererPass)
        {
        case RendererPass::GBuffer:
            return blendMode == BlendMode::Opaque || blendMode == BlendMode::AlphaTest || blendMode == BlendMode::FastAlphaBlend;
        case RendererPass::Opaque:
            return blendMode == BlendMode::Opaque || blendMode == BlendMode::AlphaTest;
        case RendererPass::Additive:
            return blendMode == BlendMode::Additive;
        case RendererPass::Distortion:
            return blendMode == BlendMode::Distortion;
        default:
            return false;
        }
    }

    bool Renderer::HasMeshForPass(const RendererMesh& mesh, RendererPass rendererPass, float alpha)
    {
        if (rendererPass == RendererPass::CollectTransparentFaces || rendererPass == RendererPass::ShadowMap)
            return true;

        for (const auto& bucket : mesh.Buckets)
        {
            if (bucket.NumVertices > 0 && IsBlendModeSupported(GetBlendModeFromAlpha(bucket.BlendMode, alpha), rendererPass))
                return true;
        }

        return false;
    }

    bool Renderer::HasItemForPass(RendererItem& item, RendererObject& object, RendererPass rendererPass)
    {
        if (GetSkinningMode(object, item.SkinIndex) == SkinningMode::Full &&
            HasMeshForPass(*GetMesh(item.SkinIndex), rendererPass, item.Color.w))
        {
            return true;
        }

        for (int meshIndex : item.MeshIndex)
        {
            if (HasMeshForPass(*GetMesh(meshIndex), rendererPass, item.Color.w))
                return true;
        }

        return false;
    }

	// Poses an object drawn without an item, as in menus, with the extra rotations set on its bones.
	void Renderer::UpdateAnimation(RendererObject& rendererObject, const FrameData& frame)
	{
		int boneCount = std::min((int)rendererObject.LinearizedBones.size(), BONE_COUNT_MAX);

		auto extraRotations = BoneRotations{};
		auto pose = SkeletonPose{};
		for (int i = 0; i < boneCount; i++)
		{
			extraRotations[i] = rendererObject.LinearizedBones[i]->ExtraRotation;
			pose.Transforms[i] = rendererObject.AnimationTransforms[i];
		}

		PoseBones((GAME_OBJECT_ID)rendererObject.Id, frame, ~0, extraRotations, pose);

		for (int i = 0; i < boneCount; i++)
			rendererObject.AnimationTransforms[i] = pose.Transforms[i];
	}

	void Renderer::UpdateItemAnimations(int itemNumber, bool force)
	{
		auto* itemToDraw = &_items[itemNumber];
		auto* nativeItem = &g_Level.Items[itemNumber];

		// TODO: hack for fixing a bug, check again if needed
		itemToDraw->ItemNumber = itemNumber;

		// Lara has her own routine
		if (nativeItem->ObjectNumber == ID_LARA)
			return;

		// Has been already done?
		if (!force && itemToDraw->DoneAnimations)
			return;

		itemToDraw->DoneAnimations = true;

		auto* obj = &Objects[nativeItem->ObjectNumber];

		if (!obj->loaded)
		{
			TENLog("Attempted to animate nonexistent object " + GetObjectName((GAME_OBJECT_ID)nativeItem->ObjectNumber), LogLevel::Error);
			return;
		}

		// Copy meshswaps and apply eventually effects.
		itemToDraw->MeshIndex = nativeItem->Model.MeshIndex;
		itemToDraw->SkinIndex = nativeItem->Model.GetSkinGlobalIndex();

		// The renderer keeps the extra rotations of each item, from which wheels and propellers keep spinning.
		auto pose = SkeletonPose{};
		GetItemPose(*nativeItem, pose, &itemToDraw->ExtraRotations);
		std::copy(pose.Transforms.begin(), pose.Transforms.end(), itemToDraw->AnimationTransforms);
		std::copy(pose.Orientations.begin(), pose.Orientations.end(), itemToDraw->BoneOrientations);
	}

	void Renderer::UpdateItemAnimations(RenderView& view)
	{
		for (const auto* room : view.RoomsToDraw)
		{
			for (const auto* itemToDraw : room->ItemsToDraw)
			{
				const auto& nativeItem = g_Level.Items[itemToDraw->ItemNumber];

				// Player has its own routine.
				if (nativeItem.ObjectNumber == ID_LARA)
					continue;

				UpdateItemAnimations(itemToDraw->ItemNumber, false);
			}
		}
	}

	void Renderer::BuildHierarchyRecursive(RendererObject *obj, RendererBone *node, RendererBone *parentNode)
	{
		node->GlobalTransform = node->Transform * parentNode->GlobalTransform;
		obj->BindPoseTransforms[node->Index] = node->GlobalTransform.Invert();
		obj->Skeleton->GlobalTranslation = Vector3::Zero;
		node->GlobalTranslation = node->Translation + parentNode->GlobalTranslation;

		for (auto* childNode : node->Children)
			BuildHierarchyRecursive(obj, childNode, node);
	}

	void Renderer::BuildHierarchy(RendererObject *obj)
	{
		obj->Skeleton->GlobalTransform = obj->Skeleton->Transform;
		obj->BindPoseTransforms[obj->Skeleton->Index] = obj->Skeleton->GlobalTransform.Invert();
		obj->Skeleton->GlobalTranslation = Vector3::Zero;

		for (auto* childNode : obj->Skeleton->Children)
			BuildHierarchyRecursive(obj, childNode, obj->Skeleton);
	}

	bool Renderer::IsFullScreen()
	{
		return (!_isWindowed);
	}

	void Renderer::UpdateCameraMatrices(CAMERA_INFO *cam, float farView)
	{
		if (farView < MIN_FAR_VIEW)
			farView = DEFAULT_FAR_VIEW;

		_currentGameCamera = RenderView(cam, cam->Roll, cam->Fov, 32, farView, g_Configuration.ScreenWidth, g_Configuration.ScreenHeight);
		_gameCamera        = RenderView(cam, cam->Roll, cam->Fov, 32, farView, g_Configuration.ScreenWidth, g_Configuration.ScreenHeight);
	}

	bool Renderer::SphereBoxIntersection(BoundingBox box, Vector3 sphereCentre, float sphereRadius)
	{
		if (sphereRadius == 0.0f)
		{
			return box.Contains(sphereCentre);
		}
		else
		{
			BoundingSphere sphere = BoundingSphere(sphereCentre, sphereRadius);
			return box.Intersects(sphere);
		}
	}

	void Renderer::FlipRooms(short roomNumber1, short roomNumber2)
	{
		std::swap(_rooms[roomNumber1], _rooms[roomNumber2]);

		_rooms[roomNumber1].RoomNumber = roomNumber1;
		_rooms[roomNumber2].RoomNumber = roomNumber2;

		_invalidateCache = true;
	}

	RendererObject& Renderer::GetRendererObject(GAME_OBJECT_ID id)
	{
		if (id == GAME_OBJECT_ID::ID_LARA || id == GAME_OBJECT_ID::ID_LARA_SKIN)
		{
			if (_moveableObjects[GAME_OBJECT_ID::ID_LARA_SKIN].has_value())
				return _moveableObjects[GAME_OBJECT_ID::ID_LARA_SKIN].value();
			else
				return _moveableObjects[GAME_OBJECT_ID::ID_LARA].value();
		}
		else
		{
			return _moveableObjects[id].value();
		}
	}

	Matrix Renderer::GetWorldMatrixForMoveable(const ItemInfo& item, Matrix* rotationMatrix, Matrix* translationMatrix) const
	{
		return GetItemWorldMatrix(item, rotationMatrix, translationMatrix);
	}

	RendererMesh* Renderer::GetMesh(int meshIndex)
	{
		return _meshes[meshIndex];
	}

	std::vector<BoundingSphere> Renderer::GetSpheres(int itemNumber)
	{
		auto& itemToDraw = _items[itemNumber];
		itemToDraw.ItemNumber = itemNumber;

		const auto* nativeItem = &g_Level.Items[itemNumber];
		if (nativeItem == nullptr)
			return {};

		if (!itemToDraw.DoneAnimations)
		{
			if (itemNumber == LaraItem->Index)
			{
				UpdateLaraAnimations(false);
			}
			else
			{
				UpdateItemAnimations(itemNumber, false);
			}
		}

		const auto& moveable = GetRendererObject(nativeItem->ObjectNumber);
		auto worldMatrix = GetWorldMatrixForMoveable(*nativeItem);

		// Collect spheres.
		auto spheres = std::vector<BoundingSphere>{};
		if (moveable.ObjectMeshes.empty())
		{
			// Virtual objects (e.g. ID_BODY_PART) have no object meshes; each instance borrows its
			// mesh per-instance, so derive the bounding spheres from the item's own mesh list.
			for (int i = 0; i < itemToDraw.MeshIndex.size(); i++)
			{
				const auto& mesh = *GetMesh(itemToDraw.MeshIndex[i]);

				const auto& animationTransform = itemToDraw.AnimationTransforms[i];
				auto pos = Vector3::Transform(mesh.Sphere.Center, animationTransform * worldMatrix);

				spheres.push_back(BoundingSphere(pos, mesh.Sphere.Radius));
			}
		}
		else
		{
			for (int i = 0; i < moveable.ObjectMeshes.size(); i++)
			{
				const auto& mesh = *moveable.ObjectMeshes[i];

				const auto& animationTransform = itemToDraw.AnimationTransforms[i];
				auto pos = Vector3::Transform(mesh.Sphere.Center, animationTransform * worldMatrix);

				auto sphere = BoundingSphere(pos, mesh.Sphere.Radius);
				spheres.push_back(sphere);
			}
		}

		return spheres;
	}

	SkinningMode Renderer::GetSkinningMode(const RendererObject& obj, int skinIndex)
	{
		if (g_GameFlow->GetSettings()->Graphics.Skinning && skinIndex != NO_VALUE)
			return SkinningMode::Full;

		if (obj.Id == GAME_OBJECT_ID::ID_LARA || obj.Id == GAME_OBJECT_ID::ID_LARA_SKIN)
			return SkinningMode::Classic;
		else
			return SkinningMode::None;
	}

	Vector4 Renderer::GetPortalRect(Vector4 v, Vector4 vp)
	{
		auto sp = (v * Vector4(0.5f, 0.5f, 0.5f, 0.5f)
			+ Vector4(0.5f, 0.5f, 0.5f, 0.5f))
			* Vector4(vp.z, vp.w, vp.z, vp.w);

		Vector4 s(sp.x + vp.x, sp.y + vp.y, sp.z + vp.x, sp.w + vp.y);

		// expand
		s.x -= 2;
		s.y -= 2;
		s.z += 2;
		s.w += 2;

		// clamp
		s.x = std::max(s.x, vp.x);
		s.y = std::max(s.y, vp.y);
		s.z = std::min(s.z, vp.x + vp.z);
		s.w = std::min(s.w, vp.y + vp.w);

		// convert from bounds to x,y,w,h
		s.z -= s.x;
		s.w -= s.y;

		// Use the viewport rect if one of the dimensions is the same size
		// as the viewport. This may fix clipping bugs while still allowing
		// impossible geometry tricks.
		if (s.z - s.x >= vp.z - vp.x || s.w - s.y >= vp.w - vp.y)
			return vp;

		return s;
	}

	float Renderer::GetFramerateMultiplier() const
	{
		return g_Configuration.EnableHighFramerate ? (g_Renderer.GetScreenRefreshRate() / (float)FPS) : 1.0f;
	}

	float Renderer::GetInterpolationFactor(bool forceRawValue) const
	{
		return (forceRawValue || g_GameFlow->CurrentFreezeMode == FreezeMode::None) ? _interpolationFactor : 0.0f;
	}

	Vector2i Renderer::GetScreenResolution() const
	{
		return Vector2i(_graphicsDevice->GetScreenWidth(), _graphicsDevice->GetScreenHeight());
	}

	int Renderer::GetScreenRefreshRate() const
	{
		return _graphicsDevice->GetRefreshRate();
	}

	std::optional<Vector2> Renderer::Get2DPosition(const Vector3& pos) const
	{
		auto point = Vector4(pos.x, pos.y, pos.z, 1.0f);
		auto cameraPos = Vector4(
			_gameCamera.Camera.WorldPosition.x,
			_gameCamera.Camera.WorldPosition.y,
			_gameCamera.Camera.WorldPosition.z,
			1.0f);
		auto cameraDir = Vector4(
			_gameCamera.Camera.WorldDirection.x,
			_gameCamera.Camera.WorldDirection.y,
			_gameCamera.Camera.WorldDirection.z,
			1.0f);

		// Point is behind camera; return nullopt.
		if ((point - cameraPos).Dot(cameraDir) < 0.0f)
			return std::nullopt;

		// Calculate clip space coords.
		point = Vector4::Transform(point, _gameCamera.Camera.ViewProjection);

		// w is close to 0; return nullopt.
		if (std::abs(point.w) <= EPSILON)
			return std::nullopt;

		// Calculate NDC.
		point /= point.w;

		// Calculate and return 2D position.
		return TEN::Utils::ConvertNDCTo2DPosition(Vector2(point));
	}

	std::pair<Vector3, Vector3> Renderer::GetRay(const Vector2& pos) const
	{
		auto nearPoint = _graphicsDevice->Unproject(Vector3(pos.x, pos.y, 0.0f), _gameCamera.Camera.Projection, _gameCamera.Camera.View, Matrix::Identity);
		auto farPoint  = _graphicsDevice->Unproject(Vector3(pos.x, pos.y, 1.0f), _gameCamera.Camera.Projection, _gameCamera.Camera.View, Matrix::Identity);

		return std::pair<Vector3, Vector3>(nearPoint, farPoint);
	}

	bool Renderer::IsRoomReflected(RenderView& renderView, int roomNumber)
	{
		for (const auto& mirror : renderView.Mirrors)
		{
			// TODO: Avoid LaraItem global.
			if (roomNumber == mirror.RoomNumber && (Camera.pos.RoomNumber == mirror.RoomNumber || LaraItem->RoomNumber == mirror.RoomNumber))
				return true;
		}

		return false;
	}

	std::string Renderer::SaveScreenshot()
	{
		char buffer[64];
		time_t rawtime;

		time(&rawtime);
		auto time = localtime(&rawtime);
		strftime(buffer, sizeof(buffer), "TEN-%Y-%m-%d_%H-%M-%S.png", time);

		auto screenPath = g_GameFlow->GetGameDir() + "Screenshots";

		if (!std::filesystem::is_directory(screenPath))
			std::filesystem::create_directory(screenPath);

		screenPath += "/";
		screenPath += buffer;

		if (!_graphicsDevice->SaveScreenshot(_backBuffer->GetRenderTarget(), screenPath))
		{
			TENLog(fmt::format("Failed to save screenshot '{}' to disk.", screenPath), LogLevel::Error);
			return std::string();
		}
		else
		{
			TENLog(fmt::format("Saved screenshot to '{}'.", screenPath), LogLevel::Info);
			return std::string(buffer);
		}
	}

	std::optional<Vector2> Renderer::ProjectDisplayItemPointToScreen(const Vector3& worldPos) const
	{
		float t = GetInterpolationFactor(true);

		Matrix viewMatrix = Matrix::CreateLookAt(
			g_DrawItems.GetInterpolatedCameraPosition(t),
			g_DrawItems.GetInterpolatedCameraTargetPosition(t),
			Vector3::Up
		);

		float aspectRatio = (float)_graphicsDevice->GetScreenWidth() / _graphicsDevice->GetScreenHeight();

		Matrix projMatrix = Matrix::CreatePerspectiveFieldOfView(
			g_DrawItems.GetInterpolatedFov(t),
			aspectRatio,
			DISPLAY_ITEM_NEAR_PLANE,
			DISPLAY_ITEM_FAR_PLANE
		);

		Matrix viewProj = viewMatrix * projMatrix;

		Vector4 p(worldPos.x, worldPos.y, worldPos.z, 1.0f);
		p = Vector4::Transform(p, viewProj);

		if (fabs(p.w) <= EPSILON)
			return std::nullopt;

		p /= p.w;

		if (p.x < -1.0f || p.x > 1.0f || p.y < -1.0f || p.y > 1.0f)
			return std::nullopt;

		float screenX = (p.x + 1.0f) * _graphicsDevice->GetScreenWidth() * 0.5f;
		float screenY = (1.0f - p.y) * _graphicsDevice->GetScreenHeight() * 0.5f;

		return Vector2(screenX, screenY);
	}

	std::optional<std::pair<Vector2, Vector2>> Renderer::GetDisplayItemBounds(const DisplayItem& item) const
	{
		float alpha = GetInterpolationFactor(true);

		// World transforms.
		auto pos    = item.GetInterpolatedPosition(alpha);
		auto orient = item.GetInterpolatedOrientation(alpha);
		float scale = item.GetInterpolatedScale(alpha).x;
		auto objectID = item.GetObjectID();

		// Find largest visible mesh sphere.
		auto& moveable = _moveableObjects[item.GetObjectID()];

		float radiusMax = 0.0f;
		auto worldCenter = Vector3::Zero;

		const auto& object = Objects[objectID];

		// Loop through meshes.
		for (int i = 0; i < moveable->ObjectMeshes.size(); i++)
		{
			if (item.GetMeshBits() && !item.GetMeshVisible(i))
				continue;

			const auto& sphere = moveable->ObjectMeshes[i]->Sphere;

			// World matrix per mesh (animation or bind-pose).
			auto meshWorldMatrix = Matrix::Identity;
			if (!object.Animations.empty())
			{
				meshWorldMatrix = moveable->AnimationTransforms[i] * Matrix::CreateScale(scale) * orient.ToRotationMatrix() * Matrix::CreateTranslation(pos);
			}
			else
			{
				meshWorldMatrix = moveable->BindPoseTransforms[i] * Matrix::CreateScale(scale) * orient.ToRotationMatrix() * Matrix::CreateTranslation(pos);
			}

			// Transform center.
			auto meshWorldCenter = Vector3::Transform(sphere.Center, meshWorldMatrix);
			float meshWorldRadius = sphere.Radius * scale;

			// Keep largest for bounding approximation.
			if (meshWorldRadius > radiusMax)
			{
				radiusMax = meshWorldRadius;
				worldCenter = meshWorldCenter;
			}
		}

		// Use default minimum radius if none found.
		if (radiusMax <= 0.0f)
			radiusMax = 10.0f;

		// Build camera matrices.
		auto camPos = g_DrawItems.GetInterpolatedCameraPosition(alpha);
		auto camTarget = g_DrawItems.GetInterpolatedCameraTargetPosition(alpha);
		auto camForward = (camTarget - camPos);
		camForward.Normalize();
		auto worldUp = Vector3::Up;
		auto camRight = camForward.Cross(worldUp);
		camRight.Normalize();
		auto camUp = camRight.Cross(camForward);
		camUp.Normalize();

		// Calculate distance from camera.
		float dist = (worldCenter - camPos).Length();

		// Build view-projection matrix.
		float aspectRatio = (float)_graphicsDevice->GetScreenWidth() / _graphicsDevice->GetScreenHeight();
		auto viewMatrix = Matrix::CreateLookAt(camPos, camTarget, Vector3::Up);
		auto projMatrix = Matrix::CreatePerspectiveFieldOfView(CurrentFOV, aspectRatio, DISPLAY_ITEM_NEAR_PLANE, DISPLAY_ITEM_FAR_PLANE);
		auto viewProj = viewMatrix * projMatrix;

		// Helper lambda to project point and clamp to extended screen bounds.
		auto projectPointClamped = [&](const Vector3& worldPos) -> Vector2
		{
			auto pos = Vector4(worldPos.x, worldPos.y, worldPos.z, 1.0f);
			pos = Vector4::Transform(pos, viewProj);

			// Handle behind camera or w near zero.
			if (pos.w <= 0.01f)
			{
				// Use estimated position based on direction.
				auto dir = worldPos - camPos;
				dir.Normalize();
				
				// Project direction onto screen plane.
				float rightDot = dir.Dot(camRight);
				float upDot = dir.Dot(camUp);
				
				// Convert to screen coordinates with large offset for off-screen.
				float screenX = rightDot * _graphicsDevice->GetScreenWidth() * 2.0f + (_graphicsDevice->GetScreenWidth() * 0.5f);
				float screenY = -upDot * _graphicsDevice->GetScreenHeight() * 2.0f + (_graphicsDevice->GetScreenHeight() * 0.5f);
				
				return Vector2(screenX, screenY);
			}

			pos /= pos.w;

			// Clamp NDC with extended margin for better size estimation.
			pos.x = std::clamp(pos.x, -3.0f, 3.0f);
			pos.y = std::clamp(pos.y, -3.0f, 3.0f);

			float screenX = (pos.x + 1.0f) * _graphicsDevice->GetScreenWidth() * 0.5f;
			float screenY = (1.0f - pos.y) * _graphicsDevice->GetScreenHeight() * 0.5f;
			return Vector2(screenX, screenY);
		};

		// Project center.
		auto center2D = projectPointClamped(worldCenter);

		// Sample points along camera right/up directions.
		auto rightWorld = worldCenter + camRight * radiusMax;
		auto leftWorld = worldCenter - camRight * radiusMax;
		auto upWorld = worldCenter + camUp * radiusMax;
		auto downWorld = worldCenter - camUp * radiusMax;

		auto rightProj = projectPointClamped(rightWorld);
		auto leftProj = projectPointClamped(leftWorld);
		auto upProj = projectPointClamped(upWorld);
		auto downProj = projectPointClamped(downWorld);

		// Calculate half extents from projected points.
		float halfWidth = std::max(std::abs(rightProj.x - center2D.x), std::abs(leftProj.x - center2D.x));
		float halfHeight = std::max(std::abs(upProj.y - center2D.y), std::abs(downProj.y - center2D.y));

		// Ensure reasonable minimum size based on screen-space estimation.
		// Calculate expected pixel size based on FOV and distance.
		float angularSize = 2.0f * atan(radiusMax / std::max(dist, 1.0f));
		float expectedPixelHeight = (angularSize / CurrentFOV) * _graphicsDevice->GetScreenHeight();
		float expectedPixelWidth = expectedPixelHeight * aspectRatio;

		// Use the larger of projected size or estimated size.
		halfWidth = std::max(halfWidth, expectedPixelWidth * 0.5f);
		halfHeight = std::max(halfHeight, expectedPixelHeight * 0.5f);

		// Ensure absolute minimum size.
		halfWidth = std::max(halfWidth, 1.0f);
		halfHeight = std::max(halfHeight, 1.0f);

		auto halfExtents = Vector2(halfWidth * 2.0f, halfHeight * 2.0f);
		return std::make_pair(center2D, halfExtents);
	}
}
