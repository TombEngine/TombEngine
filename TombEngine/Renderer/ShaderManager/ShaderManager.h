#pragma once

#include "Renderer/Structures/RendererShader.h"

using namespace TEN::Renderer::Structures;

namespace TEN::Renderer::Utils
{
	enum class Shader
	{
		// General

		None,
		Rooms,
		RoomsAnimated,
		RoomsTransparent,
		RoomAmbient,
		RoomAmbientSky,
		Items,
		Statics,
		InstancedStatics,
		Sprites,
		InstancedSprites,
		Sky,
		Solid,
		Inventory,
		FullScreenQuad,
		ShadowMap,

		// HUD

		Hud,
		HudColor,
		HudDTexture,
		HudBarColor,

		// GBuffer

		GBuffer,
		GBufferRooms,
		GBufferRoomsAnimated,
		GBufferItems,
		GBufferStatics,
		GBufferInstancedStatics,

		// SMAA

		SmaaEdgeDetection,
		SmaaLumaEdgeDetection,
		SmaaColorEdgeDetection,
		SmaaDepthEdgeDetection,
		SmaaBlendingWeightCalculation,
		SmaaNeighborhoodBlending,
		Fxaa,

		// Post-process

		PostProcess,
		PostProcessMonochrome,
		PostProcessNegative,
		PostProcessExclusion,
		PostProcessFinalPass,
		PostProcessLensFlare,

		// SSAO

		Ssao,
		SsaoBlur,

		Count
	};

	class ShaderManager
	{
	private:
		ComPtr<ID3D11Device>		_device	 = nullptr;
		ComPtr<ID3D11DeviceContext> _context = nullptr;

		int											   _compileCounter = 0;
		std::array<RendererShader, (int)Shader::Count> _shaders		   = {};

	public:
		ShaderManager() = default;
		~ShaderManager();

		const RendererShader& Get(Shader shader);

		void Initialize(ComPtr<ID3D11Device>& device, ComPtr<ID3D11DeviceContext>& context);
		void LoadShaders(int width, int height, bool recompileAAShaders = false);
		void Bind(Shader shader, bool forceNull = false);

	private:
		void LoadCommonShaders();
		void LoadPostprocessShaders();
		void LoadAAShaders(int width, int height, bool recompile);

		RendererShader LoadOrCompile(const std::string& fileName, const std::string& funcName, ShaderType type, const D3D_SHADER_MACRO* defines, bool forceRecompile);
		void		   Load(Shader shader, const std::string& fileName, const std::string& funcName, ShaderType type, const D3D_SHADER_MACRO* defines = nullptr, bool forceRecompile = false);
		void		   Destroy(Shader shader);
	};
}
