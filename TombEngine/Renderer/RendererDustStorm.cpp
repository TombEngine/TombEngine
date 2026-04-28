// ============================================================================
// RendererDustStorm.cpp - Volumetric dust storm rendering integration.
//
// Single-pass screen-space raymarched dust storm. Runs after all opaque +
// transparent geometry (so the depth buffer is final) and before HUD / glow.
// Coupled to the engine wind system for direction / speed and gated by the
// camera-room outdoor flag the same way rain and snow are.
// ============================================================================

#include "framework.h"
#include "Renderer/Renderer.h"

#include "Game/camera.h"
#include "Game/effects/weather.h"
#include "Game/room.h"
#include "Renderer/ConstantBuffers/DustStormBuffer.h"
#include "Renderer/DustStorm/DustStormSettings.h"
#include "Specific/level.h"

using namespace TEN::Effects::Environment;
using namespace TEN::Renderer::ConstantBuffers;
using namespace TEN::Renderer::DustStorm;

namespace TEN::Renderer
{
	// Shared with weather system - mirrors WEATHER_SPAWN_DIST_RAIN scale so the
	// dust storm visually fits the rain / snow column.
	static constexpr float DUST_STORM_FAR_REACH        = BLOCK(14.0f);
	static constexpr float DUST_STORM_BASE_STEP        = BLOCK(0.25f);
	static constexpr float DUST_STORM_STEP_GROWTH      = 1.8f;
	static constexpr float DUST_STORM_FOG_BLEND_START  = BLOCK(6.0f);
	static constexpr float DUST_STORM_HEIGHT_COLUMN    = BLOCK(8.0f);

	void Renderer::InitializeDustStorm()
	{
		_cbDustStorm = ConstantBuffer<CDustStormBuffer>(_device.Get());
	}

	// Weather particles are spawned only in outside-table rooms that are marked
	// windy (or underwater). Dust is not used underwater, so we mirror the windy
	// outdoor subset here.
	static bool IsDustStormRoom(const RoomData& room)
	{
		if (room.flags & (ENV_FLAG_WATER | ENV_FLAG_SWAMP))
			return false;

		return (room.flags & ENV_FLAG_WIND) != 0;
	}

	void Renderer::UpdateDustStormBuffer(RenderView& view)
	{
		const auto& settings = _dustStormSettings;

		// Color and density.
		_stDustStorm.Color   = Vector3(settings.ColorR, settings.ColorG, settings.ColorB);
		_stDustStorm.Density = std::clamp(settings.Density, 0.0f, 4.0f);

		// Vertical clamping: convert normalized [0,1] (UI) to world units below
		// the camera (Y-down). MinHeight is the LOWER bound (closer to ground,
		// larger Y), MaxHeight is the UPPER bound (smaller Y).
		float groundY = view.Camera.WorldPosition.y + DUST_STORM_HEIGHT_COLUMN * 0.25f;
		float topY    = view.Camera.WorldPosition.y - DUST_STORM_HEIGHT_COLUMN;
		float minN    = std::clamp(settings.MinHeight, 0.0f, 1.0f);
		float maxN    = std::clamp(settings.MaxHeight, 0.0f, 1.0f);
		if (maxN <= minN)
			maxN = std::min(1.0f, minN + 0.05f);

		// Convert normalized heights to world Y. minN=0 -> dust hugs ground.
		// minN/maxN swap accounts for Y-down: lower world Y = higher altitude.
		_stDustStorm.MinHeight = groundY + (topY - groundY) * minN;
		_stDustStorm.MaxHeight = groundY + (topY - groundY) * maxN;
		if (_stDustStorm.MinHeight < _stDustStorm.MaxHeight)
			std::swap(_stDustStorm.MinHeight, _stDustStorm.MaxHeight);

		// Time + turbulence.
		static float dustTime = 0.0f;
		dustTime += 1.0f / 30.0f; // engine logic ticks at ~30 Hz; cumulative dt avoids drift.
		_stDustStorm.Time            = dustTime;
		_stDustStorm.TurbulenceScale = std::clamp(settings.Turbulence, 0.0f, 4.0f);

		// Wind coupling: take base wind (steady) and scale by user setting.
		auto baseWind = Weather.BaseWind();
		Vector2 windXZ(baseWind.x, baseWind.z);
		float windMag = windXZ.Length();
		if (windMag > 0.001f)
			windXZ /= windMag;
		else
			windXZ = Vector2(1.0f, 0.0f);

		_stDustStorm.WindDirection = windXZ;
		// Convert "base wind" units to world units / sec. Reference rain
		// horizontal velocity is 8 wu / frame at 30 fps -> 240 wu/sec; scale
		// our wind so MAX_BASE_WIND_STRENGTH = ~600 wu/sec @ scale 1.
		_stDustStorm.WindSpeed = windMag * 120.0f * std::clamp(settings.WindSpeedScale, 0.0f, 8.0f);
		_stDustStorm.StepCount = (float)std::clamp(settings.StepCount, 3, 12);

		// Camera and viewport.
		_stDustStorm.ViewSize    = Vector2((float)_screenWidth, (float)_screenHeight);
		_stDustStorm.InvViewSize = Vector2(1.0f / _screenWidth, 1.0f / _screenHeight);
		_stDustStorm.CameraPos   = view.Camera.WorldPosition;
		_stDustStorm.FarPlane    = view.Camera.FarPlane;

		// Sun direction / color from the cloud light state (same source the
		// volumetric clouds use). Falls back to a sensible default if zero.
		Vector3 sunDir = _stVolumetricCloud.LightDirection;
		if (sunDir.LengthSquared() < 0.0001f)
			sunDir = Vector3(0.3f, -0.7f, 0.4f);
		sunDir.Normalize();
		_stDustStorm.LightDirection = sunDir;

		Vector3 sunColor = _stVolumetricCloud.LightColor;
		if (sunColor.LengthSquared() < 0.0001f)
			sunColor = Vector3(1.0f, 0.95f, 0.85f);
		_stDustStorm.LightColor       = sunColor;
		_stDustStorm.AmbientStrength  = 0.35f;

		_stDustStorm.BaseStepDist = DUST_STORM_BASE_STEP;
		_stDustStorm.StepGrowth   = DUST_STORM_STEP_GROWTH;

		// Engine fog blending (reuses the camera CB fog setup).
		_stDustStorm.FogColor          = Vector3(settings.ColorR, settings.ColorG, settings.ColorB) * 0.6f;
		_stDustStorm.FogStartDistance  = DUST_STORM_FOG_BLEND_START;
		_stDustStorm.FogEndDistance    = std::min(DUST_STORM_FAR_REACH, view.Camera.FarPlane);
		_stDustStorm.IntensityFade = 1.0f;
		_stDustStorm.GustMode      = settings.Gusts ? 1.0f : 0.0f;

		// Inverse view-projection for ray reconstruction. HLSL uses row-major
		// matrices (matches DirectXTK SimpleMath default).
		Matrix invVP = view.Camera.ViewProjection.Invert();
		_stDustStorm.InvViewProjection = invVP;

		for (int i = 0; i < DUST_STORM_MAX_OUTDOOR_ROOMS; i++)
		{
			_stDustStorm.OutdoorRoomMins[i] = Vector4::Zero;
			_stDustStorm.OutdoorRoomMaxs[i] = Vector4::Zero;
		}

		int outdoorRoomCount = 0;
		for (const auto* room : view.RoomsToDraw)
		{
			if (room == nullptr)
				continue;

			const auto& nativeRoom = g_Level.Rooms[room->RoomNumber];
			if (!IsDustStormRoom(nativeRoom))
				continue;

			if (outdoorRoomCount >= DUST_STORM_MAX_OUTDOOR_ROOMS)
				break;

			Vector3 roomMin = nativeRoom.Aabb.Center - nativeRoom.Aabb.Extents;
			Vector3 roomMax = nativeRoom.Aabb.Center + nativeRoom.Aabb.Extents;

			_stDustStorm.OutdoorRoomMins[outdoorRoomCount] = Vector4(roomMin.x, roomMin.y, roomMin.z, 0.0f);
			_stDustStorm.OutdoorRoomMaxs[outdoorRoomCount] = Vector4(roomMax.x, roomMax.y, roomMax.z, 0.0f);
			outdoorRoomCount++;
		}

		_stDustStorm.OutdoorRoomCount = (float)outdoorRoomCount;

		UpdateConstantBuffer(_stDustStorm, _cbDustStorm);
	}

	void Renderer::DrawDustStorm(RenderView& view)
	{
		if (!_dustStormSettings.Enabled)
			return;

		UpdateDustStormBuffer(view);
		if (_stDustStorm.OutdoorRoomCount <= 0.0f)
			return;

		_context->RSSetViewports(1, &view.Viewport);
		SetCullMode(CullMode::CounterClockwise);
		SetDepthState(DepthState::None);

		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_fullscreenTriangleInputLayout.Get());

		unsigned int stride = sizeof(PostProcessVertex);
		unsigned int offset = 0;
		_context->IASetVertexBuffers(0, 1,
			_fullscreenTriangleVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);

		// Bind CB to b10 (Hud slot - safe at this stage, see header).
		auto* buf = _cbDustStorm.get();
		_context->PSSetConstantBuffers(10, 1, buf);
		_context->VSSetConstantBuffers(10, 1, buf);

		BindRenderTargetAsTexture(TextureRegister::ColorMap, &_depthRenderTarget,
			SamplerStateRegister::PointWrap);

		// Render to the main render target with alpha blending.
		_context->OMSetRenderTargets(1, _renderTarget.RenderTargetView.GetAddressOf(),
			_renderTarget.DepthStencilView.Get());

		SetBlendMode(BlendMode::AlphaBlend);
		_shaders.Bind(Shader::DustStorm);
		DrawTriangles(3, 0);

		// Cleanup.
		_context->IASetInputLayout(_inputLayout.Get());
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		ID3D11ShaderResourceView* nullSRV = nullptr;
		_context->PSSetShaderResources((UINT)TextureRegister::ColorMap, 1, &nullSRV);

		SetBlendMode(BlendMode::Opaque);
		SetDepthState(DepthState::Write);
	}
}
