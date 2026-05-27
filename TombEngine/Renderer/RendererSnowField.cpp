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

		_stSnow.SnowCentre = SnowField::GetWorldCentre();
		_stSnow.SnowWorldRadius = SnowField::GetWorldRadius();
		_stSnow.SnowMaxDepth = (float)snow.MaxDepth;

		float tintR = (float)snow.Tint.GetR() / 255.0f;
		float tintG = (float)snow.Tint.GetG() / 255.0f;
		float tintB = (float)snow.Tint.GetB() / 255.0f;
		_stSnow.SnowTintAndRim = Vector4(tintR, tintG, tintB, snow.RimStrength);

		// HillHeight is independent of MaxDepth: the snow mesh is lifted by
		// (MaxDepth + HillHeight) at generation time so hills always have room.
		float hillHeight = std::max(0.0f, snow.HillHeight);

		// Combine the scripted/saved level offset with the transient debug UI offset.
		float levelOffset = g_GameFlow->GetLevel(CurrentLevel)->GetSnowSurfaceOffset();
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
}
