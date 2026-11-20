#include "framework.h"
#include "Renderer/Renderer.h"

#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/collision/sphere.h"
#include "Game/control/control.h"
#include "Game/itemdata/creature_info.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Math/Math.h"
#include "Renderer/Backend/SceneRenderer.h"
#include "Renderer/Scene/RenderView.h"
#include "Specific/clock.h"
#include "Specific/configuration.h"
#include "Specific/EngineMain.h"
#include "Specific/level.h"
#include "Specific/trutils.h"

using namespace TEN::Animation;
using namespace TEN::Collision::Sphere;
using namespace TEN::Math;

extern GameConfiguration g_Configuration;
extern ScriptInterfaceFlowHandler *g_GameFlow;

// Animation and query helpers: bone matrices of menu objects, skeleton bind poses, camera matrices, room flips, mesh
// spheres, frame rate and pacing, screen projection and display item bounds.

namespace TEN::Renderer
{

	// Walks the bone tree and computes the transforms of the bones of a renderer object selected by mask from an animation
	// frame, optionally blended with the previous animation. Used for objects of the menus; items are posed by the game.
	void Renderer::UpdateAnimation(RendererObject& rendererObject, const FrameData& frame, int mask, bool useObjectWorldRotation,
								   const MoveableAnimBlendData* blend, const RootMotionData* rootMotionOffset)
	{
		auto bones = std::array<RendererBone*, BONE_COUNT_MAX>{};
		int nextBoneID = 0;

		// Push skeleton.
		bones[nextBoneID++] = rendererObject.Skeleton;

		auto* transforms = rendererObject.AnimationTransforms.data();

		// Compute blend alpha.
		float blendAlpha = 0.0f;
		if (blend != nullptr)
		{
			blendAlpha = blend->GetAlpha();
		}

		// Run through bone hierarchy.
		while (nextBoneID != 0)
		{
			// Pop last bone in stack.
			auto* bone = bones[--nextBoneID];

			// Bone is nullptr; return early. NOTE: Avoids inventory crash.
			if (bone == nullptr)
				return;
			
			// Bad data; return early.
			if (frame.BoneOrientations.size() <= bone->Index)
			{
				TENLog(
					"Attempted to animate object with ID " + GetObjectName((GAME_OBJECT_ID)rendererObject.Id) +
					" using incorrect animation data. Bad animations set for slot?",
					LogLevel::Error);

				return;
			}

			// Animate bone.
			bool animateBone = (mask >> bone->Index) & 1;
			if (animateBone)
			{
				auto rootPos = frame.RootPosition;
				auto rotMatrix = Matrix::CreateFromQuaternion(frame.BoneOrientations[bone->Index]);

				// TODO: Address root motion rotation blending if any issues come up later. -- Sezz 2026.04.30

				// Apply blending.
				if (blend != nullptr)
				{
					auto offset = (rootMotionOffset != nullptr) ? rootMotionOffset->Translation : Vector3::Zero;
					rootPos = Vector3::Lerp(blend->RootPosition - offset, rootPos, blendAlpha);

					auto quat = Quaternion::Slerp(blend->BoneOrientations[bone->Index], Quaternion::CreateFromRotationMatrix(rotMatrix), blendAlpha);
					rotMatrix = Matrix::CreateFromQuaternion(quat);
				}

				auto translationMatrix = (bone == rendererObject.Skeleton) ? Matrix::CreateTranslation(rootPos) : Matrix::Identity;
				auto extraRotMatrix = Matrix::CreateFromQuaternion(bone->ExtraRotation);
				
				if (useObjectWorldRotation)
				{
					auto scale = Vector3::Zero;
					auto invQuat = Quaternion::Identity;
					auto translation = Vector3::Zero;
					transforms[bone->Parent->Index].Invert().Decompose(scale, invQuat, translation);

					rotMatrix = (rotMatrix * extraRotMatrix) * Matrix::CreateFromQuaternion(invQuat);
				}
				else
				{
					rotMatrix = extraRotMatrix * rotMatrix;
				}

				transforms[bone->Index] = rotMatrix * ((bone == rendererObject.Skeleton) ? translationMatrix : bone->Transform);
				if (bone != rendererObject.Skeleton)
					transforms[bone->Index] *= transforms[bone->Parent->Index];
			}

			// Push.
			for (auto*& child : bone->Children)
				bones[nextBoneID++] = child;
		}
	}

	// Computes the global bind pose of every bone and its inverse, used for skinning.
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

	// Rebuilds the current game camera from the game camera state; a too short far view falls back to the default.
	void Renderer::UpdateCameraMatrices(CAMERA_INFO *cam, float farView)
	{
		if (farView < MIN_FAR_VIEW)
			farView = DEFAULT_FAR_VIEW;

		_currentGameCamera = RenderView(cam, cam->Roll, cam->Fov, 32, farView, g_Configuration.ScreenWidth, g_Configuration.ScreenHeight);
		_gameCamera        = RenderView(cam, cam->Roll, cam->Fov, 32, farView, g_Configuration.ScreenWidth, g_Configuration.ScreenHeight);
	}

	// Swaps the renderer data of two rooms for a flip map and invalidates cached static data.
	void Renderer::FlipRooms(short roomNumber1, short roomNumber2)
	{
		std::swap(_rooms[roomNumber1], _rooms[roomNumber2]);

		_rooms[roomNumber1].RoomNumber = roomNumber1;
		_rooms[roomNumber2].RoomNumber = roomNumber2;

		_invalidateCache = true;
	}

	// The player object resolves to the player skin when present.
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

	// World matrix of an item (scale, rotation, translation) corrected by the root motion of its animation.
	Matrix Renderer::GetWorldMatrixForMoveable(const ItemInfo& item, Matrix* rotationMatrix, Matrix* translationMatrix) const
	{
		auto orient = item.Pose.Orientation;
		auto pos = item.Pose.Position.ToVector3();

		// Apply root motion counteraction only for animated objects. Virtual objects without
		// animations (e.g. ID_BODY_PART) would otherwise fetch a fallback animation and have their
		// world matrix polluted by bogus root motion, making them spin erratically.
		const auto& animObject = Objects[item.Animation.AnimObjectID];
		if (!animObject.Animations.empty())
		{
			const auto& anim = GetAnimData(item);
			auto rootMotionCounteract = anim.GetRootMotionCounteraction(item.Animation.FrameNumber);

			orient += rootMotionCounteract.Rotation;
			pos += Vector3::Transform(rootMotionCounteract.Translation, orient.ToRotationMatrix());
		}

		auto rotMatrix = orient.ToRotationMatrix();
		auto transMatrix = Matrix::CreateTranslation(pos);

		if (rotationMatrix != nullptr)
			*rotationMatrix = rotMatrix;

		if (translationMatrix != nullptr)
			*translationMatrix = transMatrix;

		return Matrix::CreateScale(item.Pose.Scale) * rotMatrix * transMatrix;
	}

	RendererMesh* Renderer::GetMesh(int meshIndex)
	{
		return _meshes[meshIndex];
	}

	std::vector<BoundingSphere> Renderer::GetSpheres(int itemNumber)
	{
		BoundingSphere spheres[BONE_COUNT_MAX];
		int count = GetSpheres(itemNumber, spheres, BONE_COUNT_MAX);
		return std::vector<BoundingSphere>(spheres, spheres + count);
	}

	// Writes up to maxCount mesh spheres of an item at its current pose without allocating, and returns their count.
	int Renderer::GetSpheres(int itemNumber, BoundingSphere* spheres, int maxCount)
	{
		auto& itemToDraw = _items[itemNumber];
		itemToDraw.ItemNumber = itemNumber;

		const auto* nativeItem = &g_Level.Items[itemNumber];
		if (nativeItem == nullptr)
			return 0;

		const auto& moveable = GetRendererObject(nativeItem->ObjectNumber);
		auto worldMatrix = GetWorldMatrixForMoveable(*nativeItem);

		// Virtual objects (e.g. ID_BODY_PART) have no object meshes; each instance borrows its
		// mesh per-instance, so derive the bounding spheres from the item's own mesh list.
		bool isVirtual = moveable.ObjectMeshes.empty();
		int meshCount = std::min(isVirtual ? (int)itemToDraw.MeshIndex.size() : (int)moveable.ObjectMeshes.size(), maxCount);
		for (int i = 0; i < meshCount; i++)
		{
			const auto& mesh = isVirtual ? *GetMesh(itemToDraw.MeshIndex[i]) : *moveable.ObjectMeshes[i];

			const auto& animationTransform = itemToDraw.AnimationTransforms[i];
			auto pos = Vector3::Transform(mesh.Sphere.Center, animationTransform * worldMatrix);
			spheres[i] = BoundingSphere(pos, mesh.Sphere.Radius);
		}

		return meshCount;
	}

	// Full skinning when enabled and a skin mesh exists; otherwise classic skinning for the player and rigid meshes
	// for other moveables.
	SkinningMode Renderer::GetSkinningMode(const RendererObject& obj, int skinIndex)
	{
		if (g_GameFlow->GetSettings()->Graphics.Skinning && skinIndex != NO_VALUE)
			return SkinningMode::Full;

		if (obj.Id == GAME_OBJECT_ID::ID_LARA || obj.Id == GAME_OBJECT_ID::ID_LARA_SKIN)
			return SkinningMode::Classic;
		else
			return SkinningMode::None;
	}

	// Rendered frames per game frame. Code advancing once per rendered frame divides its game frame step by it.
	float Renderer::GetFramerateMultiplier() const
	{
		return (GetFrameRate() / (float)FPS);
	}

	// Measured rate of presented frames, which differs from the display refresh rate when the frames take longer than a
	// refresh, or when the renderer presents without waiting for vertical sync.
	float Renderer::GetFrameRate() const
	{
		if (!g_Configuration.EnableHighFramerate || _frameRate <= 0.0f)
			return (float)FPS;

		return std::clamp(_frameRate, (float)FPS, (float)GetScreenRefreshRate());
	}

	// Smoothed rate of presented frames, measured between consecutive presents.
	void Renderer::UpdateFrameRate()
	{
		constexpr auto SMOOTHING		= 0.1f;
		constexpr auto INTERVAL_MIN	= 1.0f / 1000.0f;
		constexpr auto INTERVAL_MAX	= 0.25f;

		auto now = std::chrono::steady_clock::now();
		if (_lastFrameTime.time_since_epoch().count() != 0)
		{
			float interval = std::clamp(std::chrono::duration<float>(now - _lastFrameTime).count(), INTERVAL_MIN, INTERVAL_MAX);
			_frameRate = (_frameRate <= 0.0f) ? (1.0f / interval) : Lerp(_frameRate, 1.0f / interval, SMOOTHING);
		}

		_lastFrameTime = now;
	}

	// Vulkan presents the latest frame on the window thread at vertical sync, so the game thread is not paced by it. Without
	// pacing it draws frames faster than the display shows them, and the shown frames are sampled at uneven times, which
	// makes continuous motion stutter. The game thread therefore starts frames one refresh interval apart.
	void Renderer::PaceFrame()
	{
		if (!g_Configuration.EnableHighFramerate || !Backend::g_SceneRenderer.IsPresentedOnWindowThread() || !Backend::g_SceneRenderer.IsVsyncEnabled())
			return;

		auto interval = (unsigned long long)(1000000000.0 / GetScreenRefreshRate());
		auto deadline = _framePaceTime + interval;
		auto now = SDL_GetTicksNS();

		// Late frames restart the cadence, so a slow frame is not followed by a burst of unpaced ones.
		if (now >= deadline)
		{
			_framePaceTime = now;
			return;
		}

		SDL_DelayPrecise(deadline - now);
		_framePaceTime = deadline;
	}

	// Interpolation between the previous and current game frame; zero while the game is frozen unless forced.
	float Renderer::GetInterpolationFactor(bool forceRawValue) const
	{
		return (forceRawValue || g_GameFlow->CurrentFreezeMode == FreezeMode::None) ? _interpolationFactor : 0.0f;
	}

	Vector2i Renderer::GetScreenResolution() const
	{
		return Vector2i(_screenWidth, _screenHeight);
	}

	// Game logic runs at 30 frames per second; with high framerate, frames follow the display refresh rate.
	int Renderer::GetScreenRefreshRate() const
	{
		constexpr auto REFRESH_RATE_DEFAULT = 60;

		if (!g_Configuration.EnableHighFramerate)
			return FPS;

		const auto* mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(g_Platform->GetSDL3Window()));
		return ((mode != nullptr && mode->refresh_rate > 0.0f) ? (int)std::round(mode->refresh_rate) : REFRESH_RATE_DEFAULT);
	}

	// Projects a world position to 2D display space; returns nullopt for points behind the camera.
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

	// World space ray through a screen pixel, from the near plane to the far plane.
	std::pair<Vector3, Vector3> Renderer::GetRay(const Vector2& pos) const
	{
		auto viewport = TEN::Math::Library::Viewport(0.0f, 0.0f, (float)_screenWidth, (float)_screenHeight);
		auto nearPoint = viewport.Unproject(Vector3(pos.x, pos.y, 0.0f), _gameCamera.Camera.Projection, _gameCamera.Camera.View, Matrix::Identity);
		auto farPoint = viewport.Unproject(Vector3(pos.x, pos.y, 1.0f), _gameCamera.Camera.Projection, _gameCamera.Camera.View, Matrix::Identity);

		return std::pair<Vector3, Vector3>(nearPoint, farPoint);
	}

	// A room is reflected when it holds a mirror of the view and the camera or the player is in it.
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

	// Requests a timestamped screenshot in the Screenshots folder of the game directory and returns its file name.
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

		// The scene renderer saves the image when its next frame reaches the readback.
		Backend::g_SceneRenderer.RequestScreenshot(screenPath);
		TENLog(fmt::format("Saving screenshot to '{}'.", screenPath), LogLevel::Info);
		return std::string(buffer);
	}

	// Approximate screen centre and size of a display item, from its largest visible mesh sphere projected with the
	// display item camera.
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
		float aspectRatio = (float)_screenWidth / _screenHeight;
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
				float screenX = rightDot * _screenWidth * 2.0f + (_screenWidth * 0.5f);
				float screenY = -upDot * _screenHeight * 2.0f + (_screenHeight * 0.5f);
				
				return Vector2(screenX, screenY);
			}

			pos /= pos.w;

			// Clamp NDC with extended margin for better size estimation.
			pos.x = std::clamp(pos.x, -3.0f, 3.0f);
			pos.y = std::clamp(pos.y, -3.0f, 3.0f);

			float screenX = (pos.x + 1.0f) * _screenWidth * 0.5f;
			float screenY = (1.0f - pos.y) * _screenHeight * 0.5f;
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
		float expectedPixelHeight = (angularSize / CurrentFOV) * _screenHeight;
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
