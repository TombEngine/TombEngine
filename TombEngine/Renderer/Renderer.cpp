#include "framework.h"
#include "Renderer/Renderer.h"

#include "Game/camera.h"
#include "Game/effects/tomb4fx.h"
#include "Math/Math.h"
#include "Renderer/Structures/RendererRectangle.h"
#include "Renderer/RenderView.h"
#include "Renderer/RendererUtils.h"
#include "Renderer/Structures/RendererHudBar.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/clock.h"
#include "Specific/trutils.h"

namespace TEN::Renderer
{
	using namespace TEN::Renderer::Structures;
	using namespace TEN::Utils;

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

		SAFE_DELETE(_roomsVertexBuffer);
		SAFE_DELETE(_roomsIndexBuffer);
		SAFE_DELETE(_moveablesVertexBuffer);
		SAFE_DELETE(_moveablesIndexBuffer);
		SAFE_DELETE(_staticsVertexBuffer);
		SAFE_DELETE(_staticsIndexBuffer);

		_roomTextures.clear();
		_moveablesTextures.clear();
		_staticTextures.clear();
		_spritesTextures.clear();
		_animatedTextures.clear();

		_videoSprite = {};
	}

	void Renderer::Lock()
	{
		_isLocked = true;
	}

	void Renderer::UpdateVideoTexture(ITexture2D* texture)
	{
		_videoSprite.X = _videoSprite.Y = 0;
		_videoSprite.Width = texture->GetWidth();
		_videoSprite.Height = texture->GetHeight();
		_videoSprite.UV[0] = Vector2(0,0);
		_videoSprite.UV[1] = Vector2(1,0);
		_videoSprite.UV[2] = Vector2(1,1);
		_videoSprite.UV[3] = Vector2(0,1);
		_videoSprite.Texture = texture;
	}

	void Renderer::ClearVideoTexture()
	{
		_videoSprite = {};
	}

	void Renderer::ReloadShaders(bool recompileAAShaders)
	{
		try
		{
			_shaders.LoadShaders(_graphicsDevice->GetScreenWidth(), _graphicsDevice->GetScreenHeight(), recompileAAShaders);
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

	void Renderer::BindTexture(TextureRegister registerType, ITextureBase* texture, SamplerStateRegister samplerType)
	{
		if (g_GameFlow->IsPointFilterEnabled() && samplerType != SamplerStateRegister::ShadowMap)
		{
			samplerType = SamplerStateRegister::PointWrap;
		}

		// Skip redundant rebinds: a back-to-back draw using the same SRV/sampler on the
		// same register doesn't need another PSSetShaderResources/PSSetSamplers call.
		// Hot path on the sorted transparent pass.
		const int slot = (int)registerType;
		if (slot >= 0 && slot < TEXTURE_BINDING_CACHE_SIZE)
		{
			if (_lastBoundTextures[slot] == texture && _lastBoundSamplers[slot] == samplerType)
				return;

			_lastBoundTextures[slot] = texture;
			_lastBoundSamplers[slot] = samplerType;
		}

		_graphicsDevice->BindTexture(registerType, texture, samplerType);
	}

	void Renderer::UnbindTexture(ShaderStage stage, TextureRegister registerType)
	{
		const int slot = (int)registerType;
		if (slot >= 0 && slot < TEXTURE_BINDING_CACHE_SIZE)
		{
			_lastBoundTextures[slot] = nullptr;
			_lastBoundSamplers[slot] = SamplerStateRegister::None;
		}

		_graphicsDevice->UnbindTexture(stage, registerType);
	}

	void Renderer::ResetTextureBindingCache()
	{
		for (int i = 0; i < TEXTURE_BINDING_CACHE_SIZE; ++i)
		{
			_lastBoundTextures[i] = nullptr;
			_lastBoundSamplers[i] = SamplerStateRegister::None;
		}
	}

	void Renderer::ResetPipelineCache()
	{
		_lastBoundVertexBuffer  = nullptr;
		_lastBoundInputLayout   = nullptr;
		_lastBoundPrimitiveType = (PrimitiveType)-1;
		_hasBoundPipeline       = false;
		_lastBoundPipelineHash  = 0;
		_shaders.ResetBindCache();
	}

	void Renderer::ClearState()
	{
		_graphicsDevice->ClearState();
		ResetTextureBindingCache();
		ResetPipelineCache();
	}

	void Renderer::BindPipeline(const RenderPipelineState& pipeline)
	{
		// Route every piece through the Renderer wrappers — they each have their own
		// per-state dedup so back-to-back identical binds are cheap, AND state changed
		// outside BindPipeline (e.g. a direct SetBlendMode call) doesn't silently get
		// shadowed by a global pipeline hash dedup that thinks "nothing changed".
		_shaders.Bind(pipeline.ShaderId);

		SetBlendMode(pipeline.Blend);
		SetDepthState(pipeline.Depth);
		SetCullMode(pipeline.Cull);
		SetPrimitiveType(pipeline.Topology);
		if (pipeline.InputLayout != nullptr)
			SetInputLayout(pipeline.InputLayout);

		// AlphaTest lives in the PerDraw constant buffer (it's emulated via clip() in HLSL,
		// not a real fixed-function pipeline state). Route through SetAlphaTest so its own
		// dedup logic decides whether to actually upload the CB.
		SetAlphaTest(pipeline.AlphaTest, pipeline.AlphaThreshold);
	}

	void Renderer::BeginRenderPass(const RenderPassDescriptor& pass)
	{
		_graphicsDevice->BeginRenderPass(pass);

		// Binding a target as RTV may unbind the same resource as SRV (DX11 hazard
		// protection). Invalidate our texture-binding dedup so the next BindTexture for
		// that slot doesn't short-circuit thinking the SRV is still live.
		ResetTextureBindingCache();
	}

	void Renderer::EndRenderPass()
	{
		_graphicsDevice->EndRenderPass();
	}

	void Renderer::BindVertexBuffer(IVertexBuffer* vertexBuffer)
	{
		if (vertexBuffer == _lastBoundVertexBuffer)
			return;
		_lastBoundVertexBuffer = vertexBuffer;
		_graphicsDevice->BindVertexBuffer(vertexBuffer);
	}

	void Renderer::SetInputLayout(IInputLayout* inputLayout)
	{
		if (inputLayout == _lastBoundInputLayout)
			return;
		_lastBoundInputLayout = inputLayout;
		_graphicsDevice->SetInputLayout(inputLayout);
	}

	void Renderer::SetPrimitiveType(PrimitiveType primitiveType)
	{
		if (primitiveType == _lastBoundPrimitiveType)
			return;
		_lastBoundPrimitiveType = primitiveType;
		_graphicsDevice->SetPrimitiveType(primitiveType);
	}

	void Renderer::BindRenderTargetAsTexture(TextureRegister registerType, IRenderTarget2D* target, SamplerStateRegister samplerType)
	{
		// Route through BindTexture so this binding goes through the same dedup cache.
		BindTexture(registerType, target, samplerType);
	}

	void Renderer::BindRenderTarget(IRenderTarget2D* renderTarget, IDepthTarget* depthTarget)
	{
		_graphicsDevice->BindRenderTarget(renderTarget, depthTarget);
		ResetTextureBindingCache();
	}

	void Renderer::BindRenderTarget(IRenderTargetBinding renderTarget, IDepthTargetBinding depthTarget)
	{
		_graphicsDevice->BindRenderTarget(renderTarget, depthTarget);
		ResetTextureBindingCache();
	}

	void Renderer::BindRenderTargets(std::vector<IRenderTarget2D*> renderTargets, IDepthTarget* depthTarget)
	{
		_graphicsDevice->BindRenderTargets(renderTargets, depthTarget);
		ResetTextureBindingCache();
	}

	void Renderer::BindRenderTargets(std::vector<IRenderTargetBinding> renderTargets, IDepthTargetBinding depthTarget)
	{
		_graphicsDevice->BindRenderTargets(renderTargets, depthTarget);
		ResetTextureBindingCache();
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

		lights[index].Direction.Normalize();

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

	void Renderer::BindInstancedStaticLights(std::vector<RendererLight*>& lights, int instanceID)
	{
		int lightTypeMask = 0;

		for (int i = 0; i < lights.size(); i++)
			lightTypeMask = lightTypeMask | BindLight(*lights[i], _stObjects.Objects[instanceID].Lights, i);

		_stObjects.Objects[instanceID].NumLights = (int)lights.size() | lightTypeMask;
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

			lightTypeMask = lightTypeMask | BindLight(*lights[i], _stObjects.Objects[0].Lights, numLights);
			_stObjects.Objects[0].Lights[numLights].Intensity *= fadedCoeff;
			numLights++;
		}

		_stObjects.Objects[0].NumLights = numLights | lightTypeMask | (shadow ? SHADOWABLE_MASK : 0);
	}

	void Renderer::BindRoomDecals(const std::vector<RendererDecal>& decals)
	{
		memset(_stRoom.RoomDecals, 0, Decal::COUNT_MAX * sizeof(ShaderDecal));

		if (!g_Configuration.EnableDecals)
		{
			_stRoom.NumRoomDecals = 0;
			return;
		}

		for (int i = 0; i < decals.size(); i++)
		{
			if (i >= Decal::COUNT_MAX)
				break;

			_stRoom.RoomDecals[i].Position = decals[i].Position;
			_stRoom.RoomDecals[i].Radius = decals[i].Radius;
			_stRoom.RoomDecals[i].Opacity = decals[i].Opacity;
			_stRoom.RoomDecals[i].Pattern = decals[i].Pattern;
		}

		_stRoom.NumRoomDecals = (int)decals.size();
	}

	void Renderer::BindConstantBuffer(ShaderStage shaderStage, ConstantBufferRegister constantBufferType, IConstantBuffer* buffer)
	{
		_graphicsDevice->BindConstantBuffer(shaderStage, constantBufferType, buffer);
	}

	void Renderer::BindMaterial(int materialIndex, bool force)
	{
		_numRequestedMaterialsUpdates++;

		auto type = g_Level.Materials[materialIndex].Type;

		int materialTypeAndFlags = (int)type;
		materialTypeAndFlags |= int(g_Level.Materials[materialIndex].HasHeightMap) << 8;
		materialTypeAndFlags |= int(g_Level.Materials[materialIndex].HasAmbientOcclusionMap) << 9;
		materialTypeAndFlags |= int(g_Level.Materials[materialIndex].HasEmissiveMap) << 10;

		if (materialTypeAndFlags == _stPerDraw.MaterialTypeAndFlags &&
			g_Level.Materials[materialIndex].Parameters0 == _stPerDraw.MaterialParameters0 &&
			g_Level.Materials[materialIndex].Parameters1 == _stPerDraw.MaterialParameters1 &&
			g_Level.Materials[materialIndex].Parameters2 == _stPerDraw.MaterialParameters2 &&
			g_Level.Materials[materialIndex].Parameters3 == _stPerDraw.MaterialParameters3 &&
			!force)
		{
			return;
		}

		// TODO: in the future output from TE directly an optimized list
		//if (materialIndex != _lastMaterialIndex || force)
		{
			_stPerDraw.MaterialTypeAndFlags = materialTypeAndFlags;
			_stPerDraw.MaterialParameters0  = g_Level.Materials[materialIndex].Parameters0;
			_stPerDraw.MaterialParameters1  = g_Level.Materials[materialIndex].Parameters1;
			_stPerDraw.MaterialParameters2  = g_Level.Materials[materialIndex].Parameters2;
			_stPerDraw.MaterialParameters3  = g_Level.Materials[materialIndex].Parameters3;

			UpdateConstantBuffer(&_stPerDraw, _cbPerDraw.get());

			_lastMaterialIndex = materialIndex;

			_numExecutedMaterialsUpdates++;
		}

		if (type == MaterialShaderType::Reflective)
			BindRenderTargetAsTexture(TextureRegister::LegacyEnvironmentReflections, _legacyReflectionsRenderTarget->GetRenderTarget(), SamplerStateRegister::AnisotropicClamp);
		else if (type == MaterialShaderType::SkyboxReflective)
			BindTexture(TextureRegister::SkyboxEnvironmentReflections, _skyboxRenderTarget->GetRenderTarget(), SamplerStateRegister::AnisotropicClamp);
	}

	void Renderer::SetBlendMode(BlendMode blendMode, bool force)
	{	
		if (blendMode != _lastBlendMode || force)
		{
			_graphicsDevice->SetBlendMode(blendMode);

			_stPerDraw.BlendMode = (unsigned int)blendMode;
			UpdateConstantBuffer(&_stPerDraw, _cbPerDraw.get());
			
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
			_graphicsDevice->SetDepthState(depthState);
			_lastDepthState = depthState;
		}
	}

	void Renderer::SetCullMode(CullMode cullMode, bool force)
	{ 
		if (_debugPage == RendererDebugPage::WireframeMode)
		{
			if (!_doingFullscreenPass)
			{
				_graphicsDevice->SetCullMode(CullMode::Wireframe);
				return;
			}
			else
			{
				force = true;
			}
		}

		if (cullMode != _lastCullMode || force)
		{
			_graphicsDevice->SetCullMode(cullMode);
			_lastCullMode = cullMode;
		}
	}

	void Renderer::SetAlphaTest(AlphaTestMode mode, float threshold, bool force)
	{
		if (_stPerDraw.AlphaTest != (int)mode ||
			_stPerDraw.AlphaThreshold != threshold ||
			force)
		{
			_stPerDraw.AlphaTest = (int)mode;
			_stPerDraw.AlphaThreshold = threshold;
			UpdateConstantBuffer(&_stPerDraw, _cbPerDraw.get());
		}
	}

	void Renderer::SetScissor(RendererRectangle s)
	{
		_graphicsDevice->SetScissor(s);
	}

	void Renderer::ResetScissor()
	{
		RendererRectangle s;
		s.Left = 0;
		s.Right = _graphicsDevice->GetScreenWidth();
		s.Top = 0;
		s.Bottom = _graphicsDevice->GetScreenHeight();

		_graphicsDevice->SetScissor(s);
	}

	void Renderer::SetGraphicsSettingsChanged()
	{
		_graphicsSettingsChanged = true;
	}

	RendererDebugPage Renderer::GetDebugPage() const
	{
		return _debugPage;
	}
}
