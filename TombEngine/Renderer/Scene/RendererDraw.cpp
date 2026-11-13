#include "framework.h"

#include <algorithm>
#include <chrono>
#include <execution>
#include <filesystem>

#include "Game/Animation/Animation.h"
#include "Game/camera.h"
#include "Game/control/control.h"
#include "Game/control/volume.h"
#include "Game/effects/DisplaySprite.h"
#include "Game/effects/hair.h"
#include "Game/effects/tomb4fx.h"
#include "Game/effects/weather.h"
#include "Game/effects/ParticleGroup.h"
#include "Game/Gui.h"
#include "Game/Hud/Hud.h"
#include "Game/items.h"
#include "Game/Lara/lara.h"
#include "Game/savegame.h"
#include "Game/Setup.h"
#include "Objects/Generic/Object/rope.h"
#include "Objects/TR3/Entity/FishSwarm.h"
#include "Objects/TR4/Entity/tr4_beetle_swarm.h"
#include "Objects/TR4/Entity/Locust.h"
#include "Objects/TR5/Emitter/tr5_bats_emitter.h"
#include "Objects/TR5/Emitter/tr5_rats_emitter.h"
#include "Renderer/UI/ImGuiIntegration.h"
#include "Renderer/Backend/SceneRenderer.h"
#include "Game/Sky/SkyCloudDebug.h"
#include "Renderer/Scene/RenderView.h"
#include "Renderer/Renderer.h"
#include "Renderer/Structures/RendererSortableObject.h"
#include "Game/Sky/SkyCloudSystem.h"
#include "Scripting/Internal/TEN/Flow/Level/FlowLevel.h"
#include "Specific/configuration.h"
#include "Specific/level.h"
#include "Specific/trutils.h"
#include "Specific/Video/Video.h"

using namespace TEN::Animation;
using namespace TEN::Effects::Hair;
using namespace TEN::Hud;
using namespace TEN::Effects::Environment;
using namespace TEN::Effects::DisplaySprite;
using namespace TEN::Entities::Creatures::TR3;
using namespace TEN::Entities::Generic;
using namespace TEN::Renderer::Structures;
using namespace TEN::Sky;

extern GUNSHELL_STRUCT Gunshells[MAX_GUNSHELL];

// Frame flow of the frontend: PrepareScene, RenderScene (collection, sprites, sky effects, sorted faces, HUD),
// SubmitFrame (fills Backend::FrameEnvironment and calls the scene renderer) and PresentFrame (2D layer composition).
// Also holds sorted face collection of rooms, items, statics and swarms, debug primitives, dynamic lights and
// recording of 2D layer objects.

namespace TEN::Renderer
{
	// Gathers the blob shadow spheres of the frame (player body parts and shadow-casting items), keeping the
	// ShadowBlobsMax ones nearest to the player.
	void Renderer::RenderBlobShadows(RenderView& renderView)
	{
		auto& nearestSpheres = _blobShadowSpheres;
		nearestSpheres.clear();
		nearestSpheres.reserve(g_Configuration.ShadowBlobsMax);

		// Collect player spheres.
		static const std::array<LARA_MESHES, 4> sphereMeshes = { LM_HIPS, LM_TORSO, LM_LFOOT, LM_RFOOT };
		static const std::array<float, 4> sphereScaleFactors = { 6.0f, 3.2f, 2.8f, 2.8f };

		for (auto& room : renderView.RoomsToDraw)
		{
			for (auto& i : room->ItemsToDraw)
			{
				auto& nativeItem = g_Level.Items[i->ItemNumber];

				// Skip everything that isn't alive or a vehicle.
				if (Objects[nativeItem.ObjectNumber].shadowType == ShadowMode::None)
					continue;

				if (nativeItem.IsLara())
				{
					for (auto i = 0; i < sphereMeshes.size(); i++)
					{
						if (!nativeItem.MeshBits.Test(sphereMeshes[i]))
							continue;

						auto& mesh = g_Level.Meshes[nativeItem.Model.MeshIndex[sphereMeshes[i]]];

						// Push foot spheres a little lower.
						auto offset = Vector3i(mesh.sphere.Center.x, mesh.sphere.Center.y, mesh.sphere.Center.z);
						if (sphereMeshes[i] == LM_LFOOT || sphereMeshes[i] == LM_RFOOT)
							offset.y += 8;

						auto& newSphere = nearestSpheres.emplace_back();
						newSphere.Center = GetJointPosition(LaraItem, sphereMeshes[i], offset).ToVector3();
						newSphere.Radius = mesh.sphere.Radius * sphereScaleFactors[i];
					}
				}
				else
				{
					auto bounds = GameBoundingBox(&nativeItem);
					auto extents = bounds.GetExtents();
					auto center = Geometry::TranslatePoint(nativeItem.Pose.Position.ToVector3(), nativeItem.Pose.Orientation, bounds.GetCenter());
					center.y = nativeItem.Pose.Position.y;

					auto& newSphere = nearestSpheres.emplace_back();
					newSphere.Center = center;
					newSphere.Radius = std::max(abs(extents.x), abs(extents.z)) * 1.5f;
				}
			}
		}

		if (nearestSpheres.size() > g_Configuration.ShadowBlobsMax)
		{
			std::sort(nearestSpheres.begin(), nearestSpheres.end(), [](const BoundingSphere& a, const BoundingSphere& b)
				{
					auto& laraPos = LaraItem->Pose.Position;
					return Vector3::Distance(laraPos.ToVector3(), a.Center) < Vector3::Distance(laraPos.ToVector3(), b.Center);
				});

			nearestSpheres.resize(g_Configuration.ShadowBlobsMax);
		}
	}

	// Ropes are drawn as one constrained billboard per segment between interpolated joint positions.
	void Renderer::PrepareRopes(RenderView& view)
	{
		for (const auto& rope : Ropes)
		{
			if (!rope.active)
				continue;

			auto translationMatrix = Matrix::CreateTranslation(rope.position.ToVector3());
			auto absolutePoints = std::array<Vector3, 24>{};

			for (int i = 0; i < rope.segment.size(); i++)
			{
				const auto* segment = &rope.PrevMeshSegments[i];

				auto relPos = Vector3(segment->x >> FP_SHIFT, segment->y >> FP_SHIFT, segment->z >> FP_SHIFT);
				auto prevOutput = Vector3::Transform(relPos, translationMatrix);

				segment = &rope.meshSegment[i];

				relPos = Vector3(segment->x >> FP_SHIFT, segment->y >> FP_SHIFT, segment->z >> FP_SHIFT);
				auto currentOutput = Vector3::Transform(relPos, translationMatrix);

				auto absolutePos = Vector3::Lerp(prevOutput, currentOutput, GetInterpolationFactor());
				absolutePoints[i] = absolutePos;
			}

			for (int i = 0; i < (rope.segment.size() - 1); i++)
			{
				const auto& pos0 = absolutePoints[i];
				const auto& pos1 = absolutePoints[i + 1];
				auto pos = (pos0 + pos1) / 2;

				auto dir = pos1 - pos0;
				dir.Normalize();

				AddSpriteBillboardConstrained(
					&_sprites[Objects[ID_DEFAULT_SPRITES].meshIndex + SPR_EMPTY1],
					pos, _rooms[rope.room].AmbientLight * g_Level.Items[rope.index].Model.Color,
					PI_DIV_2, 1.0f, Vector2(32.0f, Vector3::Distance(pos0, pos1)), BlendMode::AlphaBlend, dir, false, view);
			}
		}
	}

	// Swarm and particle group collectors below: only buckets with sorted blend modes are collected here; the
	// opaque parts of these meshes are drawn by the scene renderer.
	void Renderer::CollectSortedRatFaces(RenderView& view)
	{
		if (!Objects[ID_RATS_EMITTER].loaded)
			return;

		for (int i = 0; i < NUM_RATS; i++)
		{
			auto* rat = &Rats[i];

			if (rat->On)
			{
				int index = (GlobalCounter + i) % Objects[ID_RATS_EMITTER].nmeshes;
				auto* mesh = GetMesh(Objects[ID_RATS_EMITTER].meshIndex + index);

				for (int j = 0; j < mesh->Buckets.size(); j++)
				{
					auto& bucket = mesh->Buckets[j];

					if (IsSortedBlendMode(bucket.BlendMode))
					{
						auto object = RendererSortableObject{};
						object.ObjectType = RendererObjectType::MoveableAsStatic;
						object.BlendMode = bucket.BlendMode;
						object.Bucket = &bucket;
						object.LightMode = mesh->LightMode;
						object.World = rat->Transform;
						object.Room = &_rooms[rat->RoomNumber];

						CollectSortedBucket(view, object, &object.World, view.Camera.WorldPosition);
					}
				}
			}
		}
	}

	void Renderer::CollectSortedFishFaces(RenderView& view)
	{
		if (!Objects[ID_FISH_EMITTER].loaded)
			return;

		for (const auto& fish : FishSwarm)
		{
			if (fish.Life <= 0.0f)
				continue;

			auto& mesh = *GetMesh(Objects[ID_FISH_EMITTER].meshIndex + fish.MeshIndex);
			auto worldMatrix = Matrix::Lerp(fish.PrevTransform, fish.Transform, GetInterpolationFactor());

			for (auto& bucket : mesh.Buckets)
			{
				if (!IsSortedBlendMode(bucket.BlendMode))
					continue;

				auto object = RendererSortableObject{};
				object.ObjectType = RendererObjectType::MoveableAsStatic;
				object.BlendMode = bucket.BlendMode;
				object.Bucket = &bucket;
				object.LightMode = mesh.LightMode;
				object.World = worldMatrix;
				object.Room = &_rooms[fish.RoomNumber];

				CollectSortedBucket(view, object, &worldMatrix, view.Camera.WorldPosition);
			}
		}
	}

	void Renderer::CollectSortedParticleGroupFaces(RenderView& view)
	{
		using namespace TEN::Effects::ParticleGroups;

		for (const auto& group : ParticleGroupList)
		{
			if (!group.Active)
				continue;

			if (!group.IsMeshGroup())
				continue;

			if (!Objects[group.ObjectID].loaded)
				continue;

			if (!_moveableObjects[group.ObjectID].has_value())
				continue;

			for (const auto& p : group.Particles)
			{
				if (!p.Active)
					continue;

				float dist = Vector3::Distance(p.Position, view.Camera.WorldPosition);
				if (dist > DEFAULT_RENDER_DISTANCE)
					continue;

				// Validate per-particle object ID (if overridden).
				if (!Objects[p.ObjectID].loaded)
					continue;

				// Ensure it's a moveable (positive nmeshes) with valid mesh data.
				if (Objects[p.ObjectID].nmeshes <= 0)
					continue;

				if (!_moveableObjects[p.ObjectID].has_value())
					continue;

				int clampedMeshIndex = std::clamp(p.SubIndex, 0, Objects[p.ObjectID].nmeshes - 1);
				auto& mesh = *GetMesh(Objects[p.ObjectID].meshIndex + clampedMeshIndex);

				// Transform is identical for all polygons of the particle; compute it once.
				auto worldMatrix = Matrix::Lerp(p.PrevTransform, p.Transform, GetInterpolationFactor());

				for (auto& bucket : mesh.Buckets)
				{
					if (!IsSortedBlendMode(bucket.BlendMode))
						continue;

					auto object = RendererSortableObject{};
					object.ObjectType = RendererObjectType::MoveableAsStatic;
					object.BlendMode = bucket.BlendMode;
					object.Bucket = &bucket;
					object.LightMode = mesh.LightMode;
					object.World = worldMatrix;
					object.Room = &_rooms[p.RoomNumber];

					CollectSortedBucket(view, object, &worldMatrix, view.Camera.WorldPosition);
				}
			}
		}
	}

	void Renderer::CollectSortedBatFaces(RenderView& view)
	{
		if (!Objects[ID_BATS_EMITTER].loaded)
			return;

		auto& mesh = *GetMesh(Objects[ID_BATS_EMITTER].meshIndex + (GlobalCounter & 3));

		for (const auto& bat : Bats)
		{
			if (!bat.On)
				continue;

			auto transformMatrix = Matrix::Lerp(bat.PrevTransform, bat.Transform, GetInterpolationFactor());

			for (auto& bucket : mesh.Buckets)
			{
				if (!IsSortedBlendMode(bucket.BlendMode))
					continue;

				auto object = RendererSortableObject{};
				object.ObjectType = RendererObjectType::MoveableAsStatic;
				object.BlendMode = bucket.BlendMode;
				object.Bucket = &bucket;
				object.LightMode = mesh.LightMode;
				object.World = transformMatrix;
				object.Room = &_rooms[bat.RoomNumber];

				CollectSortedBucket(view, object, &transformMatrix, view.Camera.WorldPosition);
			}
		}
	}

	void Renderer::CollectSortedScarabFaces(RenderView& view)
	{
		if (!Objects[ID_LITTLE_BEETLE].loaded)
			return;

		auto& mesh = *GetMesh(Objects[ID_LITTLE_BEETLE].meshIndex + ((Wibble >> 2) % 2));

		for (const auto& beetle : TEN::Entities::TR4::BeetleSwarm)
		{
			if (!beetle.On)
				continue;

			auto transformMatrix = Matrix::Lerp(beetle.PrevTransform, beetle.Transform, GetInterpolationFactor());

			for (auto& bucket : mesh.Buckets)
			{
				if (!IsSortedBlendMode(bucket.BlendMode))
					continue;

				auto object = RendererSortableObject{};
				object.ObjectType = RendererObjectType::MoveableAsStatic;
				object.BlendMode = bucket.BlendMode;
				object.Bucket = &bucket;
				object.LightMode = mesh.LightMode;
				object.World = transformMatrix;
				object.Room = &_rooms[beetle.RoomNumber];

				CollectSortedBucket(view, object, &transformMatrix, view.Camera.WorldPosition);
			}
		}
	}

	void Renderer::CollectSortedLocustFaces(RenderView& view)
	{
		if (!Objects[ID_LOCUSTS].loaded)
			return;

		auto* obj = &Objects[ID_LOCUSTS];
		auto& moveableObj = *_moveableObjects[ID_LOCUSTS];

		for (const auto& locust : TEN::Entities::TR4::Locusts)
		{
			if (!locust.On)
				continue;

			auto& mesh = *GetMesh(Objects[ID_LOCUSTS].meshIndex + (-locust.Counter & 3));
			auto transformMatrix = Matrix::Lerp(locust.PrevTransform, locust.Transform, GetInterpolationFactor());

			for (auto& bucket : mesh.Buckets)
			{
				if (!IsSortedBlendMode(bucket.BlendMode))
					continue;

				auto object = RendererSortableObject{};
				object.ObjectType = RendererObjectType::MoveableAsStatic;
				object.BlendMode = bucket.BlendMode;
				object.Bucket = &bucket;
				object.LightMode = mesh.LightMode;
				object.World = transformMatrix;
				object.Room = &_rooms[locust.RoomNumber];

				CollectSortedBucket(view, object, &transformMatrix, view.Camera.WorldPosition);
			}
		}
	}

	// Debug primitives: lines and triangles are queued only in debug mode for the current debug page, and passed to the
	// scene renderer with the frame environment. Shapes are built from lines or triangles.
	void Renderer::AddDebugLine(const Vector3& origin, const Vector3& target, const Color& color, RendererDebugPage page)
	{
		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		auto line = RendererLine3D{};
		line.Origin = origin;
		line.Target = target;
		line.Color = color;

		_lines3DToDraw.push_back(line);
	}

	void Renderer::AddDebugTriangle(const Vector3& vertex0, const Vector3& vertex1, const Vector3& vertex2, const Color& color, RendererDebugPage page)
	{
		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		auto tri = RendererTriangle3D{};
		tri.Vertices = { vertex0, vertex1, vertex2 };
		tri.Color = color;

		_triangles3DToDraw.push_back(tri);
	}

	void Renderer::AddDebugTarget(const Vector3& center, const Quaternion& orient, float radius, const Color& color, RendererDebugPage page)
	{
		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		auto rotMatrix = Matrix::CreateFromQuaternion(orient);

		auto origin0 = center + Vector3::Transform(Vector3(radius, 0.0f, 0.0f), rotMatrix);
		auto target0 = center + Vector3::Transform(Vector3(-radius, 0.0f, 0.0f), rotMatrix);
		AddDebugLine(origin0, target0, color);

		auto origin1 = center + Vector3::Transform(Vector3(0.0f, radius, 0.0f), rotMatrix);
		auto target1 = center + Vector3::Transform(Vector3(0.0f, -radius, 0.0f), rotMatrix);
		AddDebugLine(origin1, target1, color);

		auto origin2 = center + Vector3::Transform(Vector3(0.0f, 0.0f, radius), rotMatrix);
		auto target2 = center + Vector3::Transform(Vector3(0.0f, 0.0f, -radius), rotMatrix);
		AddDebugLine(origin2, target2, color);
	}

	void Renderer::AddDebugBox(const std::array<Vector3, BOX_VERTEX_COUNT>& corners, const Color& color, RendererDebugPage page, bool isWireframe)
	{
		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		// Construct box.
		if (isWireframe)
		{
			for (int i = 0; i < BOX_EDGE_COUNT; i++)
			{
				switch (i)
				{
				case 0:
					AddDebugLine(corners[0], corners[1], color, page);
					break;
				case 1:
					AddDebugLine(corners[1], corners[2], color, page);
					break;
				case 2:
					AddDebugLine(corners[2], corners[3], color, page);
					break;
				case 3:
					AddDebugLine(corners[3], corners[0], color, page);
					break;

				case 4:
					AddDebugLine(corners[4], corners[5], color, page);
					break;
				case 5:
					AddDebugLine(corners[5], corners[6], color, page);
					break;
				case 6:
					AddDebugLine(corners[6], corners[7], color, page);
					break;
				case 7:
					AddDebugLine(corners[7], corners[4], color, page);
					break;

				case 8:
					AddDebugLine(corners[0], corners[4], color, page);
					break;
				case 9:
					AddDebugLine(corners[1], corners[5], color, page);
					break;
				case 10:
					AddDebugLine(corners[2], corners[6], color, page);
					break;
				case 11:
					AddDebugLine(corners[3], corners[7], color, page);
					break;
				}
			}
		}
		else
		{
			for (int i = 0; i < BOX_FACE_COUNT; i++)
			{
				switch (i)
				{
				case 0:
					AddDebugTriangle(corners[0], corners[1], corners[2], color, page);
					AddDebugTriangle(corners[0], corners[2], corners[3], color, page);
					break;

				case 1:
					AddDebugTriangle(corners[4], corners[5], corners[6], color, page);
					AddDebugTriangle(corners[4], corners[6], corners[7], color, page);
					break;

				case 2:
					AddDebugTriangle(corners[0], corners[1], corners[4], color, page);
					AddDebugTriangle(corners[1], corners[4], corners[5], color, page);
					break;

				case 3:
					AddDebugTriangle(corners[1], corners[2], corners[5], color, page);
					AddDebugTriangle(corners[2], corners[5], corners[6], color, page);
					break;

				case 4:
					AddDebugTriangle(corners[2], corners[3], corners[6], color, page);
					AddDebugTriangle(corners[3], corners[6], corners[7], color, page);
					break;

				case 5:
					AddDebugTriangle(corners[0], corners[3], corners[4], color, page);
					AddDebugTriangle(corners[3], corners[4], corners[7], color, page);
					break;
				}
			}
		}
	}

	void Renderer::AddDebugBox(const Vector3& min, const Vector3& max, const Color& color, RendererDebugPage page, bool isWireframe)
	{
		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		auto box = BoundingBox((max + min) / 2, (max - min) / 2);
		AddDebugBox(box, color, page, isWireframe);
	}

	void Renderer::AddDebugBox(const BoundingOrientedBox& box, const Color& color, RendererDebugPage page, bool isWireframe)
	{
		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		auto corners = std::array<Vector3, BOX_VERTEX_COUNT>{};
		box.GetCorners(corners.data());

		AddDebugBox(corners, color, page, isWireframe);
	}

	void Renderer::AddDebugBox(const BoundingBox& box, const Color& color, RendererDebugPage page, bool isWireframe)
	{
		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		auto corners = std::array<Vector3, BOX_VERTEX_COUNT>{};
		box.GetCorners(corners.data());

		AddDebugBox(corners, color, page, isWireframe);
	}

	void Renderer::AddDebugCone(const Vector3& center, const Quaternion& orient, float radius, float length, const Vector4& color, RendererDebugPage page, bool isWireframe)
	{
		constexpr auto SUBDIVISION_COUNT = 32;

		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		auto rotMatrix = Matrix::CreateFromQuaternion(orient);

		auto baseVertices = std::vector<Vector3>{};
		baseVertices.reserve(SUBDIVISION_COUNT);
		float angle = 0.0f;

		// Calculate base circle vertices.
		for (int i = 0; i <= SUBDIVISION_COUNT; i++)
		{
			float sinAngle = sin(angle);
			float cosAngle = cos(angle);

			auto vertex = center + Vector3::Transform(Vector3(radius * sinAngle, radius * cosAngle, 0.0f), rotMatrix);
			baseVertices.push_back(vertex);

			angle += PI_MUL_2 / SUBDIVISION_COUNT;
		}

		auto dir = Geometry::ConvertQuatToDirection(orient);
		auto topCenter = Geometry::TranslatePoint(center, dir, length);

		// Construct cone.
		for (int i = 0; i < baseVertices.size(); i++)
		{
			const auto& vertex0 = baseVertices[i];
			const auto& vertex1 = baseVertices[(i == (baseVertices.size() - 1)) ? 0 : (i + 1)];

			if (isWireframe)
			{
				AddDebugLine(vertex0, vertex1, color);

				if ((i % (SUBDIVISION_COUNT / 8)) == 0)
					AddDebugLine(vertex0, topCenter, color);
			}
			else
			{
				AddDebugTriangle(vertex0, vertex1, center, color);
				AddDebugTriangle(vertex0, vertex1, topCenter, color);
			}
		}
	}

	void Renderer::AddDebugCylinder(const Vector3& center, const Quaternion& orient, float radius, float length, const Color& color, RendererDebugPage page, bool isWireframe)
	{
		constexpr auto SUBDIVISION_COUNT = 32;

		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		auto rotMatrix = Matrix::CreateFromQuaternion(orient);

		auto baseVertices = std::vector<Vector3>{};
		baseVertices.reserve(SUBDIVISION_COUNT);
		float angle = 0.0f;

		// Calculate base circle vertices.
		for (int i = 0; i <= SUBDIVISION_COUNT; i++)
		{
			float sinAngle = sin(angle);
			float cosAngle = cos(angle);

			auto vertex = center + Vector3::Transform(Vector3(radius * sinAngle, radius * cosAngle, 0.0f), rotMatrix);
			baseVertices.push_back(vertex);

			angle += PI_MUL_2 / SUBDIVISION_COUNT;
		}

		auto dir = Geometry::ConvertQuatToDirection(orient);

		// Calculate top circle vertices.
		auto topVertices = baseVertices;
		for (auto& vertex : topVertices)
			vertex = Geometry::TranslatePoint(vertex, dir, length);

		// Construct cylinder.
		auto topCenter = Geometry::TranslatePoint(center, dir, length);
		for (int i = 0; i < baseVertices.size(); i++)
		{
			const auto& baseVertex0 = baseVertices[i];
			const auto& baseVertex1 = baseVertices[(i == (baseVertices.size() - 1)) ? 0 : (i + 1)];

			const auto& topVertex0 = topVertices[i];
			const auto& topVertex1 = topVertices[(i == (topVertices.size() - 1)) ? 0 : (i + 1)];

			if (isWireframe)
			{
				AddDebugLine(baseVertex0, baseVertex1, color);
				AddDebugLine(topVertex0, topVertex1, color);

				if ((i % (SUBDIVISION_COUNT / 8)) == 0)
					AddDebugLine(baseVertex0, topVertex0, color);
			}
			else
			{
				AddDebugTriangle(baseVertex0, baseVertex1, center, color);
				AddDebugTriangle(topVertex0, topVertex1, topCenter, color);
				AddDebugTriangle(baseVertex0, baseVertex1, topVertex1, color);
				AddDebugTriangle(baseVertex0, topVertex0, topVertex1, color);
			}
		}
	}

	void Renderer::AddDebugSphere(const Vector3& center, float radius, const Color& color, RendererDebugPage page, bool isWireframe)
	{
		constexpr auto AXIS_COUNT = 3;
		constexpr auto SUBDIVISION_COUNT = 16;
		constexpr auto STEP_ANGLE = PI / (SUBDIVISION_COUNT / 4);

		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		// Construct sphere.
		if (isWireframe)
		{
			auto prevVertices = std::array<Vector3, AXIS_COUNT>{};
			for (int i = 0; i < (SUBDIVISION_COUNT / 4); i++)
			{
				float x = sin(STEP_ANGLE * i) * radius;
				float z = cos(STEP_ANGLE * i) * radius;

				float angle = 0.0f;

				for (int j = 0; j < SUBDIVISION_COUNT; j++)
				{
					auto vertices = std::array<Vector3, AXIS_COUNT>
					{
						center + Vector3(sin(angle) * abs(x), z, cos(angle) * abs(x)),
							center + Vector3(cos(angle) * abs(x), sin(angle) * abs(x), z),
							center + Vector3(z, sin(angle) * abs(x), cos(angle) * abs(x))
					};

					if (j > 0)
					{
						for (int k = 0; k < vertices.size(); k++)
							AddDebugLine(prevVertices[k], vertices[k], color);
					}

					prevVertices = vertices;
					angle += PI_MUL_2 / (SUBDIVISION_COUNT - 1);
				}
			}
		}
		else
		{
			auto vertices = std::vector<Vector3>{};
			for (int i = 0; i <= SUBDIVISION_COUNT; i++)
			{
				float pitch = ((float)i / SUBDIVISION_COUNT) * PI;
				float y = cos(pitch) * radius;
				float radiusAtY = sin(pitch) * radius;

				for (int j = 0; j < SUBDIVISION_COUNT; j++)
				{
					float theta = ((float)j / SUBDIVISION_COUNT) * PI_MUL_2;
					float x = cos(theta) * radiusAtY;
					float z = sin(theta) * radiusAtY;

					auto vertex = center + Vector3(x, y, z);
					vertices.push_back(vertex);
				}
			}

			for (int i = 0; i < SUBDIVISION_COUNT; i++)
			{
				for (int j = 0; j < SUBDIVISION_COUNT; j++)
				{
					int index0 = i * SUBDIVISION_COUNT + j;
					int index1 = ((i + 1) * SUBDIVISION_COUNT) + j;
					int index2 = ((i + 1) * SUBDIVISION_COUNT) + (j + 1) % SUBDIVISION_COUNT;
					int index3 = (i * SUBDIVISION_COUNT) + ((j + 1) % SUBDIVISION_COUNT);

					AddDebugTriangle(vertices[index0], vertices[index1], vertices[index2], color);
					AddDebugTriangle(vertices[index0], vertices[index2], vertices[index3], color);
				}
			}
		}
	}

	void Renderer::AddDebugSphere(const BoundingSphere& sphere, const Color& color, RendererDebugPage page, bool isWireframe)
	{
		if (_isLocked)
			return;

		if (!DebugMode || (_debugPage != page && page != RendererDebugPage::None))
			return;

		AddDebugSphere(sphere.Center, sphere.Radius, color, page, isWireframe);
	}

	// Dynamic lights added by the game code for the current frame. Ignored while the renderer is locked or the game
	// is fully frozen, since the light lists are not swapped then.
	void Renderer::AddDynamicSpotLight(const Vector3& pos, const Vector3& dir, float radius, float falloff, float distance, const Color& color, bool castShadows, int hash)
	{
		if (_isLocked || g_GameFlow->LastFreezeMode == FreezeMode::Full)
			return;

		RendererLight dynamicLight = {};

		dynamicLight.Color = Vector3(color.x, color.y, color.z);
		if (falloff < 8)
			dynamicLight.Color *= (falloff / 8.0f);

		// Calculate outer cone angle (degrees) based on radius at the cone's end distance.
		float outerConeAngle = atan(radius / distance) * (180.0f / PI);
		float innerConeAngle = atan((radius - falloff) / distance) * (180.0f / PI);
		float innerDistance = std::max(0.0f, distance - distance * (falloff / radius));

		// Normalize direction for safety.
		auto normalizedDirection = dir;
		normalizedDirection.Normalize();

		dynamicLight.RoomNumber = NO_VALUE;
		dynamicLight.Intensity = 1.0f;
		dynamicLight.Position = pos;
		dynamicLight.Direction = normalizedDirection;
		dynamicLight.In = innerDistance;
		dynamicLight.Out = distance;
		dynamicLight.InRange = innerConeAngle > 0.0f ? innerConeAngle : 0.5f;
		dynamicLight.OutRange = outerConeAngle;
		dynamicLight.Type = LightType::Spot;
		dynamicLight.CastShadows = castShadows;
		dynamicLight.BoundingSphere = BoundingSphere(pos, distance);
		dynamicLight.Luma = Luma(dynamicLight.Color);
		dynamicLight.Hash = hash;

		PrepareDynamicLight(dynamicLight);
	}

	void Renderer::AddDynamicPointLight(const Vector3& pos, float radius, const Color& color, bool castShadows, int hash)
	{
		if (_isLocked || g_GameFlow->LastFreezeMode == FreezeMode::Full)
			return;

		if (radius <= EPSILON)
			return;

		auto dynamicLight = RendererLight{};

		dynamicLight.Color = Vector3(color.x, color.y, color.z);
		if (radius < BLOCK(2))
			dynamicLight.Color *= (radius / BLOCK(2));

		dynamicLight.RoomNumber = NO_VALUE;
		dynamicLight.Intensity = 1.0f;
		dynamicLight.Position = pos;
		dynamicLight.In = 1.0f;
		dynamicLight.Out = radius;
		dynamicLight.Type = LightType::Point;
		dynamicLight.CastShadows = castShadows;
		dynamicLight.BoundingSphere = BoundingSphere(pos, radius);
		dynamicLight.Luma = Luma(dynamicLight.Color);
		dynamicLight.Hash = hash;

		PrepareDynamicLight(dynamicLight);
	}

	void Renderer::AddDynamicFogBulb(const Vector3& pos, float radius, float density, const Color& color, int hash)
	{
		if (_isLocked || g_GameFlow->LastFreezeMode == FreezeMode::Full)
			return;

		auto dynamicLight = RendererLight{};

		dynamicLight.Color = Vector3(color.x, color.y, color.z);

		dynamicLight.RoomNumber = NO_VALUE;
		dynamicLight.Intensity = density;
		dynamicLight.Position = pos;
		dynamicLight.In = 1.0f;
		dynamicLight.Out = radius;
		dynamicLight.Type = LightType::FogBulb;
		dynamicLight.CastShadows = false;
		dynamicLight.BoundingSphere = BoundingSphere(pos, radius);
		dynamicLight.Luma = Luma(dynamicLight.Color);
		dynamicLight.Hash = hash;

		PrepareDynamicLight(dynamicLight);
	}

	// Queues a dynamic light, taking its previous position and direction from the light with the same hash of the
	// previous frame (for interpolation), and adds its reflection for mirrors that reflect lights.
	void Renderer::PrepareDynamicLight(RendererLight& light)
	{
		// If hash is provided, search for same light in previous buffer.
		if (light.Hash != 0)
		{
			// Determine previous buffer index.
			const auto& prevList = _dynamicLights[1 - _dynamicLightList];

			// Find light in previous buffer with same hash.
			auto it = std::find_if(
				prevList.begin(), prevList.end(),
				[&light](const auto& prevLight)
				{
					return (prevLight.Hash == light.Hash);
				});

			if (it != prevList.end())
			{
				// If matching light is found, copy it.
				const auto& prevLight = *it;
				light.PrevPosition = prevLight.Position;
				light.PrevDirection = prevLight.Direction;
			}
		}

		// Queue dynamic light.
		_dynamicLights[_dynamicLightList].push_back(light);

		// Check if light is spawned in mirrored room and create reflection.
		for (const auto& mirror : g_Level.Mirrors)
		{
			if (!mirror.ReflectLights)
				continue;

			// TODO: Avoid LaraItem global.
			if ((Camera.pos.RoomNumber == mirror.RoomNumber || LaraItem->RoomNumber == mirror.RoomNumber) &&
				IsPointInRoom(light.Position, mirror.RoomNumber))
			{
				auto reflectedLight = light;
				reflectedLight.Position = Vector3::Transform(light.Position, mirror.ReflectionMatrix);
				reflectedLight.Direction = Vector3::Transform(light.Direction, mirror.ReflectionMatrix);
				reflectedLight.Hash = 0;

				_dynamicLights[_dynamicLightList].push_back(reflectedLight);
			}
		}
	}

	// Called at the start of a game frame: swaps the double-buffered dynamic light lists, clears per-frame debug
	// primitives and strings, updates the blink color and unlocks the renderer for new additions.
	void Renderer::PrepareScene()
	{
		if (g_GameFlow->CurrentFreezeMode != FreezeMode::Full &&
			g_Gui.GetInventoryMode() == InventoryMode::None)
		{
			_dynamicLightList ^= 1;
			_dynamicLights[_dynamicLightList].clear();
		}

		_lines3DToDraw.clear();
		_triangles3DToDraw.clear();
		_stringsToDraw.clear();
		_debugMessages.clear();

		constexpr auto BLINK_VALUE_MAX = 1.0f;
		constexpr auto BLINK_VALUE_MIN = 0.1f;
		constexpr auto BLINK_TIME_STEP = 0.2f;

		// Calculate blink increment based on sine wave.
		float blink = ((sin(_blinkTime) + BLINK_VALUE_MAX) * 0.5f) + BLINK_VALUE_MIN;
		_blinkColorValue = Vector4(blink, blink, blink, 1.0f);

		// Update blink time.
		_blinkTime += BLINK_TIME_STEP;
		if (_blinkTime > PI_MUL_2)
			_blinkTime -= PI_MUL_2;

		_oldGameCamera = _currentGameCamera;
		Camera.DisableInterpolation = false;

		_isLocked = false;

		DrawDebugInfo(_gameCamera);
	}

	void Renderer::ClearScene()
	{
		_gameCamera.Clear();

		ResetItems();
	}

	// CPU frontend of the frame: culls the view, updates animations, lights, sprites, sky effects and sorted faces,
	// records the HUD and 2D layer, and passes everything to the scene renderer.
	void Renderer::RenderScene(RenderView& view, SceneRenderMode renderMode)
	{
		using ns = std::chrono::nanoseconds;
		using ms = std::chrono::duration<float, std::milli>;

		ResetDebugVariables();

		// Cull rooms through portals and collect their contents.
		auto time1 = std::chrono::high_resolution_clock::now();
		CollectMirrors(view);
		CollectRooms(view, false);
		auto time = std::chrono::high_resolution_clock::now();
		_timeRoomsCollector = (std::chrono::duration_cast<ns>(time - time1)).count() / 1000000;
		time1 = time;

		// Animations, shadow light, blob shadows and sprites.
		UpdateLaraAnimations(false);
		UpdateItemAnimations(view);
		CollectLightsForCamera();
		RenderBlobShadows(view);
		PrepareSceneSprites(view);

		auto time2 = std::chrono::high_resolution_clock::now();
		_timeUpdate = (std::chrono::duration_cast<ns>(time2 - time1)).count() / 1000000;
		time1 = time2;

		PrepareSkyEffects(view);

		// Transparent faces, collected and sorted back to front.
		auto sortedTime0 = std::chrono::high_resolution_clock::now();
		CollectSortedFaces(view);
		auto sortedTime1 = std::chrono::high_resolution_clock::now();
		SortTransparentFaces(view);
		auto sortedTime2 = std::chrono::high_resolution_clock::now();
		_timeSortedCollect = ms(sortedTime1 - sortedTime0).count();
		_timeSortedSort = ms(sortedTime2 - sortedTime1).count();

		CollectHud(view, renderMode);

		time2 = std::chrono::high_resolution_clock::now();
		_timeFrame = (std::chrono::duration_cast<ns>(time2 - time1)).count() / 1000000;

		// Hand the view to the scene renderer; the 2D layer is composed later by PresentFrame.
		CollectFrameStats(view);
		SubmitFrame(view);

		ClearScene();
		CalculateFrameRate();
	}

	// Collects the sprites of all effects into the view. SortAndPrepareSprites then moves sprites with sorted blend
	// modes to the sorted faces; the others stay in the view list and are batched by the scene renderer.
	void Renderer::PrepareSceneSprites(RenderView& view)
	{
		PrepareFires(view);
		PrepareSmokes(view);
		PrepareSmokeParticles(view);
		PrepareSimpleParticles(view);
		PrepareParticleGroups(view);
		PrepareSparkParticles(view);
		PrepareExplosionParticles(view);
		PrepareFootprints(view);
		PrepareBlood(view);
		PrepareWeatherParticles(view);
		PrepareParticles(view);
		PrepareBubbles(view);
		PrepareDrips(view);
		PrepareRipples(view);
		PrepareUnderwaterBloodParticles(view);
		PrepareSplashes(view);
		PrepareShockwaves(view);
		PrepareElectricity(view);
		PrepareHelicalLasers(view);
		PrepareRopes(view);
		PrepareStreamers(view);
		PrepareLaserBarriers(view);
		PrepareSingleLaserBeam(view);
		PrepareFireflies(view);

		SortAndPrepareSprites(view);
	}

	// HUD objects, HUD bars and texts, screen overlays and display sprites of the 2D layer.
	void Renderer::CollectHud(RenderView& view, SceneRenderMode renderMode)
	{
		bool isHudDrawn = (renderMode == SceneRenderMode::Full && g_GameFlow->LastGameStatus == GameStatus::Normal);
		if (isHudDrawn)
		{
			g_Hud.Draw3D();
			g_DrawItems.Draw();
		}

		ClearDrawPhaseDisplaySprites();
		if (isHudDrawn)
			g_Hud.Draw2D(*LaraItem);

		DrawOverlays(view);

		if (isHudDrawn)
		{
			CollectDisplaySprites(view);
			DrawDisplaySprites(view, false);

			// Strings in the title level are drawn in a separate menu pass.
			if (CurrentLevel != 0)
				DrawAllStrings();

			DrawDisplaySprites(view, true);
		}
	}

	// Distance fog of the level; a zero maximum distance turns it off.
	static void SetFrameFog(Backend::FrameEnvironment& environment)
	{
		const auto& level = *g_GameFlow->GetLevel(CurrentLevel);
		if (level.GetFogMaxDistance() <= 0)
			return;

		auto fogColor = level.GetFogColor();
		environment.FogColor = Vector4(fogColor.GetR() / 255.0f, fogColor.GetG() / 255.0f, fogColor.GetB() / 255.0f, 1.0f);
		environment.FogMinDistance = level.GetFogMinDistance();
		environment.FogMaxDistance = level.GetFogMaxDistance();
	}

	// Fills the Backend::FrameEnvironment of the frame (camera, fog, post-process, video, sky draws and uniforms,
	// depth of field, lens flares, debug primitives and shadows) and renders the 3D scene with the scene renderer.
	void Renderer::SubmitFrame(RenderView& view)
	{
		if (!Backend::g_SceneRenderer.IsActive())
			return;

		// Camera, fog, timing and post-process settings.
		auto environment = Backend::FrameEnvironment{};
		environment.CameraPosition = view.Camera.WorldPosition;
		SetFrameFog(environment);
		environment.Frame = GlobalCounter;
		environment.InterpolatedFrame = (float)GlobalCounter + GetInterpolationFactor();
		environment.InterpolationFactor = GetInterpolationFactor();
		environment.IsCameraUnderwater = ((g_Level.Rooms[view.Camera.RoomNumber].flags & ENV_FLAG_WATER) != 0);
		environment.IsAmbientOcclusionEnabled = (g_GameFlow->GetSettings()->Graphics.AmbientOcclusion && g_Configuration.EnableAmbientOcclusion);
		environment.Tint = _postProcessTint;
		environment.Gamma = g_Configuration.Gamma;
		environment.ColorScheme = (int)_postProcessMode;
		environment.ColorSchemeStrength = _postProcessStrength;
		environment.ScreenFade = ScreenFadeCurrent;
		environment.CinematicBarsHeight = CinematicBarsHeight;
		environment.AntialiasingMode = (int)g_Configuration.AntialiasingMode;

		const auto& graphics = g_GameFlow->GetSettings()->Graphics;
		environment.IsHdrEnabled = (graphics.Hdr && g_Configuration.EnableHdr);
		environment.Exposure = graphics.Exposure;
		environment.BloomIntensity = graphics.BloomIntensity;
		environment.BloomThreshold = graphics.BloomThreshold;

		// 0 is the dynamic scale, which holds the refresh rate with high framerate and 30 frames per second without.
		constexpr auto PERFORMANCE_RENDER_SCALE = 1.0f / 1.5f;
		environment.RenderScale = (g_Configuration.RenderScale == RenderScaleMode::Automatic) ? 0.0f :
			((g_Configuration.RenderScale == RenderScaleMode::Performance) ? PERFORMANCE_RENDER_SCALE : 1.0f);
		environment.TargetFrameTime = 1000.0f / (float)(g_Configuration.EnableHighFramerate ? GetScreenRefreshRate() : FPS);

		// Background video, shown by video animated sets and sprites while it plays.
		environment.VideoSprite = &_videoSprite;
		environment.WhiteSprite = &_whiteSprite;
		if (IsVideoSpriteActive())
		{
			auto videoSize = TEN::Video::g_VideoPlayer.GetFrameSize();
			environment.VideoFrame = (const unsigned char*)TEN::Video::g_VideoPlayer.GetFrameData();
			environment.VideoWidth = videoSize.x;
			environment.VideoHeight = videoSize.y;
		}

		// Stars, meteors and the sun sprite.
		_skySprites.clear();
		float starfieldAlphaScale = GetStarfieldAlphaScale();
		for (const auto& star : Weather.GetStars())
			_skySprites.push_back(GetStarSprite(star, view, starfieldAlphaScale));

		for (const auto& meteor : Weather.GetMeteors())
		{
			if (meteor.Active)
				_skySprites.push_back(GetMeteorSprite(meteor, view, starfieldAlphaScale));
		}

		if (GetSunSprite(view, _sunSprite))
			environment.SkySunSprite = &_sunSprite;

		// Sky draw records and uniforms prepared by PrepareSkyEffects.
		environment.SkySprites = &_skySprites;
		environment.CloudDraws = &_cloudDraws;
		environment.CloudNoise = Backend::CloudNoiseData{ _cloudNoiseTextures.Noise3DData.data(), _cloudNoiseTextures.Noise3DResolution, _cloudNoiseTextures.Worley2DData.data(), _cloudNoiseTextures.Worley2DResolution };

		environment.SkyEffects = _skyEffects;
		environment.GodRays = _godRayDraw;
		environment.HorizonLayers = _horizonLayers;
		environment.DustStorm = _dustStormDraw;
		if (_skyEffects.IsDomeDrawn || _skyEffects.IsSunMoonDiscDrawn || _skyEffects.IsAuroraDrawn)
		{
			environment.AtmosphericSkyUniforms = &_stAtmosphericSky;
			environment.AtmosphericSkyUniformsSize = (int)sizeof(_stAtmosphericSky);
		}

		// Depth of field, lens flares, debug primitives and shadows.
		environment.DofMode = (int)_currentDOF.Mode;
		environment.DofDistance = _currentDOF.Distance;
		environment.DofRange = _currentDOF.Range;
		environment.DofStrength = _currentDOF.Strength;

		float cloudOcclusion = GetLensFlareCloudOcclusion();
		environment.LensFlareCount = std::min((int)view.LensFlaresToDraw.size(), Backend::LENS_FLARES_MAX);
		for (int i = 0; i < environment.LensFlareCount; i++)
		{
			environment.LensFlarePositions[i] = view.LensFlaresToDraw[i].Position;
			environment.LensFlareColors[i] = GetLensFlareColor(view.LensFlaresToDraw[i], cloudOcclusion);
		}
		environment.DebugLines = &_lines3DToDraw;
		environment.DebugTriangles = &_triangles3DToDraw;
		environment.ShadowLight = _shadowLight;
		environment.IsShadowMapEnabled = (g_Configuration.ShadowType != ShadowMode::None);
		environment.IsPlayerShadowOnly = (g_Configuration.ShadowType == ShadowMode::Player);
		environment.ShadowMapSize = g_Configuration.ShadowMapSize;
		environment.ShadowSphereCount = std::min((int)_blobShadowSpheres.size(), Backend::SHADOW_SPHERES_MAX);
		for (int i = 0; i < environment.ShadowSphereCount; i++)
			environment.ShadowSpheres[i] = Vector4(_blobShadowSpheres[i].Center.x, _blobShadowSpheres[i].Center.y, _blobShadowSpheres[i].Center.z, _blobShadowSpheres[i].Radius);

		Backend::g_SceneRenderer.RenderFrame(view, environment);
	}

	// Exclusive video playback: the current frame of the video handler over the whole screen.
	void Renderer::RenderVideoFrame(float aspect)
	{
		auto video = RendererImage{ Backend::OVERLAY_VIDEO_PATH, _videoSprite.Width, _videoSprite.Height };
		DrawFullScreenQuad(video, Vector3::One, true, aspect);

		ClearScene();

		_uiFrame.BackgroundFade = 0.0f;
		PresentFrame();
	}

	// Renders the game scene without presenting it (e.g. behind menus), optionally blurred with full-screen depth of field.
	void Renderer::DumpGameScene(SceneRenderMode renderMode, float blur)
	{
		if (blur > EPSILON)
			SetDOF({ DOFMode::Full, 0, 0, blur }, false);

		RenderScene(_gameCamera, renderMode);

		if (blur > EPSILON)
			RestoreDOF();
	}

	// Collects the faces with sorted blend modes of rooms, moveables, effects, swarms, particle groups and statics.
	// Sprites with sorted blend modes were already added by SortAndPrepareSprites.
	void Renderer::CollectSortedFaces(RenderView& view)
	{
		CollectSortedRoomFaces(view);
		CollectSortedItemFaces(view);
		CollectSortedEffectFaces(view);
		CollectSortedScarabFaces(view);
		CollectSortedBatFaces(view);
		CollectSortedRatFaces(view);
		CollectSortedLocustFaces(view);
		CollectSortedFishFaces(view);
		CollectSortedParticleGroupFaces(view);
		CollectSortedStaticFaces(view);
	}

	void Renderer::CollectSortedItemFaces(RenderView& view)
	{
		for (auto room : view.RoomsToDraw)
		{
			for (auto itemToDraw : room->ItemsToDraw)
			{
				switch (itemToDraw->ObjectID)
				{
				case ID_LARA:
					CollectSortedPlayerFaces(view);
					continue;

				default:
					CollectSortedMoveableFaces(itemToDraw, view);
					continue;
				}
			}
		}
	}

	void Renderer::CollectSortedMoveableFaces(RendererItem* item, RenderView& view)
	{
		ItemInfo* nativeItem = &g_Level.Items[item->ItemNumber];
		RendererObject& moveableObj = *_moveableObjects[item->ObjectID];

		auto skinMode = GetSkinningMode(moveableObj, item->SkinIndex);

		for (int k = 0; k < item->MeshIndex.size(); k++)
		{
			if (!nativeItem->MeshBits.Test(k))
				continue;

			if (skinMode == SkinningMode::Full && g_Level.Meshes[nativeItem->Model.MeshIndex[k]].hidden)
				continue;

			CollectSortedMeshFaces(item, GetMesh(item->MeshIndex[k]), RendererObjectType::Moveable, k, false, view);
		}
	}

	void Renderer::CollectSortedStaticFaces(RenderView& view)
	{
		if (view.SortedStaticsToDraw.empty())
			return;

		// Statics are grouped by static ID; buckets become sorted when their blend mode or the static alpha requires it.
		for (auto it = view.SortedStaticsToDraw.begin(); it != view.SortedStaticsToDraw.end(); it++)
		{
			const auto& statics = it->second;

			auto* refStatic = statics[0];
			auto& refStaticObj = GetStaticRendererObject(refStatic->ObjectNumber);
			if (refStaticObj.ObjectMeshes.size() == 0)
				continue;

			auto* refMesh = refStaticObj.ObjectMeshes[0];

			for (int i = 0; i < statics.size(); i++)
			{
				for (int j = 0; j < refMesh->Buckets.size(); j++)
				{
					auto& bucket = refMesh->Buckets[j];

					if (bucket.NumVertices == 0)
						continue;

					auto blendMode = GetBlendModeFromAlpha(bucket.BlendMode, statics[i]->Color.w);

					if (IsSortedBlendMode(blendMode) || statics[i]->Color.w < ALPHA_BLEND_THRESHOLD)
					{
						auto object = RendererSortableObject{};
						object.ObjectType = RendererObjectType::Static;
						object.Bucket = &bucket;
						object.Static = statics[i];
						object.BlendMode = blendMode;
						object.LightMode = refMesh->LightMode;
						object.Room = &_rooms[statics[i]->RoomNumber];

						CollectSortedBucket(view, object, &statics[i]->World, view.Camera.WorldPosition);
					}
				}
			}
		}
	}

	void Renderer::CollectSortedRoomFaces(RenderView& view)
	{
		for (int i = (int)view.RoomsToDraw.size() - 1; i >= 0; i--)
		{
			int index = i;
			RendererRoom* room = view.RoomsToDraw[index];

			for (int j = 0; j < room->Buckets.size(); j++)
			{
				auto& bucket = room->Buckets[j];

				if (IsSortedBlendMode(bucket.BlendMode))
				{
					auto object = RendererSortableObject{};
					object.ObjectType = RendererObjectType::Room;
					object.BlendMode = bucket.BlendMode;
					object.Bucket = &bucket;
					object.Room = room;

					CollectSortedBucket(view, object, nullptr, view.Camera.WorldPosition);
				}
			}
		}
	}

	// When the atmospheric sky dome is active, star and meteor visibility follows the day/night factor.
	float Renderer::GetStarfieldAlphaScale() const
	{
		constexpr auto SHORT_TO_RAD = PI_MUL_2 / 65536.0f;

		auto* levelPtr = g_GameFlow->GetLevel(CurrentLevel);
		if (!_atmosphericSkySettings.Enabled || !levelPtr->GetLensFlareEnabled())
			return 1.0f;

		float pitch = (float)levelPtr->GetLensFlarePitch() * SHORT_TO_RAD;
		return ComputeStarfieldVisibility(std::sin(pitch));
	}

	RendererSpriteToDraw Renderer::GetStarSprite(const StarParticle& star, const RenderView& renderView, float alphaScale)
	{
		constexpr auto STAR_SIZE = 2;

		auto sprite = RendererSpriteToDraw{};
		sprite.Sprite = &_sprites[Objects[ID_DEFAULT_SPRITES].meshIndex + SPR_LENS_FLARE_3];
		sprite.Type = SpriteType::Billboard;
		sprite.pos = renderView.Camera.WorldPosition + star.Direction * BLOCK(1);
		sprite.Rotation = 0;
		sprite.Scale = 1;
		sprite.Width = STAR_SIZE * star.Scale;
		sprite.Height = STAR_SIZE * star.Scale;

		// Underwater sky occlusion: stars below the water-line are fully hidden.
		// TEN Y-down: star.Direction.y < 0 means the star is above the horizon.
		float underwaterStarMask = 1.0f;
		if (_underwaterSkySettings.Enabled && _stAtmosphericSky.UnderwaterSkyVisibility > 0.001f)
		{
			float upY = -star.Direction.y;
			float layerH = std::max(_underwaterSkySettings.LayerHeight, 0.05f);
			float soft   = std::max(_underwaterSkySettings.HorizonSoftness, 0.005f);
			float t = std::clamp((upY - (layerH - soft)) / (2.0f * soft), 0.0f, 1.0f);
			float aboveFactor = t * t * (3.0f - 2.0f * t);
			underwaterStarMask = Lerp(1.0f, aboveFactor * 0.3f, _stAtmosphericSky.UnderwaterSkyVisibility);
		}

		sprite.color = Vector4(star.Color.x, star.Color.y, star.Color.z, star.Blinking * star.Extinction * alphaScale * underwaterStarMask);
		sprite.c1 = sprite.c2 = sprite.c3 = sprite.c4 = sprite.color;
		return sprite;
	}

	RendererSpriteToDraw Renderer::GetMeteorSprite(const MeteorParticle& meteor, const RenderView& renderView, float alphaScale)
	{
		auto sprite = RendererSpriteToDraw{};
		sprite.Sprite = &_sprites[Objects[ID_DEFAULT_SPRITES].meshIndex + SPR_LENS_FLARE_3];
		sprite.Type = SpriteType::CustomBillboard;
		sprite.pos = renderView.Camera.WorldPosition + Vector3::Lerp(meteor.PrevPosition, meteor.Position, GetInterpolationFactor());
		sprite.Rotation = 0;
		sprite.Scale = 1;
		sprite.Width = 2;
		sprite.Height = 192;
		sprite.ConstrainAxis = meteor.Direction;
		sprite.color = Vector4(meteor.Color.x, meteor.Color.y, meteor.Color.z, Lerp(meteor.PrevFade, meteor.Fade, GetInterpolationFactor()) * alphaScale);
		sprite.c1 = sprite.c2 = sprite.c3 = sprite.c4 = sprite.color;
		return sprite;
	}

	// Sun sprite of the global lens flare, drawn when the atmospheric sky dome (which has its own sun) is off.
	bool Renderer::GetSunSprite(const RenderView& renderView, RendererSpriteToDraw& sprite)
	{
		constexpr auto SUN_SIZE = 64;

		if (renderView.LensFlaresToDraw.empty() || !renderView.LensFlaresToDraw[0].IsGlobal || _atmosphericSkySettings.Enabled)
			return false;

		const auto& lensFlare = renderView.LensFlaresToDraw[0];

		// Attenuate sun sprite by volumetric cloud transmittance, as the lens flare halo.
		float sunCloudOcclusion = GetLensFlareCloudOcclusion();
		auto sunColor = Color(lensFlare.Color.x * sunCloudOcclusion, lensFlare.Color.y * sunCloudOcclusion, lensFlare.Color.z * sunCloudOcclusion, lensFlare.Color.w);

		// Scale sun sprite larger near horizon. TombEngine uses Y-down: Direction.y is -1 at zenith, ~0 at horizon.
		float sunElevation = -lensFlare.Direction.y;
		float sunSizeScale = 1.0f + (1.0f - std::clamp(sunElevation, 0.0f, 1.0f)) * 0.8f;

		// Fade the sun sprite below the horizon: full at elevation 0, gone by about -7 degrees.
		float sunHorizonFade = std::clamp(1.0f + sunElevation * 8.0f, 0.0f, 1.0f);
		sunHorizonFade = sunHorizonFade * sunHorizonFade * (3.0f - 2.0f * sunHorizonFade);
		sunColor = Color(sunColor.x * sunHorizonFade, sunColor.y * sunHorizonFade, sunColor.z * sunHorizonFade, sunColor.w);

		sprite = RendererSpriteToDraw{};
		sprite.Sprite = &_sprites[Objects[ID_DEFAULT_SPRITES].meshIndex + lensFlare.SpriteID];
		sprite.Type = SpriteType::Billboard;
		sprite.pos = renderView.Camera.WorldPosition + lensFlare.Direction * BLOCK(1);
		sprite.Rotation = 0.0f;
		sprite.Scale = 1.0f;
		sprite.Width = SUN_SIZE * sunSizeScale;
		sprite.Height = SUN_SIZE * sunSizeScale;
		sprite.color = sunColor;
		sprite.c1 = sprite.c2 = sprite.c3 = sprite.c4 = sprite.color;
		return true;
	}

	// Entry point of a game frame: interpolates the camera, renders the scene, records the ImGui debug overlay and
	// presents the frame.
	void Renderer::Render(float interpFactor)
	{
		auto renderStartTime = std::chrono::steady_clock::now();

		bool captureScreenshots = TEN::Benchmark::ConsumeBenchmarkScreenshotRequest(GlobalCounter);
		if (captureScreenshots && Backend::g_SceneRenderer.IsActive())
			Backend::g_SceneRenderer.RequestScreenshot(g_GameFlow->GetGameDir() + "Logs/" + TEN::Benchmark::GetBenchmarkScreenshotName());

		// Screenshots show the game frame itself: the interpolation between game frames depends on the frame pacing of the run.
		if (captureScreenshots)
			interpFactor = 1.0f;

		InterpolateCamera(interpFactor);

		RenderScene(_gameCamera);

		// ImGui debug overlay, shown in debug mode with the performance panel or a debug page. Drawn after RenderScene, so it is on top.
		ImGuiSetOverlayVisible(IsDebugOverlayVisible());
		ImGuiNewFrame();
		if (ImGuiIsOverlayVisible())
			DrawDebugOverlay(_gameCamera);

		ImGuiRenderFrame();
		_uiFrame.DebugDrawData = ImGuiGetDrawData();

		auto presentStartTime = std::chrono::steady_clock::now();
		PresentFrame();

		RecordBenchmarkFrame(renderStartTime, presentStartTime);
	}

	// Extends the last layer of the scene renderer 2D layer when the element follows it, or starts a new layer.
	void Renderer::AddUiLayer(Backend::OverlayLayerType type, int first)
	{
		auto& layers = _uiFrame.Layers;
		if (!layers.empty() && layers.back().Type == type && layers.back().First + layers.back().Count == first && type != Backend::OverlayLayerType::ScreenOverlays)
		{
			layers.back().Count++;
			return;
		}

		layers.push_back(Backend::OverlayLayer{ type, first, 1 });
	}

	// Adds an image file to the 2D layer; images without a file path are skipped.
	void Renderer::AddUiImage(const std::string& path, const Vector2& min, const Vector2& max, const Vector2& uvMin, const Vector2& uvMax, const Vector4& color, BlendMode blendMode)
	{
		if (path.empty())
			return;

		_uiFrame.Images.push_back(Backend::OverlayImage{ path, min, max, uvMin, uvMax, color, blendMode });
		AddUiLayer(Backend::OverlayLayerType::Images, (int)_uiFrame.Images.size() - 1);
	}

	// Starts a 3D object of the 2D layer (inventory and HUD objects); its meshes follow with AddUiObjectMesh.
	void Renderer::BeginUiObject(const Matrix& viewProjection, const Vector4& color, const Vector4& ambientLight, unsigned int frame, float interpolatedFrame, bool isAlphaTested)
	{
		if (!Backend::g_SceneRenderer.IsActive())
			return;

		auto object = Backend::OverlayObject{ viewProjection, color, ambientLight, frame, interpolatedFrame, isAlphaTested, (int)_uiFrame.ObjectMeshes.size(), 0 };
		_uiFrame.Objects.push_back(object);
		AddUiLayer(Backend::OverlayLayerType::Objects, (int)_uiFrame.Objects.size() - 1);
	}

	// Adds a mesh to the last 2D layer object, with optional bone matrices for skinned meshes.
	void Renderer::AddUiObjectMesh(const RendererMesh* mesh, const Matrix& world, const Matrix* bones, int boneCount)
	{
		if (!Backend::g_SceneRenderer.IsActive() || _uiFrame.Objects.empty())
			return;

		int firstBone = (int)_uiFrame.ObjectBones.size();
		if (bones != nullptr)
			_uiFrame.ObjectBones.insert(_uiFrame.ObjectBones.end(), bones, bones + boneCount);

		_uiFrame.ObjectMeshes.push_back(Backend::OverlayObjectMesh{ mesh, world, firstBone, (bones != nullptr) ? boneCount : 0 });
		_uiFrame.Objects.back().MeshCount++;
	}

	// Every present goes through here: the scene renderer composes its frame from the recorded 2D layer.
	void Renderer::PresentFrame()
	{
		UpdateFrameRate();

		if (Backend::g_SceneRenderer.IsActive())
		{
			auto screenResolution = GetScreenResolution();
			_uiFrame.ScreenWidth = screenResolution.x;
			_uiFrame.ScreenHeight = screenResolution.y;
			_uiFrame.Gamma = g_Configuration.Gamma;
			// Menu frames have no history for the temporal antialiasing: SMAA replaces it.
			_uiFrame.AntialiasingMode = (g_Configuration.AntialiasingMode == AntialiasingMode::Temporal) ? (int)AntialiasingMode::Smaa : (int)g_Configuration.AntialiasingMode;
			if (IsVideoSpriteActive())
			{
				auto videoSize = TEN::Video::g_VideoPlayer.GetFrameSize();
				_uiFrame.VideoFrame = (const unsigned char*)TEN::Video::g_VideoPlayer.GetFrameData();
				_uiFrame.VideoWidth = videoSize.x;
				_uiFrame.VideoHeight = videoSize.y;
			}

			Backend::g_SceneRenderer.FinishFrame(_uiFrame);
		}

		// Vectors keep their capacity across frames.
		_uiFrame.BackgroundFade = 1.0f;
		_uiFrame.Texts.clear();
		_uiFrame.Bars.clear();
		_uiFrame.Sprites.clear();
		_uiFrame.Images.clear();
		_uiFrame.Layers.clear();
		_uiFrame.Objects.clear();
		_uiFrame.ObjectMeshes.clear();
		_uiFrame.ObjectBones.clear();
		_uiFrame.VideoFrame = nullptr;
		_uiFrame.IsMenu = false;
		_uiFrame.DebugDrawData = nullptr;
		_uiFrame.FlashColor = Vector3::Zero;
		_uiFrame.OpticSprite = nullptr;
		_uiFrame.IsLaserSight = false;

		PaceFrame();
	}

	// Collects the buckets of an item mesh that are sorted, either by blend mode or because of the item alpha.
	void Renderer::CollectSortedMeshFaces(RendererItem* itemToDraw, RendererMesh* mesh, RendererObjectType type, int boneIndex, bool skinned, RenderView& view)
	{
		auto world = itemToDraw->InterpolatedAnimationTransforms[boneIndex] * itemToDraw->InterpolatedWorld;
		const auto& cameraPosition = view.Camera.WorldPosition;

		for (auto& bucket : mesh->Buckets)
		{
			if (bucket.NumVertices == 0)
				continue;

			auto blendMode = GetBlendModeFromAlpha(bucket.BlendMode, itemToDraw->Color.w);
			if (!IsSortedBlendMode(blendMode))
				continue;

			auto object = RendererSortableObject{};
			object.ObjectType = type;
			object.Skinned = skinned;
			object.BlendMode = blendMode;
			object.Bucket = &bucket;
			object.Item = itemToDraw;
			object.LightMode = mesh->LightMode;

			CollectSortedBucket(view, object, &world, cameraPosition);
		}
	}

	// Blends the rendered camera between the previous and current game camera for high framerate.
	void Renderer::InterpolateCamera(float interpFactor)
	{
		_interpolationFactor = interpFactor;

		// Interpolate camera.
		if (!Camera.DisableInterpolation)
		{
			_gameCamera.Camera.WorldPosition = Vector3::Lerp(_oldGameCamera.Camera.WorldPosition, _currentGameCamera.Camera.WorldPosition, interpFactor);
			_gameCamera.Camera.WorldDirection = Vector3::Lerp(_oldGameCamera.Camera.WorldDirection, _currentGameCamera.Camera.WorldDirection, interpFactor);
			_gameCamera.Camera.View = Matrix::Lerp(_oldGameCamera.Camera.View, _currentGameCamera.Camera.View, interpFactor);
			_gameCamera.Camera.Projection = Matrix::Lerp(_oldGameCamera.Camera.Projection, _currentGameCamera.Camera.Projection, interpFactor);
			_gameCamera.Camera.ViewProjection = _gameCamera.Camera.View * _gameCamera.Camera.Projection;
			_gameCamera.Camera.FOV = Lerp(_oldGameCamera.Camera.FOV, _currentGameCamera.Camera.FOV, interpFactor);
			_gameCamera.Camera.Frustum.Update(_gameCamera.Camera.View, _gameCamera.Camera.Projection);
		}
		else
		{
			_gameCamera.Camera.WorldPosition = _currentGameCamera.Camera.WorldPosition;
			_gameCamera.Camera.WorldDirection = _currentGameCamera.Camera.WorldDirection;
			_gameCamera.Camera.View = _currentGameCamera.Camera.View;
			_gameCamera.Camera.Projection = _currentGameCamera.Camera.Projection;
			_gameCamera.Camera.ViewProjection = _gameCamera.Camera.View * _gameCamera.Camera.Projection;
			_gameCamera.Camera.FOV = _currentGameCamera.Camera.FOV;
			_gameCamera.Camera.Frustum = _currentGameCamera.Camera.Frustum;
		}

		_gameCamera.Camera.ViewSize = _currentGameCamera.Camera.ViewSize;
		_gameCamera.Camera.InvViewSize = _currentGameCamera.Camera.InvViewSize;
		_gameCamera.Camera.NearPlane = _currentGameCamera.Camera.NearPlane;
		_gameCamera.Camera.FarPlane = _currentGameCamera.Camera.FarPlane;
	}

}
