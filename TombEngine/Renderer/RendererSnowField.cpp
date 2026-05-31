// ============================================================================
// RendererSnowField.cpp - Deformable snow overlay GPU integration (Phase 5).
//
// Owns the R8 heightmap texture, the snow constant buffer, and the dedicated
// draw pass that issues all snow overlay buckets through SnowOverlay.hlsl.
// The CPU-side heightmap lives in TEN::Effects::SnowField and is uploaded
// each frame the system is active.
// ============================================================================

#include "framework.h"
#include "Renderer/Renderer.h"

#include "Game/effects/SnowField.h"
#include "Game/items.h"
#include "Renderer/ConstantBuffers/SnowBuffer.h"
#include "Renderer/RendererEnums.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Scripting/Internal/TEN/Flow/FlowHandler.h"
#include "Scripting/Internal/TEN/Flow/Settings/Settings.h"
#include "Specific/configuration.h"
#include "Specific/level.h"

using namespace TEN::Renderer::ConstantBuffers;
namespace SnowField = TEN::Effects::SnowField;

namespace TEN::Renderer
{
	void Renderer::InitializeSnowField()
	{
		_cbSnow = CreateConstantBuffer<CSnowBuffer>();

		// Dynamic R8 texture matching SnowField::RESOLUTION. Updated every frame via
		// DX11Texture2D's USAGE_DYNAMIC map/discard path.
		_snowFieldHeightmap = _graphicsDevice->CreateTexture2D(
			SnowField::RESOLUTION, SnowField::RESOLUTION,
			SurfaceFormat::SF_R8_Unorm, nullptr, true);
	}

	void Renderer::DeinitializeSnowField()
	{
		_snowFieldHeightmap.reset();
		_cbSnow.reset();
	}

	void Renderer::UploadSnowFieldHeightmap()
	{
		if (_snowFieldHeightmap == nullptr || !SnowField::IsActive())
			return;

		const auto& src = SnowField::GetHeightmap();
		if (src.empty())
			return;

		// Copy into the std::vector<char> the device API expects.
		auto data = std::vector<char>(src.size());
		memcpy(data.data(), src.data(), src.size());

		_graphicsDevice->UpdateTexture2D(_snowFieldHeightmap.get(), data);
	}

	void Renderer::UpdateSnowBuffer()
	{
		const auto& snow = g_GameFlow->GetSettings()->Snow;
		const auto* level = g_GameFlow->GetLevel(CurrentLevel);

		_stSnow.SnowCentre = SnowField::GetWorldCentre();
		_stSnow.SnowWorldRadius = SnowField::GetWorldRadius();

		// Use per-level depth if set; fall back to global settings.
		int perLevelMaxDepth = level->GetSnowMaxDepth();
		_stSnow.SnowMaxDepth = (float)((perLevelMaxDepth > 0) ? perLevelMaxDepth : snow.MaxDepth);

		float tintR = (float)snow.Tint.GetR() / 255.0f;
		float tintG = (float)snow.Tint.GetG() / 255.0f;
		float tintB = (float)snow.Tint.GetB() / 255.0f;
		_stSnow.SnowTintAndRim = Vector4(tintR, tintG, tintB, snow.RimStrength);

		// HillHeight is independent of MaxDepth: the snow mesh is lifted by
		// (MaxDepth + HillHeight) at generation time so hills always have room.
		float hillHeight = std::max(0.0f, snow.HillHeight);

		// Combine the scripted/saved level offset with the transient debug UI offset.
		float levelOffset = level->GetSnowSurfaceOffset();
		_stSnow.SnowHillParams = Vector4(hillHeight, snow.HillFrequency, levelOffset + _snowDebugYOffset, 0.0f);

		UpdateConstantBuffer(&_stSnow, _cbSnow.get());
	}

	void Renderer::DrawSnowOverlay(RenderView& view)
	{
		if (_snowFieldHeightmap == nullptr || _cbSnow == nullptr)
			return;

		const auto& snow = g_GameFlow->GetSettings()->Snow;
		if (!snow.Enabled || !SnowField::IsActive())
			return;

		// Bail out before touching any GPU state if there is no level geometry loaded
		// (e.g. title screen with no rooms) or no snow buckets in this view. This avoids
		// binding null vertex buffers and keeps the pass a true no-op when irrelevant.
		if (_roomsVertexBuffer == nullptr || _roomsIndexBuffer == nullptr || view.RoomsToDraw.empty())
			return;

		bool hasSnowBucket = false;
		for (const auto* room : view.RoomsToDraw)
		{
			for (const auto& bucket : room->Buckets)
			{
				if (bucket.IsSnowOverlay && bucket.NumVertices > 0)
				{
					hasSnowBucket = true;
					break;
				}
			}
			if (hasSnowBucket)
				break;
		}

		if (!hasSnowBucket)
			return;

		// Per-frame uploads.
		UploadSnowFieldHeightmap();
		UpdateSnowBuffer();

		// Bind snow shader and standard rooms VB/IB (snow overlay buckets live in the
		// same room vertex/index buffers as regular floor geometry, see Phase 2).
		_shaders.Bind(Shader::SnowOverlay);

		_graphicsDevice->BindVertexBuffer(_roomsVertexBuffer.get());
		_graphicsDevice->BindIndexBuffer(_roomsIndexBuffer.get());
		_graphicsDevice->SetInputLayout(_vertexInputLayout.get());
		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);

		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::CounterClockwise);
		SetDepthState(DepthState::Write);

		// CBSnow at b6, needed by both stages (VS for vertex displacement, PS for tint).
		auto* cb = _cbSnow.get();
		BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::Snow, cb);
		BindConstantBuffer(ShaderStage::PixelShader,  ConstantBufferRegister::Snow, cb);

		// Heightmap (t16) must be readable by VS for displacement and by PS for the
		// rim highlight derivative.
		_graphicsDevice->BindTextureToStage(ShaderStage::VertexShader, TextureRegister::SnowFieldHeightmap,
			_snowFieldHeightmap.get(), SamplerStateRegister::LinearClamp);
		_graphicsDevice->BindTextureToStage(ShaderStage::PixelShader, TextureRegister::SnowFieldHeightmap,
			_snowFieldHeightmap.get(), SamplerStateRegister::LinearClamp);

		for (int i = (int)view.RoomsToDraw.size() - 1; i >= 0; i--)
		{
			const auto& room = *view.RoomsToDraw[i];

			bool hasBucket = false;
			for (const auto& bucket : room.Buckets)
			{
				if (bucket.IsSnowOverlay && bucket.NumVertices > 0)
				{
					hasBucket = true;
					break;
				}
			}
			if (!hasBucket)
				continue;

			// Per-room lighting state (mirrors DrawRooms): ambient color, room flags,
			// caustics, dynamic lights and decals. Required so the snow PS can reproduce
			// Rooms.hlsl's full lighting model (ambient, point/spot, shadows, fog bulbs).
			const auto& nativeRoom = g_Level.Rooms[room.RoomNumber];

			_stRoom.AmbientColor = Vector3(room.AmbientLight.x, room.AmbientLight.y, room.AmbientLight.z);
			_stRoom.Caustics = int(g_Configuration.EnableCaustics && (nativeRoom.flags & ENV_FLAG_WATER) && !(nativeRoom.flags & ENV_FLAG_NOCAUSTICS));
			_stRoom.Water = (nativeRoom.flags & ENV_FLAG_WATER) != 0 ? 1 : 0;
			_stRoom.Outdoor = (nativeRoom.flags & ENV_FLAG_SKYBOX) != 0 ? 1 : 0;

			BindRoomLights(view.LightsToDraw);
			BindRoomDecals(room.Decals);

			UpdateConstantBuffer(&_stRoom, _cbRoom.get());

			for (const auto& bucket : room.Buckets)
			{
				if (!bucket.IsSnowOverlay || bucket.NumVertices == 0)
					continue;

				// Bind the underlying floor texture as t0 via the standard rooms atlas path.
				BindBucketTextures(bucket, TextureSource::Rooms, false);

				DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);
				_numRoomsDrawCalls++;
			}
		}

		// Cleanup SRVs to avoid hazard warnings on subsequent passes.
		_graphicsDevice->UnbindTexture(ShaderStage::VertexShader, TextureRegister::SnowFieldHeightmap);
		_graphicsDevice->UnbindTexture(ShaderStage::PixelShader,  TextureRegister::SnowFieldHeightmap);
	}

	// Dedicated draw pass for snow-overlay buckets that live on moveable (item) meshes.
	// Mirrors DrawSnowOverlay but binds the moveables VB/IB and the SnowOverlayObjects
	// shader, which reads per-instance world from Objects[0] and applies the lift +
	// heightmap deformation in world space. Bones are not used; for multi-mesh items
	// the per-mesh bone transform is composed into Objects[0].World on the CPU.
	void Renderer::DrawSnowOverlayItems(RenderView& view)
	{
		if (_snowFieldHeightmap == nullptr || _cbSnow == nullptr)
			return;

		const auto& snow = g_GameFlow->GetSettings()->Snow;
		if (!snow.Enabled || !SnowField::IsActive())
			return;

		if (_moveablesVertexBuffer == nullptr || _moveablesIndexBuffer == nullptr || view.RoomsToDraw.empty())
			return;

		// Quick scan: do any visible items carry snow-overlay buckets at all?
		bool hasSnowBucket = false;
		for (const auto* room : view.RoomsToDraw)
		{
			for (const auto* item : room->ItemsToDraw)
			{
				if (item->ObjectID < 0 || item->ObjectID >= (int)_moveableObjects.size())
					continue;
				if (!_moveableObjects[item->ObjectID].has_value())
					continue;

				for (int k = 0; k < (int)item->MeshIndex.size(); k++)
				{
					auto* mesh = GetMesh(item->MeshIndex[k]);
					for (const auto& bucket : mesh->Buckets)
					{
						if (bucket.IsSnowOverlay && bucket.NumVertices > 0)
						{
							hasSnowBucket = true;
							break;
						}
					}
					if (hasSnowBucket) break;
				}
				if (hasSnowBucket) break;
			}
			if (hasSnowBucket) break;
		}

		if (!hasSnowBucket)
			return;

		// Ensure heightmap and CBSnow are current even if DrawSnowOverlay (rooms) was a
		// no-op this frame (e.g. no room snow buckets in this scene).
		UploadSnowFieldHeightmap();
		UpdateSnowBuffer();

		_shaders.Bind(Shader::SnowOverlayObjects);

		_graphicsDevice->BindVertexBuffer(_moveablesVertexBuffer.get());
		_graphicsDevice->BindIndexBuffer(_moveablesIndexBuffer.get());
		_graphicsDevice->SetInputLayout(_vertexInputLayout.get());
		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);

		SetBlendMode(BlendMode::Opaque);
		// Snow overlay on items must be visible from all camera angles (camera orbits
		// freely around the item, so front/back of each face changes continuously).
		SetCullMode(CullMode::None);
		SetDepthState(DepthState::Write);

		auto* cb = _cbSnow.get();
		BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::Snow, cb);
		BindConstantBuffer(ShaderStage::PixelShader,  ConstantBufferRegister::Snow, cb);

		_graphicsDevice->BindTextureToStage(ShaderStage::VertexShader, TextureRegister::SnowFieldHeightmap,
			_snowFieldHeightmap.get(), SamplerStateRegister::LinearClamp);
		_graphicsDevice->BindTextureToStage(ShaderStage::PixelShader, TextureRegister::SnowFieldHeightmap,
			_snowFieldHeightmap.get(), SamplerStateRegister::LinearClamp);

		// Shader ignores Skinned; we still clear it so any future shader read is benign.
		_stObjects.Skinned = 0;

		for (const auto* room : view.RoomsToDraw)
		{
			for (auto* item : room->ItemsToDraw)
			{
				if (item->ObjectID < 0 || item->ObjectID >= (int)_moveableObjects.size())
					continue;
				if (!_moveableObjects[item->ObjectID].has_value())
					continue;

				if (_currentMirror != nullptr && (g_Level.Items[item->ItemNumber].Flags & IFLAG_CLEAR_BODY))
					continue;

				auto& moveableObj = *_moveableObjects[item->ObjectID];
				auto* nativeItem  = &g_Level.Items[item->ItemNumber];

				bool itemLightsBound = false;
				bool acceptsShadows  = moveableObj.ShadowType == ShadowMode::None;

				for (int k = 0; k < (int)item->MeshIndex.size(); k++)
				{
					if (!nativeItem->MeshBits.Test(k))
						continue;

					auto* mesh = GetMesh(item->MeshIndex[k]);

					// Skip meshes that don't carry any snow-overlay bucket.
					bool meshHasSnow = false;
					for (const auto& bucket : mesh->Buckets)
					{
						if (bucket.IsSnowOverlay && bucket.NumVertices > 0)
						{
							meshHasSnow = true;
							break;
						}
					}
					if (!meshHasSnow)
						continue;

					// Compose per-mesh world: itemWorld with the mesh's bone transform pre-applied.
					Matrix meshWorld = item->InterpolatedAnimTransforms[k] * item->InterpolatedWorld;
					ReflectMatrixOptionally(meshWorld);

					_stObjects.Objects[0].World        = meshWorld;
					_stObjects.Objects[0].Color        = item->Color;
					_stObjects.Objects[0].AmbientLight = item->AmbientLight;
					_stObjects.Objects[0].LightMode    = (int)mesh->LightMode;

					if (!itemLightsBound)
					{
						BindMoveableLights(item->LightsToDraw, item->RoomNumber, item->PrevRoomNumber, item->LightFade, acceptsShadows);
						itemLightsBound = true;
					}

					UpdateConstantBuffer(&_stObjects, _cbObjects.get());

					for (const auto& bucket : mesh->Buckets)
					{
						if (!bucket.IsSnowOverlay || bucket.NumVertices == 0)
							continue;

						BindBucketTextures(bucket, TextureSource::Moveables, bucket.Animated);
						DrawIndexedTriangles(bucket.NumIndices, bucket.StartIndex, 0);
						_numMoveablesDrawCalls++;
					}
				}
			}
		}

		_graphicsDevice->UnbindTexture(ShaderStage::VertexShader, TextureRegister::SnowFieldHeightmap);
		_graphicsDevice->UnbindTexture(ShaderStage::PixelShader,  TextureRegister::SnowFieldHeightmap);
	}

	// Dedicated draw pass for snow-overlay buckets on instanced static meshes. Mirrors
	// the instanced path of DrawStatics but binds SnowOverlayObjects so trodden snow
	// and the debug Y offset apply uniformly to statics as well as to room geometry.
	void Renderer::DrawSnowOverlayStatics(RenderView& view)
	{
		if (_snowFieldHeightmap == nullptr || _cbSnow == nullptr)
			return;

		const auto& snow = g_GameFlow->GetSettings()->Snow;
		if (!snow.Enabled || !SnowField::IsActive())
			return;

		if (_staticsVertexBuffer == nullptr || _staticsIndexBuffer == nullptr || view.SortedStaticsToDraw.empty())
			return;

		// Quick scan across all visible static groups.
		bool hasSnowBucket = false;
		for (const auto& kv : view.SortedStaticsToDraw)
		{
			const auto& statics = kv.second;
			if (statics.empty()) continue;
			auto& refStaticObj = GetStaticRendererObject(statics[0]->ObjectNumber);
			if (refStaticObj.ObjectMeshes.empty()) continue;
			auto* refMesh = refStaticObj.ObjectMeshes[0];
			for (const auto& bucket : refMesh->Buckets)
			{
				if (bucket.IsSnowOverlay && bucket.NumVertices > 0)
				{
					hasSnowBucket = true;
					break;
				}
			}
			if (hasSnowBucket) break;
		}

		if (!hasSnowBucket)
			return;

		// Same defensive upload as in DrawSnowOverlayItems: keeps statics self-contained.
		UploadSnowFieldHeightmap();
		UpdateSnowBuffer();

		_shaders.Bind(Shader::SnowOverlayObjects);

		_graphicsDevice->BindVertexBuffer(_staticsVertexBuffer.get());
		_graphicsDevice->BindIndexBuffer(_staticsIndexBuffer.get());
		_graphicsDevice->SetInputLayout(_vertexInputLayout.get());
		_graphicsDevice->SetPrimitiveType(PrimitiveType::TriangleList);

		SetBlendMode(BlendMode::Opaque);
		SetCullMode(CullMode::None);
		SetDepthState(DepthState::Write);

		auto* cb = _cbSnow.get();
		BindConstantBuffer(ShaderStage::VertexShader, ConstantBufferRegister::Snow, cb);
		BindConstantBuffer(ShaderStage::PixelShader,  ConstantBufferRegister::Snow, cb);

		_graphicsDevice->BindTextureToStage(ShaderStage::VertexShader, TextureRegister::SnowFieldHeightmap,
			_snowFieldHeightmap.get(), SamplerStateRegister::LinearClamp);
		_graphicsDevice->BindTextureToStage(ShaderStage::PixelShader, TextureRegister::SnowFieldHeightmap,
			_snowFieldHeightmap.get(), SamplerStateRegister::LinearClamp);

		_stObjects.Skinned = 0;

		for (auto it = view.SortedStaticsToDraw.begin(); it != view.SortedStaticsToDraw.end(); it++)
		{
			auto& statics = it->second;
			if (statics.empty()) continue;

			auto& refStaticObj = GetStaticRendererObject(statics[0]->ObjectNumber);
			if (refStaticObj.ObjectMeshes.empty()) continue;
			auto* refMesh = refStaticObj.ObjectMeshes[0];

			// Skip whole group if the reference mesh has no snow bucket.
			bool groupHasSnow = false;
			for (const auto& bucket : refMesh->Buckets)
			{
				if (bucket.IsSnowOverlay && bucket.NumVertices > 0)
				{
					groupHasSnow = true;
					break;
				}
			}
			if (!groupHasSnow)
				continue;

			int staticsCount    = (int)statics.size();
			int bucketSize      = INSTANCED_STATIC_MESH_BUCKET_SIZE;
			int baseStaticIndex = 0;

			while (baseStaticIndex < staticsCount)
			{
				int instancesCount = 0;
				int maxIdx = std::min(baseStaticIndex + bucketSize, staticsCount);

				for (int s = baseStaticIndex; s < maxIdx; s++)
				{
					auto* current = statics[s];
					auto* sroom   = &_rooms[current->RoomNumber];

					if (IgnoreReflectionPassForRoom(current->RoomNumber))
						continue;

					if (current->Color.w < ALPHA_BLEND_THRESHOLD)
						continue;

					auto world = current->World;
					ReflectMatrixOptionally(world);

					_stObjects.Objects[instancesCount].World        = world;
					_stObjects.Objects[instancesCount].Color        = current->Color;
					_stObjects.Objects[instancesCount].AmbientLight = sroom->AmbientLight;
					_stObjects.Objects[instancesCount].LightMode    = (int)refMesh->LightMode;

					BindInstancedStaticLights(current->LightsToDraw, instancesCount);

					instancesCount++;
				}

				baseStaticIndex += bucketSize;

				if (instancesCount > 0)
				{
					UpdateConstantBuffer(&_stObjects, _cbObjects.get());

					for (const auto& bucket : refMesh->Buckets)
					{
						if (!bucket.IsSnowOverlay || bucket.NumVertices == 0)
							continue;

						BindBucketTextures(bucket, TextureSource::Statics, bucket.Animated);
						BindMaterial(bucket.MaterialIndex, false);

						DrawIndexedInstancedTriangles(bucket.NumIndices, instancesCount, bucket.StartIndex, 0);
						_numInstancedStaticsDrawCalls++;
					}
				}
			}
		}

		_graphicsDevice->UnbindTexture(ShaderStage::VertexShader, TextureRegister::SnowFieldHeightmap);
		_graphicsDevice->UnbindTexture(ShaderStage::PixelShader,  TextureRegister::SnowFieldHeightmap);
	}
}
