#include "framework.h"
#include "Renderer/Renderer.h"

#include <algorithm>
#include <unordered_map>

#include "Game/effects/hair.h"
#include "Objects/game_object_ids.h"
#include "Renderer/RenderView.h"
#include "Renderer/Structures/RendererSortableObject.h"
#include "Specific/configuration.h"
#include "Specific/level.h"

using namespace TEN::Effects::Hair;
using namespace TEN::Renderer::Structures;

namespace TEN::Renderer
{
	// Depth step below which two object groups count as equally distant and fall back
	// to the blend mode priority for a stable order.
	constexpr float GROUP_DEPTH_STEP = 128.0f;

	// Same-texture sprites group together only within this depth band, so a local
	// cloud batches as one unit without dragging distant sprites out of depth order.
	constexpr float SPRITE_GROUP_DEPTH_BAND = BLOCK(4);

	// Identity of a draw group collected by SortTransparentFaces: polygons with the same
	// key batch together in the sorted pass.
	struct GroupKey
	{
		const void*			Primary	  = nullptr;
		const void*			Secondary = nullptr;
		unsigned long long	Extra	  = 0;

		bool operator ==(const GroupKey& key) const
		{
			return (Primary == key.Primary && Secondary == key.Secondary && Extra == key.Extra);
		}
	};

	struct GroupKeyHash
	{
		size_t operator ()(const GroupKey& key) const
		{
			unsigned long long hash = (unsigned long long)(uintptr_t)key.Primary;
			hash ^= (unsigned long long)(uintptr_t)key.Secondary * 0x9E3779B97F4A7C15;
			hash ^= key.Extra * 0xC2B2AE3D27D4EB4F;
			hash ^= hash >> 29;
			return (size_t)hash;
		}
	};

	// Per-group data accumulated by SortTransparentFaces to establish the draw order.
	struct SortedGroup
	{
		bool  IsRoom		= false;
		int	  RoomSlot		= 0;
		float DistanceSum	= 0.0f;
		int	  PolygonCount	= 0;
		int	  BlendPriority = 0;
	};

	void Renderer::SortTransparentFaces(RenderView& view)
	{
		if (view.TransparentObjectsToDraw.empty())
			return;

		// Fixed priority between blend modes of groups at the same depth (issue #1793),
		// so their relative order is stable and predictable instead of arbitrary.
		auto getBlendPriority = [](BlendMode blendMode)
		{
			switch (blendMode)
			{
			case BlendMode::Exclude:	 return 0;
			case BlendMode::Subtractive: return 1;
			case BlendMode::Lighten:	 return 2;
			case BlendMode::Screen:		 return 3;
			case BlendMode::AlphaBlend:	 return 4;
			default:					 return 5;
			}
		};

		// Position of every room in the back-to-front draw order (portal traversal reversed).
		auto roomSlots = std::unordered_map<const RendererRoom*, int>{};
		roomSlots.reserve(view.RoomsToDraw.size());
		for (int i = 0; i < view.RoomsToDraw.size(); i++)
			roomSlots[view.RoomsToDraw[i]] = (int)view.RoomsToDraw.size() - 1 - i;

		auto groupIndices = std::unordered_map<GroupKey, int, GroupKeyHash>{};
		auto groups = std::vector<SortedGroup>{};

		for (auto& object : view.TransparentObjectsToDraw)
		{
			auto key = GroupKey{};
			auto blendMode = object.BlendMode;

			switch (object.ObjectType)
			{
			case RendererObjectType::Room:
				key.Primary = object.Bucket;
				break;

			case RendererObjectType::Moveable:
			case RendererObjectType::HairPrimary:
			case RendererObjectType::HairSecondary:
				key.Primary = object.Item;
				key.Secondary = object.Bucket;
				key.Extra = (unsigned long long)object.ObjectType;
				break;

			case RendererObjectType::Static:
				key.Primary = object.Static;
				key.Secondary = object.Bucket;
				break;

			case RendererObjectType::Effect:
				key.Primary = object.Effect;
				key.Secondary = object.Bucket;
				break;

			case RendererObjectType::MoveableAsStatic:
				// No stable instance pointer exists for swarm objects: the world translation
				// tells instances of the same mesh apart (e.g. two bats sharing buckets).
				key.Primary = object.Bucket;
				key.Extra =
					((unsigned long long)(std::lround(object.World._41) & 0x1FFFFF)) |
					((unsigned long long)(std::lround(object.World._42) & 0x1FFFFF) << 21) |
					((unsigned long long)(std::lround(object.World._43) & 0x1FFFFF) << 42);
				break;

			case RendererObjectType::Sprite:
				blendMode = object.Sprite->BlendMode;
				key.Primary = object.Sprite->Sprite;
				key.Extra =
					(unsigned long long)(unsigned int)(int)(object.Distance / SPRITE_GROUP_DEPTH_BAND) |
					((unsigned long long)blendMode << 32) |
					((unsigned long long)object.Sprite->Type << 40) |
					((unsigned long long)object.Sprite->renderType << 48) |
					((unsigned long long)(object.Sprite->SoftParticle ? 1 : 0) << 56);
				break;

			default:
				key.Primary = object.Polygon;
				key.Extra = (unsigned long long)object.ObjectType;
				break;
			}

			auto [it, isNewGroup] = groupIndices.try_emplace(key, (int)groups.size());
			if (isNewGroup)
			{
				auto& group = groups.emplace_back();
				group.IsRoom = (object.ObjectType == RendererObjectType::Room);
				group.BlendPriority = getBlendPriority(blendMode);

				if (group.IsRoom)
				{
					auto slot = roomSlots.find(object.Room);
					group.RoomSlot = (slot != roomSlots.end()) ? slot->second : (int)view.RoomsToDraw.size();
				}
			}

			auto& group = groups[it->second];
			group.DistanceSum += object.Distance;
			group.PolygonCount++;

			// Temporarily the group index; remapped to the final rank below.
			object.GroupRank = it->second;
		}

		auto order = std::vector<int>(groups.size());
		for (int i = 0; i < order.size(); i++)
			order[i] = i;

		std::sort(
			order.begin(), order.end(),
			[&groups](int index0, int index1)
			{
				const auto& group0 = groups[index0];
				const auto& group1 = groups[index1];

				// Rooms draw first, back to front by portal traversal order.
				if (group0.IsRoom != group1.IsRoom)
					return group0.IsRoom;

				float distance0 = group0.DistanceSum / group0.PolygonCount;
				float distance1 = group1.DistanceSum / group1.PolygonCount;

				if (group0.IsRoom)
				{
					if (group0.RoomSlot != group1.RoomSlot)
						return (group0.RoomSlot < group1.RoomSlot);

					return (distance0 > distance1);
				}

				// Object groups draw back to front by mean polygon distance.
				int depth0 = (int)(distance0 / GROUP_DEPTH_STEP);
				int depth1 = (int)(distance1 / GROUP_DEPTH_STEP);
				if (depth0 != depth1)
					return (depth0 > depth1);

				if (group0.BlendPriority != group1.BlendPriority)
					return (group0.BlendPriority < group1.BlendPriority);

				return (index0 < index1);
			});

		auto groupRanks = std::vector<int>(groups.size());
		for (int i = 0; i < order.size(); i++)
			groupRanks[order[i]] = i;

		for (auto& object : view.TransparentObjectsToDraw)
			object.GroupRank = groupRanks[object.GroupRank];

		// Make groups contiguous in rank order, keeping polygons depth-sorted inside
		// their own group.
		std::sort(
			view.TransparentObjectsToDraw.begin(),
			view.TransparentObjectsToDraw.end(),
			[](const RendererSortableObject& object0, const RendererSortableObject& object1)
			{
				if (object0.GroupRank != object1.GroupRank)
					return (object0.GroupRank < object1.GroupRank);

				return (object0.Distance > object1.Distance);
			});
	}

	void Renderer::DrawSortedFaces(RenderView& view)
	{
		// Invalidate the room CB cache used by DrawRoomSorted.
		// Invalidate the CB caches used by the sorted draw functions.
		_lastSortedRoomNumber = NO_VALUE;
		_lastSortedObjectType = RendererObjectType::Unknown;
		_lastSortedObject = nullptr;

		_sortedPolygonsVertices.clear();
		_sortedPolygonsIndices.clear();
		_sortedPolygonsBatches.clear();

		// Type of the last batch actually drawn, used by the draw functions to skip redundant
		// shader and vertex buffer binds when consecutive batches share the object type.
		auto lastObjectType = RendererObjectType::Unknown;

		// Upload the accumulated geometry with a single map per buffer, then draw the recorded
		// batches. Uploading once per flush instead of once per batch keeps the number of
		// WRITE_DISCARD maps (and driver buffer renames) independent from the batch count.
		auto flushBatches = [&]()
		{
			if (_sortedPolygonsBatches.empty())
				return;

			if (!_sortedPolygonsIndices.empty())
			{
				_graphicsDevice->UpdateIndexBuffer(_sortedPolygonsIndexBuffer.get(), (int)_sortedPolygonsIndices.size(), 0, _sortedPolygonsIndices.data());
				_graphicsDevice->BindIndexBuffer(_sortedPolygonsIndexBuffer.get());
			}

			if (!_sortedPolygonsVertices.empty())
				_graphicsDevice->UpdateVertexBuffer(_sortedPolygonsVertexBuffer.get(), 0, (int)_sortedPolygonsVertices.size(), _sortedPolygonsVertices.data());

			for (const auto& batch : _sortedPolygonsBatches)
			{
				switch (batch.Object->ObjectType)
				{
				case RendererObjectType::Room:
					DrawRoomSorted(batch.Object, lastObjectType, view, batch.Base, batch.Count);
					break;

				case RendererObjectType::Moveable:
					DrawItemSorted(batch.Object, lastObjectType, view, batch.Base, batch.Count);
					break;

				case RendererObjectType::HairPrimary:
				case RendererObjectType::HairSecondary:
					DrawHairSorted(batch.Object, lastObjectType, view, batch.Object->ObjectType == RendererObjectType::HairPrimary ? 0 : 1, batch.Base, batch.Count);
					break;

				case RendererObjectType::Static:
					DrawStaticSorted(batch.Object, lastObjectType, view, batch.Base, batch.Count);
					break;

				case RendererObjectType::MoveableAsStatic:
					DrawMoveableAsStaticSorted(batch.Object, lastObjectType, view, batch.Base, batch.Count);
					break;

				case RendererObjectType::Effect:
					DrawEffectSorted(batch.Object, lastObjectType, view, batch.Base, batch.Count);
					break;

				case RendererObjectType::Sprite:
					DrawSpriteSorted(batch.Object, lastObjectType, view, batch.Base, batch.Count);
					break;

				default:
					continue;
				}

				lastObjectType = batch.Object->ObjectType;
			}

			_sortedPolygonsBatches.clear();
			_sortedPolygonsIndices.clear();
			_sortedPolygonsVertices.clear();
		};

		for (int i = 0; i < view.TransparentObjectsToDraw.size(); i++)
		{
			auto* object = &view.TransparentObjectsToDraw[i];

			if (_currentMirror != nullptr && object->ObjectType == RendererObjectType::Room)
				continue;

			int startEntry = i;
			int base = (object->ObjectType == RendererObjectType::Sprite ?
				(int)_sortedPolygonsVertices.size() : (int)_sortedPolygonsIndices.size());
			int count = 0;

			if (object->ObjectType != RendererObjectType::Sprite)
			{
				// All entries of a group share the object type, so the index source only
				// depends on the group head. Moveables, hair, swarm objects and effects
				// all reference moveable geometry.
				int* sourceIndices = nullptr;
				switch (object->ObjectType)
				{
				case RendererObjectType::Room:
					sourceIndices = _roomsIndices.data();
					break;

				case RendererObjectType::Static:
					sourceIndices = _staticsIndices.data();
					break;

				case RendererObjectType::Moveable:
				case RendererObjectType::HairPrimary:
				case RendererObjectType::HairSecondary:
				case RendererObjectType::MoveableAsStatic:
				case RendererObjectType::Effect:
					sourceIndices = _moveablesIndices.data();
					break;

				default:
					continue;
				}

				while (i < view.TransparentObjectsToDraw.size() &&
					view.TransparentObjectsToDraw[i].GroupRank == object->GroupRank &&
					_sortedPolygonsIndices.size() + (view.TransparentObjectsToDraw[i].Polygon->Shape == 0 ? 6 : 3) < MAX_TRANSPARENT_VERTICES)
				{
					auto* currentObject = &view.TransparentObjectsToDraw[i];
					_sortedPolygonsIndices.bulk_push_back(
						sourceIndices,
						currentObject->Polygon->BaseIndex,
						currentObject->Polygon->Shape == 0 ? 6 : 3);
					i++;
				}

				count = (int)_sortedPolygonsIndices.size() - base;
			}
			else
			{
				while (i < view.TransparentObjectsToDraw.size() &&
					view.TransparentObjectsToDraw[i].GroupRank == object->GroupRank &&
					_sortedPolygonsVertices.size() + 6 < MAX_TRANSPARENT_VERTICES)
				{
					RendererSortableObject* currentObject = &view.TransparentObjectsToDraw[i];
					RendererSpriteToDraw* spr = currentObject->Sprite;

					Vector3 p0t;
					Vector3 p1t;
					Vector3 p2t;
					Vector3 p3t;

					Vector2 uv0;
					Vector2 uv1;
					Vector2 uv2;
					Vector2 uv3;

					if (spr->Type == SpriteType::ThreeD)
					{
						p0t = spr->vtx1;
						p1t = spr->vtx2;
						p2t = spr->vtx3;
						p3t = spr->vtx4;


					}
					else
					{
						p0t = Vector3(-0.5, 0.5, 0);
						p1t = Vector3(0.5, 0.5, 0);
						p2t = Vector3(0.5, -0.5, 0);
						p3t = Vector3(-0.5, -0.5, 0);
					}

					uv0 = spr->Sprite->UV[0];
					uv1 = spr->Sprite->UV[1];
					uv2 = spr->Sprite->UV[2];
					uv3 = spr->Sprite->UV[3];

					auto world = GetWorldMatrixForSprite(*currentObject->Sprite, view);

					Vertex v0;
					v0.Position = Vector3::Transform(p0t, world);
					v0.UV = uv0;
					v0.Color = VectorColorToRGBA(spr->c1);
					v0.Effects = 0 << INDEX_IN_POLY_VERTEX_SHIFT;

					Vertex v1;
					v1.Position = Vector3::Transform(p1t, world);
					v1.UV = uv1;
					v1.Color = VectorColorToRGBA(spr->c2);
					v1.Effects = 1 << INDEX_IN_POLY_VERTEX_SHIFT;

					Vertex v2;
					v2.Position = Vector3::Transform(p2t, world);
					v2.UV = uv2;
					v2.Color = VectorColorToRGBA(spr->c3);
					v2.Effects = 2 << INDEX_IN_POLY_VERTEX_SHIFT;

					Vertex v3;
					v3.Position = Vector3::Transform(p3t, world);
					v3.UV = uv3;
					v3.Color = VectorColorToRGBA(spr->c4);
					v3.Effects = 3 << INDEX_IN_POLY_VERTEX_SHIFT;

					_sortedPolygonsVertices.push_back(v0);
					_sortedPolygonsVertices.push_back(v1);
					_sortedPolygonsVertices.push_back(v3);
					_sortedPolygonsVertices.push_back(v2);
					_sortedPolygonsVertices.push_back(v3);
					_sortedPolygonsVertices.push_back(v1);

					i++;
				}

				count = (int)_sortedPolygonsVertices.size() - base;
			}

			if (count == 0)
			{
				// The shared buffer is full: draw what has been accumulated so far and retry
				// this entry against the emptied buffers.
				flushBatches();
				i = startEntry - 1;
				continue;
			}

			_sortedPolygonsBatches.push_back({ object, base, count });

			i--;
		}

		flushBatches();

	}

	void Renderer::DrawRoomSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view, int baseIndex, int count)
	{
		if (lastObjectType != objectInfo->ObjectType)
		{
			_graphicsDevice->BindVertexBuffer(_roomsVertexBuffer.get());
			_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
			_graphicsDevice->SetInputLayout(_vertexInputLayout.get());

			SetDepthState(DepthState::Read);
			SetCullMode(CullMode::CounterClockwise);

			_shaders.Bind(Shader::Rooms);
		}

		// Rebuild and upload the room CB only when the room changes. Consecutive sorted batches
		// from the same room would produce identical CB contents (lights come from the view, not
		// the room), and nothing else touches the room CB during the sorted faces pass.
		if (objectInfo->Room->RoomNumber != _lastSortedRoomNumber)
		{
			RoomData* nativeRoom = &g_Level.Rooms[objectInfo->Room->RoomNumber];

			_stRoom.Caustics =  int(g_Configuration.EnableCaustics && (nativeRoom->flags & ENV_FLAG_WATER) && !(nativeRoom->flags & ENV_FLAG_NOCAUSTICS));
			_stRoom.AmbientColor = Vector3(objectInfo->Room->AmbientLight.x, objectInfo->Room->AmbientLight.y, objectInfo->Room->AmbientLight.z);
			BindRoomLights(view.LightsToDraw);
			_stRoom.NumRoomDecals = 0; // Don't draw decals on sorted faces to avoid slowdowns.
			_stRoom.Water = (nativeRoom->flags & ENV_FLAG_WATER) != 0 ? 1 : 0;
			UpdateConstantBuffer(&_stRoom, _cbRoom.get());

			_lastSortedRoomNumber = objectInfo->Room->RoomNumber;
		}

		SetScissor(objectInfo->Room->ClipBounds);

		SetBlendMode(objectInfo->BlendMode);
		SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

		BindBucketTextures(*objectInfo->Bucket, TextureSource::Rooms, objectInfo->Bucket->Animated);
		BindMaterial(objectInfo->Bucket->MaterialIndex, false);

		DrawIndexedTriangles(count, baseIndex, 0);

		_numSortedRoomsDrawCalls++;
		_numSortedTriangles += count / 3;

		ResetScissor();
	}

	void Renderer::DrawItemSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view, int baseIndex, int count)
	{
		if (lastObjectType != objectInfo->ObjectType)
		{
			_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
			_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
			_graphicsDevice->SetInputLayout(_vertexInputLayout.get());

			SetDepthState(DepthState::Read);
			SetCullMode(CullMode::CounterClockwise);

			_shaders.Bind(Shader::Items);
		}

		// Rebuild and upload the objects CB only when the item changes: grouping splits the
		// same item into one batch per bucket which would re-upload identical data.
		if (_lastSortedObjectType != objectInfo->ObjectType || _lastSortedObject != objectInfo->Item)
		{
			// Bind main item properties.
			Matrix world = objectInfo->Item->InterpolatedWorld;
			_stObjects.Objects[0].World = world;
			_stObjects.Objects[0].Color = objectInfo->Item->Color;
			_stObjects.Objects[0].AmbientLight = objectInfo->Item->AmbientLight;
			_stObjects.Skinned = (int)(objectInfo->Skinned ? SkinningMode::Full : SkinningMode::None);

			const auto& moveableObj = *_moveableObjects[objectInfo->Item->ObjectID];

			if (objectInfo->Skinned)
			{
				for (int m = 0; m < moveableObj.BindPoseTransforms.size(); m++)
					_stObjects.Bones[m] = moveableObj.BindPoseTransforms[m] * objectInfo->Item->InterpolatedAnimationTransforms[m];
			}
			else
			{
				memcpy(_stObjects.Bones, objectInfo->Item->InterpolatedAnimationTransforms, sizeof(Matrix) * BONE_COUNT_MAX);
			}

			for (int k = 0; k < moveableObj.ObjectMeshes.size(); k++)
				_stObjects.BoneLightModes[k] = (int)moveableObj.ObjectMeshes[k]->LightMode;

			bool acceptsShadows = moveableObj.ShadowType == ShadowMode::None;
			BindMoveableLights(objectInfo->Item->LightsToDraw, objectInfo->Item->RoomNumber, objectInfo->Item->PrevRoomNumber, objectInfo->Item->LightFade, acceptsShadows);

			// Only Objects[0] is used, so upload just the CB prefix.
			UpdateConstantBuffer(&_stObjects, _cbObjects.get(), GetObjectsBufferPrefixSize(1));

			_lastSortedObjectType = objectInfo->ObjectType;
			_lastSortedObject = objectInfo->Item;
		}

		SetBlendMode(objectInfo->BlendMode);
		SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

		BindBucketTextures(*objectInfo->Bucket, TextureSource::Moveables, objectInfo->Bucket->Animated);
		BindMaterial(objectInfo->Bucket->MaterialIndex, false);

		DrawIndexedTriangles(count, baseIndex, 0);

		_numSortedMoveablesDrawCalls++;
		_numSortedTriangles += count / 3;
	}

	void Renderer::DrawStaticSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view, int baseIndex, int count)
	{
		if (lastObjectType != objectInfo->ObjectType)
		{
			_graphicsDevice->BindVertexBuffer(_staticsVertexBuffer.get());
			_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
			_graphicsDevice->SetInputLayout(_vertexInputLayout.get());

			SetDepthState(DepthState::Read);
			SetCullMode(CullMode::CounterClockwise);

			_shaders.Bind(Shader::InstancedStatics);
		}

		// Rebuild and upload the objects CB only when the static changes: grouping splits the
		// same static into one batch per bucket which would re-upload identical data.
		if (_lastSortedObjectType != objectInfo->ObjectType || _lastSortedObject != objectInfo->Static)
		{
			_stObjects.Skinned = (int)SkinningMode::Static;

			auto world = objectInfo->Static->World;
			_stObjects.Objects[0].World = world;

			_stObjects.Objects[0].Color = objectInfo->Static->Color;
			_stObjects.Objects[0].AmbientLight = objectInfo->Room->AmbientLight;
			_stObjects.Objects[0].LightMode = (int)GetStaticRendererObject(objectInfo->Static->ObjectNumber).ObjectMeshes[0]->LightMode;
			BindInstancedStaticLights(objectInfo->Static->LightsToDraw, 0);

			// Only Objects[0] is used, so upload just the CB prefix.
			UpdateConstantBuffer(&_stObjects, _cbObjects.get(), GetObjectsBufferPrefixSize(1));

			_lastSortedObjectType = objectInfo->ObjectType;
			_lastSortedObject = objectInfo->Static;
		}

		SetBlendMode(objectInfo->BlendMode);
		SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

		BindBucketTextures(*objectInfo->Bucket, TextureSource::Statics, objectInfo->Bucket->Animated);
		BindMaterial(objectInfo->Bucket->MaterialIndex, false);

		DrawIndexedInstancedTriangles(count, 1, baseIndex, 0);

		_numSortedStaticsDrawCalls++;
		_numSortedTriangles += count / 3;
	}

	void Renderer::DrawMoveableAsStaticSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view, int baseIndex, int count)
	{
		_stObjects.Skinned = (int)SkinningMode::Static;

		if (lastObjectType != objectInfo->ObjectType)
		{
			_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
			_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
			_graphicsDevice->SetInputLayout(_vertexInputLayout.get());

			SetDepthState(DepthState::Read);
			SetCullMode(CullMode::CounterClockwise);

			_shaders.Bind(Shader::InstancedStatics);
		}

		auto world = objectInfo->World;
		_stObjects.Objects[0].World = world;

		_stObjects.Objects[0].Color = NEUTRAL_COLOR;
		_stObjects.Objects[0].AmbientLight = objectInfo->Room->AmbientLight;
		_stObjects.Objects[0].LightMode = (int)objectInfo->LightMode;
		BindInstancedStaticLights(objectInfo->Room->LightsToDraw, 0);

		// Only Objects[0] is used, so upload just the CB prefix. No per-object key exists for
		// this type, so the upload always happens and the objects CB cache is invalidated.
		UpdateConstantBuffer(&_stObjects, _cbObjects.get(), GetObjectsBufferPrefixSize(1));
		_lastSortedObjectType = objectInfo->ObjectType;
		_lastSortedObject = nullptr;

		SetBlendMode(objectInfo->BlendMode);
		SetAlphaTest(AlphaTestMode::GreatherThan, ALPHA_TEST_THRESHOLD);

		BindBucketTextures(*objectInfo->Bucket, TextureSource::Statics, objectInfo->Bucket->Animated);
		BindMaterial(objectInfo->Bucket->MaterialIndex, false);

		DrawIndexedInstancedTriangles(count, 1, baseIndex, 0);

		_numSortedStaticsDrawCalls++;
		_numSortedTriangles += count / 3;
	}

	void Renderer::DrawEffectSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view, int baseIndex, int count)
	{
		if (lastObjectType != objectInfo->ObjectType)
		{
			_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
			_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
			_graphicsDevice->SetInputLayout(_vertexInputLayout.get());

			SetDepthState(DepthState::Read);
			SetCullMode(CullMode::CounterClockwise);

			_shaders.Bind(Shader::InstancedStatics);
		}

		// Rebuild and upload the objects CB only when the effect changes: grouping splits the
		// same effect into one batch per bucket which would re-upload identical data.
		if (_lastSortedObjectType != objectInfo->ObjectType || _lastSortedObject != objectInfo->Effect)
		{
			_stObjects.Skinned = (int)SkinningMode::Static;

			auto world = objectInfo->Effect->InterpolatedWorld;
			_stObjects.Objects[0].World = world;

			_stObjects.Objects[0].Color = objectInfo->Effect->Color;
			_stObjects.Objects[0].AmbientLight = objectInfo->Effect->AmbientLight;
			_stObjects.Objects[0].LightMode = (int)LightMode::Dynamic;
			BindInstancedStaticLights(objectInfo->Effect->LightsToDraw, 0);

			// Only Objects[0] is used, so upload just the CB prefix.
			UpdateConstantBuffer(&_stObjects, _cbObjects.get(), GetObjectsBufferPrefixSize(1));

			_lastSortedObjectType = objectInfo->ObjectType;
			_lastSortedObject = objectInfo->Effect;
		}

		SetBlendMode(objectInfo->BlendMode);
		SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

		BindBucketTextures(*objectInfo->Bucket, TextureSource::Moveables, objectInfo->Bucket->Animated);
		BindMaterial(objectInfo->Bucket->MaterialIndex, false);

		DrawIndexedInstancedTriangles(count, 1, baseIndex, 0);

		_numEffectsDrawCalls++;
		_numSortedTriangles += count / 3;
	}

	void Renderer::DrawHairSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view, int index, int baseIndex, int count)
	{
		if (index >= HairEffect.Units.size())
		{
			TENLog("Attempt to draw nonexistent hair unit", LogLevel::Warning);
			return;
		}

		if (lastObjectType != objectInfo->ObjectType)
		{
			_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
			_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
			_graphicsDevice->SetInputLayout(_vertexInputLayout.get());

			SetDepthState(DepthState::Read);
			SetCullMode(CullMode::CounterClockwise);

			_shaders.Bind(Shader::Items);
		}

		// Rebuild and upload the objects CB only when the hair unit changes: grouping splits
		// the same unit into one batch per bucket which would re-upload identical data.
		// The object type distinguishes the primary and secondary units of the same item.
		if (_lastSortedObjectType != objectInfo->ObjectType || _lastSortedObject != objectInfo->Item)
		{
			// Bind main item properties.
			Matrix world = objectInfo->Item->InterpolatedWorld;
			_stObjects.Objects[0].World = world;
			_stObjects.Objects[0].Color = objectInfo->Item->Color;
			_stObjects.Objects[0].AmbientLight = objectInfo->Item->AmbientLight;
			_stObjects.Skinned = (int)(objectInfo->Skinned ? SkinningMode::Full : SkinningMode::None);

			const auto& moveableObj = *_moveableObjects[(int)GAME_OBJECT_ID::ID_HAIR_PRIMARY + index];

			_stObjects.Objects[0].World = Matrix::Identity;
			_stObjects.Bones[0] = objectInfo->Item->InterpolatedAnimationTransforms[HairUnit::GetRootMeshID(index)] * objectInfo->Item->InterpolatedWorld;
			ReflectMatrixOptionally(_stObjects.Bones[0]);

			for (int i = 0; i < HairEffect.Units[index].Segments.size(); i++)
			{
				const auto& segment = HairEffect.Units[index].Segments[i];
				auto worldMatrix = segment.GlobalTransform;

				ReflectMatrixOptionally(worldMatrix);

				_stObjects.Bones[i + 1] = worldMatrix;
				_stObjects.BoneLightModes[i] = (int)LightMode::Dynamic;
			}

			for (int k = 0; k < moveableObj.ObjectMeshes.size(); k++)
				_stObjects.BoneLightModes[k] = (int)moveableObj.ObjectMeshes[k]->LightMode;

			bool acceptsShadows = moveableObj.ShadowType == ShadowMode::None;
			BindMoveableLights(objectInfo->Item->LightsToDraw, objectInfo->Item->RoomNumber, objectInfo->Item->PrevRoomNumber, objectInfo->Item->LightFade, acceptsShadows);

			// Only Objects[0] is used, so upload just the CB prefix.
			UpdateConstantBuffer(&_stObjects, _cbObjects.get(), GetObjectsBufferPrefixSize(1));

			_lastSortedObjectType = objectInfo->ObjectType;
			_lastSortedObject = objectInfo->Item;
		}

		SetBlendMode(objectInfo->BlendMode);
		SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

		BindBucketTextures(*objectInfo->Bucket, TextureSource::Moveables, objectInfo->Bucket->Animated);
		BindMaterial(objectInfo->Bucket->MaterialIndex, false);

		DrawIndexedTriangles(count, baseIndex, 0);

		_numSortedMoveablesDrawCalls++;
		_numSortedTriangles += count / 3;
	}

	void Renderer::DrawSingleSprite(RendererSortableObject* object, RendererObjectType lastObjectType, RenderView& view)
	{
		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleStrip);

		BindRenderTargetAsTexture(TextureRegister::GBufferDepthMap, _depthRenderTarget->GetRenderTarget(), SamplerStateRegister::LinearClamp);

		SetDepthState(DepthState::Read);
		SetCullMode(CullMode::None);
		SetBlendMode(object->Sprite->BlendMode);
		SetAlphaTest(AlphaTestMode::GreatherThan, ALPHA_TEST_THRESHOLD);

		_shaders.Bind(Shader::InstancedSprites);

		_stInstancedSpriteBuffer.Sprites[0].World = object->Sprite->Type != SpriteType::ThreeD ?
			GetWorldMatrixForSprite(*object->Sprite, view) :
			Matrix::Identity;
		_stInstancedSpriteBuffer.Sprites[0].PerVertexColor = 1;
		_stInstancedSpriteBuffer.Sprites[0].IsSoftParticle = object->Sprite->SoftParticle ? 1 : 0;
		_stInstancedSpriteBuffer.Sprites[0].RenderType = (int)object->Sprite->renderType;

		PackSpriteTextureCoordinates(0, object->Sprite->Sprite);

		UpdateConstantBuffer(&_stInstancedSpriteBuffer, _cbInstancedSpriteBuffer.get());;

		BindTexture(TextureRegister::ColorMap, object->Sprite->Sprite->Texture, SamplerStateRegister::LinearClamp);
		
		// Set up vertex buffer and parameters.
		if (object->Sprite->Type != SpriteType::ThreeD)
		{
			_graphicsDevice->BindVertexBuffer(_quadVertexBuffer.get());
		}
		else
		{
			auto vertex0 = Vertex{};
			vertex0.Position = object->Sprite->vtx1;
			vertex0.UV = object->Sprite->Sprite->UV[0];
			vertex0.Color = VectorColorToRGBA(object->Sprite->c1);
			vertex0.Effects = 0 << INDEX_IN_POLY_VERTEX_SHIFT;

			auto vertex1 = Vertex{};
			vertex1.Position = object->Sprite->vtx2;
			vertex1.UV = object->Sprite->Sprite->UV[1];
			vertex1.Color = VectorColorToRGBA(object->Sprite->c2);
			vertex1.Effects = 1 << INDEX_IN_POLY_VERTEX_SHIFT;

			auto vertex2 = Vertex{};
			vertex2.Position = object->Sprite->vtx3;
			vertex2.UV = object->Sprite->Sprite->UV[2];
			vertex2.Color = VectorColorToRGBA(object->Sprite->c3);
			vertex2.Effects = 2 << INDEX_IN_POLY_VERTEX_SHIFT;

			auto vertex3 = Vertex{};
			vertex3.Position = object->Sprite->vtx4;
			vertex3.UV = object->Sprite->Sprite->UV[3];
			vertex3.Color = VectorColorToRGBA(object->Sprite->c4);
			vertex3.Effects = 3 << INDEX_IN_POLY_VERTEX_SHIFT;

			_spriteVertices.clear();
			_spriteVertices.push_back(vertex0);
			_spriteVertices.push_back(vertex1);
			_spriteVertices.push_back(vertex3);
			_spriteVertices.push_back(vertex2);

			_graphicsDevice->UpdateVertexBuffer(_spriteVertexBuffer.get(), 0, 4, _spriteVertices.data());
			_graphicsDevice->BindVertexBuffer(_spriteVertexBuffer.get());
		}

		// Draw sprites with instancing.
		DrawInstancedTriangles(4, 1, 0);

		_numSortedSpritesDrawCalls++;
		_numSortedTriangles += 2;

		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
	}

	void Renderer::DrawSpriteSorted(RendererSortableObject* objectInfo, RendererObjectType lastObjectType, RenderView& view, int baseVertex, int count)
	{
		if (lastObjectType != objectInfo->ObjectType)
		{
			_shaders.Bind(Shader::InstancedSprites);

			_graphicsDevice->BindVertexBuffer(_sortedPolygonsVertexBuffer.get());
			_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);
			_graphicsDevice->SetInputLayout(_vertexInputLayout.get());

		}

		_stInstancedSpriteBuffer.Sprites[0].World = Matrix::Identity;
		_stInstancedSpriteBuffer.Sprites[0].PerVertexColor = 1;
		_stInstancedSpriteBuffer.Sprites[0].IsSoftParticle = objectInfo->Sprite->SoftParticle ? 1 : 0;
		_stInstancedSpriteBuffer.Sprites[0].RenderType = (int)objectInfo->Sprite->renderType;

		PackSpriteTextureCoordinates(0, objectInfo->Sprite->Sprite);

		// Only Sprites[0] is used, so upload just the CB prefix.
		UpdateConstantBuffer(&_stInstancedSpriteBuffer, _cbInstancedSpriteBuffer.get(), (int)sizeof(InstancedSprite));

		SetDepthState(DepthState::Read);
		SetCullMode(CullMode::None);
		SetBlendMode(objectInfo->Sprite->BlendMode);
		SetAlphaTest(AlphaTestMode::None, ALPHA_TEST_THRESHOLD);

		BindTexture(TextureRegister::ColorMap, objectInfo->Sprite->Sprite->Texture, SamplerStateRegister::LinearClamp);
		BindRenderTargetAsTexture(TextureRegister::GBufferDepthMap, _depthRenderTarget->GetRenderTarget(), SamplerStateRegister::PointWrap);

		DrawInstancedTriangles(count, 1, baseVertex);

		_numSortedSpritesDrawCalls++;
		_numSortedTriangles += count / 3;
	}
}
