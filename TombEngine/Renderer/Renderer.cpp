#include "framework.h"
#include "Renderer/Renderer.h"
#include "Game/camera.h"
#include "Game/effects/tomb4fx.h"
#include "Math/Math.h"
#include "Renderer/Structures/RendererRectangle.h"
#include "Renderer/RenderView.h"
#include "Renderer/RendererUtils.h"
#include "Renderer/Graphics/RenderTargetCube.h"
#include "Renderer/Graphics/VertexBuffer.h"
#include "Renderer/Structures/RendererHudBar.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/clock.h"

namespace TEN::Renderer
{
	using namespace TEN::Renderer::Structures;
	using namespace Utils;

	Renderer g_Renderer;

	Renderer::Renderer() :
		_gameCamera({0, 0, 0}, {0, 0, 1}, {0, 1, 0}, 1, 1, 0, 1, 10, 90),
		_oldGameCamera({ 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }, 1, 1, 0, 1, 10, 90),
		_currentGameCamera({ 0, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }, 1, 1, 0, 1, 10, 90)
	{
	}

	Renderer::~Renderer()
	{
		FreeRendererData();
	}

	void Renderer::FreeRendererData()
	{
		_items.resize(0);
		_effects.resize(0);
		_moveableObjects.resize(0);
		_staticObjects.clear();
		_sprites.resize(0);
		_rooms.resize(0);
		_roomTextures.resize(0);
		_moveablesTextures.resize(0);
		_staticTextures.resize(0);
		_spritesTextures.resize(0);
		_animatedTextures.resize(0);
		_animatedTextureSets.resize(0);

		_shadowLight = nullptr;

		_dynamicLightList = 0;
		for (auto& dynamicLightList : _dynamicLights)
			dynamicLightList.resize(0);

		for (auto& mesh : _meshes)
			delete mesh;
		_meshes.resize(0);
	}

	void Renderer::Lock()
	{
		_isLocked = true;
	}

	void Renderer::UpdateVideoTexture(Texture2D* texture)
	{
		_videoSprite.X = _videoSprite.Y = 0;
		_videoSprite.Width = texture->Width;
		_videoSprite.Height = texture->Height;
		_videoSprite.UV[0] = Vector2(0,0);
		_videoSprite.UV[1] = Vector2(1,0);
		_videoSprite.UV[2] = Vector2(1,1);
		_videoSprite.UV[3] = Vector2(0,1);
		_videoSprite.Texture = texture;
	}

	void Renderer::ReloadShaders(bool recompileAAShaders)
	{
		try
		{
			_shaders.LoadShaders(_screenWidth, _screenHeight, recompileAAShaders);
		}
		catch (const std::exception& e)
		{
			TENLog("An exception occured during shader reload: " + std::string(e.what()), LogLevel::Error);
		}
	}

	int Renderer::Synchronize()
	{
		// Sync the renderer
		int nf = TimeSync();
		if (nf < 2)
		{
			int i = 2 - nf;
			nf = 2;
			do
			{
				while (!TimeSync());
				i--;
			}
			while (i);
		}

		return nf;
	}

	void Renderer::UpdateProgress(float value)
	{
		RenderLoadingScreen(value);
	}

	void Renderer::RenderToCubemap(const RenderTargetCube& dest, const Vector3& pos, int roomNumer)
	{
		for (int i = 0; i < 6; i++)
		{
			auto renderView = RenderView(pos, RenderTargetCube::forwardVectors[i], RenderTargetCube::upVectors[i],
			                             dest.Resolution, dest.Resolution, Camera.pos.RoomNumber, 10, 20480,
			                             90 * RADIAN);  
			//RenderSimpleScene(dest.RenderTargetView[i].Get(), dest.DepthStencilView[i].Get(), renderView);
			_context->ClearState();
		}
	}

	float Renderer::CalculateFrameRate()
	{
		static int last_time = clock();
		static int count = 0;
		static float fps = 0.0f;

		count++;
		if (count == 10)
		{
			double t;
			time_t this_time;
			this_time = clock();
			t = (this_time - last_time) / (double)CLOCKS_PER_SEC;
			last_time = this_time;
			fps = float(count / t);
			count = 0;
		}

		_fps = fps;

		return fps;
	}

	void Renderer::BindTexture(TextureRegister registerType, TextureBase* texture, SamplerStateRegister samplerType)
	{
		_context->PSSetShaderResources((UINT)registerType, 1, texture->ShaderResourceView.GetAddressOf());

		if (g_GameFlow->IsPointFilterEnabled() && samplerType != SamplerStateRegister::ShadowMap)
		{
			samplerType = SamplerStateRegister::PointWrap;
		}

		ID3D11SamplerState* samplerState = nullptr;
		switch (samplerType)
		{
		case SamplerStateRegister::AnisotropicClamp:
			samplerState = _renderStates->AnisotropicClamp();
			break;

		case SamplerStateRegister::AnisotropicWrap:
			samplerState = _renderStates->AnisotropicWrap();
			break;

		case SamplerStateRegister::LinearClamp:
			samplerState = _renderStates->LinearClamp();
			break;

		case SamplerStateRegister::LinearWrap:
			samplerState = _renderStates->LinearWrap();
			break;

		case SamplerStateRegister::PointWrap:
			samplerState = _renderStates->PointWrap();
			break;

		case SamplerStateRegister::ShadowMap:
			samplerState = _shadowSampler.Get();
			break;

		default:
			return;
		}

		_context->PSSetSamplers((UINT)registerType, 1, &samplerState);
	}

	void Renderer::BindRenderTargetAsTexture(TextureRegister registerType, RenderTarget2D* target, SamplerStateRegister samplerType)
	{
		_context->PSSetShaderResources((UINT)registerType, 1, target->ShaderResourceView.GetAddressOf());
		  
		ID3D11SamplerState* samplerState = nullptr;
		switch (samplerType)
		{
		case SamplerStateRegister::AnisotropicClamp:
			samplerState = _renderStates->AnisotropicClamp();
			break;

		case SamplerStateRegister::AnisotropicWrap:
			samplerState = _renderStates->AnisotropicWrap();
			break;

		case SamplerStateRegister::LinearClamp:
			samplerState = _renderStates->LinearClamp();
			break;

		case SamplerStateRegister::LinearWrap:
			samplerState = _renderStates->LinearWrap();
			break;

		case SamplerStateRegister::PointWrap:
			samplerState = _renderStates->PointWrap();
			break;

		case SamplerStateRegister::ShadowMap:
			samplerState = _shadowSampler.Get();
			break;

		default:
			return;
		}

		_context->PSSetSamplers((UINT)registerType, 1, &samplerState);
	}

	int Renderer::BindLight(RendererLight& light, ShaderLight* lights, int index)
	{
		memcpy(&lights[index], &light, sizeof(ShaderLight));

		// Precalculate ranges so that it's not recalculated in shader for every pixel.
		if (light.Type == LightType::Spot)
		{
			lights[index].InRange  = cos(light.InRange * (PI / 180.0f));
			lights[index].OutRange = cos(light.OutRange * (PI / 180.0f));
		}

		// If light has hash, interpolate its position with previous position.
		if (light.Hash != 0)
		{
			lights[index].Position  = Vector3::Lerp(light.PrevPosition, light.Position, GetInterpolationFactor());
			lights[index].Direction = Vector3::Lerp(light.PrevDirection, light.Direction, GetInterpolationFactor());
		}

		ReflectVectorOptionally(lights[index].Position);
		ReflectVectorOptionally(lights[index].Direction);

		// Bitmask light type to filter it in the shader later.
		return (1 << (31 - (int)light.Type));
	}

	void Renderer::BindRoomLights(std::vector<RendererLight*>& lights)
	{
		int lightTypeMask = 0;

		for (int i = 0; i < lights.size(); i++)
			lightTypeMask = lightTypeMask | BindLight(*lights[i], _stRoom.RoomLights, i);
		
		_stRoom.NumRoomLights = (int)lights.size() | lightTypeMask;
	}

	void Renderer::BindStaticLights(std::vector<RendererLight*>& lights)
	{
		int lightTypeMask = 0;

		for (int i = 0; i < lights.size(); i++)
			lightTypeMask = lightTypeMask | BindLight(*lights[i], _stStatic.Lights, i);
		
		_stStatic.NumLights = (int)lights.size() | lightTypeMask;
	}

	void Renderer::BindInstancedStaticLights(std::vector<RendererLight*>& lights, int instanceID)
	{
		int lightTypeMask = 0;

		for (int i = 0; i < lights.size(); i++)
			lightTypeMask = lightTypeMask | BindLight(*lights[i], _stInstancedStaticMeshBuffer.StaticMeshes[instanceID].Lights, i);

		_stInstancedStaticMeshBuffer.StaticMeshes[instanceID].NumLights = (int)lights.size() | lightTypeMask;
	}

	void Renderer::BindMoveableLights(std::vector<RendererLight*>& lights, int roomNumber, int prevRoomNumber, float fade, bool shadow)
	{
		constexpr int SHADOWABLE_MASK = (1 << 16);

		int lightTypeMask = 0;
		int numLights = 0;

		for (int i = 0; i < lights.size(); i++)
		{
			float fadedCoeff = 1.0f;

			// Interpolate lights which don't affect neighbor rooms
			if (!lights[i]->AffectNeighbourRooms && roomNumber != NO_VALUE && lights[i]->RoomNumber != NO_VALUE)
			{
				if (lights[i]->RoomNumber == roomNumber)
					fadedCoeff = fade;
				else if (lights[i]->RoomNumber == prevRoomNumber)
					fadedCoeff = 1.0f - fade;
				else
					continue;
			}

			if (fadedCoeff == 0.0f)
				continue;

			lightTypeMask = lightTypeMask | BindLight(*lights[i], _stItem.Lights, numLights);
			_stItem.Lights[numLights].Intensity *= fadedCoeff;
			numLights++;
		}

		_stItem.NumLights = numLights | lightTypeMask | (shadow ? SHADOWABLE_MASK : 0);
	}

	void Renderer::BindConstantBufferVS(ConstantBufferRegister constantBufferType, ID3D11Buffer** buffer)
	{
		_context->VSSetConstantBuffers(static_cast<UINT>(constantBufferType), 1, buffer);
	}

	void Renderer::BindConstantBufferPS(ConstantBufferRegister constantBufferType, ID3D11Buffer** buffer)
	{
		_context->PSSetConstantBuffers(static_cast<UINT>(constantBufferType), 1, buffer);
	}

	void Renderer::SetBlendMode(BlendMode blendMode, bool force)
	{
		if (blendMode != _lastBlendMode || force)
		{
			switch (blendMode)
			{
			case BlendMode::AlphaBlend:
				_context->OMSetBlendState(_renderStates->NonPremultiplied(), nullptr, 0xFFFFFFFF);
				break;

			case BlendMode::AlphaTest:
				_context->OMSetBlendState(_renderStates->Opaque(), nullptr, 0xFFFFFFFF);
				break;

			case BlendMode::Opaque:
				_context->OMSetBlendState(_renderStates->Opaque(), nullptr, 0xFFFFFFFF);
				break;

			case BlendMode::Subtractive:
				_context->OMSetBlendState(_subtractiveBlendState.Get(), nullptr, 0xFFFFFFFF);
				break;

			case BlendMode::Additive:
				_context->OMSetBlendState(_renderStates->Additive(), nullptr, 0xFFFFFFFF);
				break;

			case BlendMode::Screen:
				_context->OMSetBlendState(_screenBlendState.Get(), nullptr, 0xFFFFFFFF);
				break;

			case BlendMode::Lighten:
				_context->OMSetBlendState(_lightenBlendState.Get(), nullptr, 0xFFFFFFFF);
				break;

			case BlendMode::Exclude:
				_context->OMSetBlendState(_excludeBlendState.Get(), nullptr, 0xFFFFFFFF);
				break;
			}

			_stBlending.BlendMode = static_cast<unsigned int>(blendMode);
			_cbBlending.UpdateData(_stBlending, _context.Get());
			
			_lastBlendMode = blendMode;
		}

		switch (blendMode)
		{
		case BlendMode::Opaque:
		case BlendMode::AlphaTest:
			SetDepthState(DepthState::Write);
			break;

		default:
			SetDepthState(DepthState::Read);
			break;
		}
	}

	void Renderer::SetDepthState(DepthState depthState, bool force)
	{
		if (depthState != _lastDepthState || force)
		{
			switch (depthState)
			{
			case DepthState::Read:
				_context->OMSetDepthStencilState(_renderStates->DepthRead(), 0xFFFFFFFF);
				break;

			case DepthState::Write:
				_context->OMSetDepthStencilState(_renderStates->DepthDefault(), 0xFFFFFFFF);
				break;

			case DepthState::None:
				_context->OMSetDepthStencilState(_renderStates->DepthNone(), 0xFFFFFFFF);
				break;

			}

			_lastDepthState = depthState;
		}
	}

	void Renderer::SetCullMode(CullMode cullMode, bool force)
	{ 
		if (_debugPage == RendererDebugPage::WireframeMode)
		{
			if (!_doingFullscreenPass)
			{
				_context->RSSetState(_renderStates->Wireframe());
				return;
			}
			else
			{
				force = true;
			}
		}

		if (cullMode != _lastCullMode || force)
		{
			switch (cullMode)
			{
			case CullMode::None:
				_context->RSSetState(_cullNoneRasterizerState.Get());
				break;

			case CullMode::CounterClockwise:
				_context->RSSetState(_cullCounterClockwiseRasterizerState.Get());
				break;

			case CullMode::Clockwise:
				_context->RSSetState(_cullClockwiseRasterizerState.Get());
				break;
			}

			_lastCullMode = cullMode;
		}
	}

	void Renderer::SetAlphaTest(AlphaTestMode mode, float threshold, bool force)
	{
		if (_stBlending.AlphaTest != (int)mode ||
			_stBlending.AlphaThreshold != threshold ||
			force)
		{
			_stBlending.AlphaTest = (int)mode;
			_stBlending.AlphaThreshold = threshold;
			_cbBlending.UpdateData(_stBlending, _context.Get());
			BindConstantBufferPS(ConstantBufferRegister::Blending, _cbBlending.get());
		}
	}

	void Renderer::SetScissor(RendererRectangle s)
	{
		D3D11_RECT rects;
		rects.left = s.Left;
		rects.top = s.Top;
		rects.right = s.Right;
		rects.bottom = s.Bottom;

		_context->RSSetScissorRects(1, &rects);
	}

	void Renderer::ResetScissor()
	{
		D3D11_RECT rects[1];
		rects[0].left = 0;
		rects[0].right = _screenWidth;
		rects[0].top = 0;
		rects[0].bottom = _screenHeight;

		_context->RSSetScissorRects(1, rects);
	}

	void Renderer::SetGraphicsSettingsChanged()
	{
		_graphicsSettingsChanged = true;
	}
}
