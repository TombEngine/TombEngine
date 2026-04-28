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
	static constexpr float DUST_STORM_BLEED_WIND_REF   = 960.0f;
	static constexpr float DUST_STORM_BLEED_BOTTOM_MAX = BLOCK(2.5f);
	static constexpr float DUST_STORM_BLEED_TOP_RATIO  = 0.2f;
	static constexpr int   DUST_STORM_MAX_BLEED_VOLUMES = 4;

	void Renderer::InitializeDustStorm()
	{
		_cbDustStorm = ConstantBuffer<CDustStormBuffer>(_device.Get());
	}

	// Returns true if a room is currently considered "outdoor".
	// Mirrors the rain / snow eligibility convention.
	static bool IsRoomOutdoor(int roomIdx)
	{
		if (roomIdx < 0 || roomIdx >= (int)g_Level.Rooms.size())
			return false;

		const auto& room = g_Level.Rooms[roomIdx];
		if (room.flags & (ENV_FLAG_WATER | ENV_FLAG_SWAMP))
			return false;

		return (room.flags & ENV_FLAG_SKYBOX) != 0;
	}

	static bool IsCameraOutdoor(const RenderViewCamera& cam)
	{
		return IsRoomOutdoor(cam.RoomNumber);
	}

	static bool BuildDustBleedVolume(short indoorRoomNumber, const Structures::RendererDoor& door,
		const Vector3& cameraPos, const Vector2& windXZ, float windFactor,
		Vector4& centerAndStrength, Vector4& invBasis0, Vector4& invBasis1, Vector4& invBasis2)
	{
		if (!IsRoomOutdoor(door.RoomNumber) || windFactor <= 0.001f)
			return false;

		auto roomCenteri = GetRoomCenter(indoorRoomNumber);
		Vector3 roomCenter((float)roomCenteri.x, (float)roomCenteri.y, (float)roomCenteri.z);

		Vector3 vertices[4];
		Vector3 portalCenter = Vector3::Zero;
		for (int i = 0; i < 4; i++)
		{
			vertices[i] = Vector3(door.AbsoluteVertices[i].x, door.AbsoluteVertices[i].y, door.AbsoluteVertices[i].z);
			portalCenter += vertices[i];
		}
		portalCenter *= 0.25f;

		Vector2 toIndoor(roomCenter.x - portalCenter.x, roomCenter.z - portalCenter.z);
		if (toIndoor.LengthSquared() < 1.0f)
			toIndoor = Vector2(cameraPos.x - portalCenter.x, cameraPos.z - portalCenter.z);
		if (toIndoor.LengthSquared() < 1.0f)
			return false;

		toIndoor.Normalize();
		float inflow = std::clamp(windXZ.Dot(toIndoor), 0.0f, 1.0f);
		if (inflow <= 0.1f)
			return false;

		Vector3 portalNormal = door.Normal;
		if (portalNormal.LengthSquared() < 0.0001f)
		{
			auto edge0 = vertices[1] - vertices[0];
			auto edge1 = vertices[3] - vertices[0];
			portalNormal = edge0.Cross(edge1);
		}
		if (portalNormal.LengthSquared() < 0.0001f)
			return false;
		portalNormal.Normalize();

		Vector3 worldDown(0.0f, 1.0f, 0.0f);
		Vector3 downAxis = worldDown - portalNormal * worldDown.Dot(portalNormal);
		if (downAxis.LengthSquared() < 0.0001f)
			return false;
		downAxis.Normalize();

		Vector3 rightAxis = portalNormal.Cross(downAxis);
		if (rightAxis.LengthSquared() < 0.0001f)
			return false;
		rightAxis.Normalize();

		float halfWidth = 0.0f;
		float halfHeight = 0.0f;
		for (int i = 0; i < 4; i++)
		{
			auto delta = vertices[i] - portalCenter;
			halfWidth = std::max(halfWidth, std::abs(delta.Dot(rightAxis)));
			halfHeight = std::max(halfHeight, std::abs(delta.Dot(downAxis)));
		}

		if (halfWidth < 8.0f || halfHeight < 8.0f)
			return false;

		Vector3 windDir3D(windXZ.x, 0.0f, windXZ.y);
		if (windDir3D.LengthSquared() < 0.0001f)
			return false;
		windDir3D.Normalize();

		float bottomDepth = DUST_STORM_BLEED_BOTTOM_MAX * windFactor * inflow;
		if (bottomDepth < BLOCK(0.25f))
			return false;

		Vector3 basisX = rightAxis * halfWidth;
		Vector3 basisY = downAxis * halfHeight;
		Vector3 basisZ = windDir3D * bottomDepth;

		float det = basisX.Dot(basisY.Cross(basisZ));
		if (std::abs(det) < 0.0001f)
			return false;

		Vector3 row0 = basisY.Cross(basisZ) / det;
		Vector3 row1 = basisZ.Cross(basisX) / det;
		Vector3 row2 = basisX.Cross(basisY) / det;

		centerAndStrength = Vector4(portalCenter.x, portalCenter.y, portalCenter.z, inflow);
		invBasis0 = Vector4(row0.x, row0.y, row0.z, 0.0f);
		invBasis1 = Vector4(row1.x, row1.y, row1.z, 0.0f);
		invBasis2 = Vector4(row2.x, row2.y, row2.z, 0.0f);
		return true;
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
		_stDustStorm.IntensityFade     = 1.0f;
		_stDustStorm.CameraIsOutdoor   = IsCameraOutdoor(view.Camera) ? 1 : 0;

		// Screen-space wind direction for portal bleed: transform wind as a
		// homogeneous direction (w=0) through VP, then take the XY clip-space
		// components and normalize. Flip Y to convert from NDC (Y-up) to UV
		// space (Y-down). This correctly tracks the camera orientation so the
		// bleed angle matches the wind direction visible on screen.
		{
			Vector3 windWorld3(windXZ.x, 0.0f, windXZ.y);
			windWorld3.Normalize();
			Vector4 windClip = Vector4::Transform(Vector4(windWorld3.x, windWorld3.y, windWorld3.z, 0.0f), view.Camera.ViewProjection);
			Vector2 windScreen(windClip.x, -windClip.y); // flip Y: NDC up = UV down
			float windLen = windScreen.Length();
			_stDustStorm.WindScreenDir = windLen > 0.001f ? windScreen / windLen : Vector2(1.0f, 0.0f);
		}

		// Inverse view-projection for ray reconstruction. HLSL uses row-major
		// matrices (matches DirectXTK SimpleMath default).
		Matrix invVP = view.Camera.ViewProjection.Invert();
		_stDustStorm.InvViewProjection = invVP;
		_stDustStorm.ViewProjection    = view.Camera.ViewProjection;

		_stDustStorm.BleedTopDepthRatio = DUST_STORM_BLEED_TOP_RATIO;
		_stDustStorm.BleedEdgeFadeStart = 0.75f;
		_stDustStorm.BleedDepthFadeStart = 0.15f;
		_stDustStorm.NumBleedVolumes = 0;

		for (int i = 0; i < DUST_STORM_MAX_BLEED_VOLUMES; i++)
		{
			_stDustStorm.BleedVolumeCenterAndStrength[i] = Vector4::Zero;
			_stDustStorm.BleedVolumeInvBasis0[i] = Vector4::Zero;
			_stDustStorm.BleedVolumeInvBasis1[i] = Vector4::Zero;
			_stDustStorm.BleedVolumeInvBasis2[i] = Vector4::Zero;
		}

		if (!_stDustStorm.CameraIsOutdoor)
		{
			int roomNumber = view.Camera.RoomNumber;
			float bleedWindFactor = std::clamp(_stDustStorm.WindSpeed / DUST_STORM_BLEED_WIND_REF, 0.0f, 1.0f);

			if (roomNumber >= 0 && roomNumber < (int)_rooms.size() && bleedWindFactor > 0.001f)
			{
				for (const auto& door : _rooms[roomNumber].Doors)
				{
					if (_stDustStorm.NumBleedVolumes >= DUST_STORM_MAX_BLEED_VOLUMES)
						break;

					Vector4 centerAndStrength = Vector4::Zero;
					Vector4 invBasis0 = Vector4::Zero;
					Vector4 invBasis1 = Vector4::Zero;
					Vector4 invBasis2 = Vector4::Zero;

					if (!BuildDustBleedVolume((short)roomNumber, door, view.Camera.WorldPosition,
						windXZ, bleedWindFactor, centerAndStrength, invBasis0, invBasis1, invBasis2))
					{
						continue;
					}

					int volumeIndex = _stDustStorm.NumBleedVolumes;
					_stDustStorm.BleedVolumeCenterAndStrength[volumeIndex] = centerAndStrength;
					_stDustStorm.BleedVolumeInvBasis0[volumeIndex] = invBasis0;
					_stDustStorm.BleedVolumeInvBasis1[volumeIndex] = invBasis1;
					_stDustStorm.BleedVolumeInvBasis2[volumeIndex] = invBasis2;
					_stDustStorm.NumBleedVolumes++;
				}
			}
		}

		UpdateConstantBuffer(_stDustStorm, _cbDustStorm);
	}

	void Renderer::DrawDustStorm(RenderView& view)
	{
		if (!_dustStormSettings.Enabled)
			return;

		UpdateDustStormBuffer(view);

		// Bind CB to b10 (Hud slot - safe at this stage, see header).
		auto* buf = _cbDustStorm.get();
		_context->PSSetConstantBuffers(10, 1, buf);
		_context->VSSetConstantBuffers(10, 1, buf);

		// Bind linear depth as t0 and outdoor mask as t1.
		BindRenderTargetAsTexture(TextureRegister::ColorMap, &_depthRenderTarget,
			SamplerStateRegister::PointWrap);
		BindRenderTargetAsTexture(TextureRegister::NormalMap, &_outdoorMaskRenderTarget,
			SamplerStateRegister::PointWrap);

		// Render to the main render target with alpha blending.
		_context->OMSetRenderTargets(1, _renderTarget.RenderTargetView.GetAddressOf(),
			_renderTarget.DepthStencilView.Get());
		_context->RSSetViewports(1, &view.Viewport);

		SetBlendMode(BlendMode::AlphaBlend);
		SetCullMode(CullMode::CounterClockwise);
		SetDepthState(DepthState::None);

		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		_context->IASetInputLayout(_fullscreenTriangleInputLayout.Get());

		unsigned int stride = sizeof(PostProcessVertex);
		unsigned int offset = 0;
		_context->IASetVertexBuffers(0, 1,
			_fullscreenTriangleVertexBuffer.Buffer.GetAddressOf(), &stride, &offset);

		_shaders.Bind(Shader::DustStorm);
		DrawTriangles(3, 0);

		// Cleanup.
		_context->IASetInputLayout(_inputLayout.Get());
		_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		ID3D11ShaderResourceView* nullSRV = nullptr;
		_context->PSSetShaderResources((UINT)TextureRegister::ColorMap, 1, &nullSRV);
		_context->PSSetShaderResources((UINT)TextureRegister::NormalMap, 1, &nullSRV);

		SetBlendMode(BlendMode::Opaque);
		SetDepthState(DepthState::Write);
	}
}
