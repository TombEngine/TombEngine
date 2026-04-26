#include "framework.h"
#include "Renderer/ShaderManager/ShaderManager.h"

#include "Renderer/RendererUtils.h"
#include "Specific/configuration.h"
#include "Specific/trutils.h"
#include "Version.h"

using namespace TEN::Renderer::Structures;
using namespace TEN::Utils;
using namespace TEN::Renderer::Graphics;

namespace TEN::Renderer::Utils
{
	ShaderManager::~ShaderManager()
	{
		_graphicsDevice = nullptr;

		for (int i = 0; i < (int)Shader::Count; i++)
			Destroy((Shader)i);
	}

	const IShader* ShaderManager::Get(Shader shader)
	{
		return _shaders[(int)shader].get();
	}

	void ShaderManager::Initialize(IGraphicsDevice* graphicsDevice)
	{
		_graphicsDevice = graphicsDevice;
	}

	void ShaderManager::LoadPostprocessShaders()
	{
		Load(Shader::PostProcess, "PostProcess", "", ShaderType::PixelAndVertex, {});

		Load(Shader::PostProcessMonochrome, "PostProcess", "Monochrome", ShaderType::Pixel, {});
		Load(Shader::PostProcessNegative, "PostProcess", "Negative", ShaderType::Pixel, {});
		Load(Shader::PostProcessExclusion, "PostProcess", "Exclusion", ShaderType::Pixel, {});
		Load(Shader::PostProcessFinalPass, "PostProcess", "FinalPass", ShaderType::Pixel, {});
		Load(Shader::PostProcessLensFlare, "PostProcess", "LensFlare", ShaderType::Pixel, {});

		Load(Shader::Ssao, "SSAO", "", ShaderType::Pixel, {});
		Load(Shader::SsaoBlur, "SSAO", "Blur", ShaderType::Pixel, {});

		Load(Shader::Downscale, "PostProcess", "Downscale", ShaderType::Pixel, {});
		Load(Shader::Blur, "PostProcess", "Blur", ShaderType::Pixel, {});
		Load(Shader::GlowCombine, "PostProcess", "GlowCombine", ShaderType::Pixel, {});
	}

	void ShaderManager::LoadAAShaders(int width, int height, bool recompile)
	{
		auto string = std::stringstream{};
		auto defines = std::map<std::string, std::string>{};

		// Set up pixel size macro.
		string << "float4(1.0 / " << width << ", 1.0 / " << height << ", " << width << ", " << height << ")";
		auto pixelSizeText = string.str();
		
		defines["SMAA_RT_METRICS"] = pixelSizeText;

		if (g_Configuration.AntialiasingMode == AntialiasingMode::Medium)
		{
			defines["SMAA_PRESET_MEDIUM"] = "";
		}
		else
		{
			defines["SMAA_PRESET_ULTRA"] = "";
		}

		// defines.push_back({ "SMAA_PREDICATION", "1" });

		// Set up target macro.
		defines["SMAA_HLSL_4_1"] = "1";

		Load(Shader::SmaaEdgeDetection, "SMAA", "EdgeDetection", ShaderType::Vertex, defines, recompile);
		Load(Shader::SmaaLumaEdgeDetection, "SMAA", "LumaEdgeDetection", ShaderType::Pixel, defines, recompile);
		Load(Shader::SmaaColorEdgeDetection, "SMAA", "ColorEdgeDetection", ShaderType::Pixel, defines, recompile);
		Load(Shader::SmaaDepthEdgeDetection, "SMAA", "DepthEdgeDetection", ShaderType::Pixel, defines, recompile);
		Load(Shader::SmaaBlendingWeightCalculation, "SMAA", "BlendingWeightCalculation", ShaderType::PixelAndVertex, defines, recompile);
		Load(Shader::SmaaNeighborhoodBlending, "SMAA", "NeighborhoodBlending", ShaderType::PixelAndVertex, defines, recompile);

		Load(Shader::Fxaa, "FXAA", "", ShaderType::Pixel, {});
	}

	void ShaderManager::LoadCommonShaders()
	{
		auto animated = std::map<std::string, std::string>{};
		animated["ANIMATED"] = "";

		auto roomTransparent = std::map<std::string, std::string>{};
		roomTransparent["TRANSPARENT"] = "";

		auto shadowMap = std::map<std::string, std::string>{};
		shadowMap["SHADOW_MAP"] = "";

		Load(Shader::Rooms, "Rooms", "", ShaderType::PixelAndVertex, {});
		Load(Shader::RoomsTransparent, "Rooms", "", ShaderType::Pixel, roomTransparent);
		Load(Shader::RoomAmbient, "RoomAmbient", "", ShaderType::PixelAndVertex, {});
		Load(Shader::RoomAmbientSky, "RoomAmbient", "Sky", ShaderType::PixelAndVertex, {});
		Load(Shader::Items, "Items", "", ShaderType::PixelAndVertex, {});
		Load(Shader::Sky, "Sky", "", ShaderType::PixelAndVertex, {});
		Load(Shader::Solid, "Solid", "", ShaderType::PixelAndVertex, {});
		Load(Shader::Inventory, "Inventory", "", ShaderType::PixelAndVertex, {});

		Load(Shader::FullScreenQuad, "FullScreenQuad", "", ShaderType::PixelAndVertex, {});

		Load(Shader::ShadowMap, "ShadowMap", "", ShaderType::PixelAndVertex, shadowMap);

		Load(Shader::Hud, "HUD", "", ShaderType::Vertex, {});
		Load(Shader::HudColor, "HUD", "ColoredHUD", ShaderType::Pixel, {});
		Load(Shader::HudDTexture, "HUD", "TexturedHUD", ShaderType::Pixel, {});
		Load(Shader::HudBarColor, "HUD", "TexturedHUDBar", ShaderType::Pixel, {});

		Load(Shader::InstancedStatics, "InstancedStatics", "", ShaderType::PixelAndVertex, {});
		Load(Shader::InstancedSprites, "InstancedSprites", "", ShaderType::PixelAndVertex, {});

		Load(Shader::GBuffer, "GBuffer", "", ShaderType::Pixel, {});
		Load(Shader::GBufferRooms, "GBuffer", "Rooms", ShaderType::Vertex, {});
		Load(Shader::GBufferItems, "GBuffer", "Items", ShaderType::Vertex, {});
		Load(Shader::GBufferInstancedStatics, "GBuffer", "InstancedStatics", ShaderType::Vertex, {});
	}

	void ShaderManager::LoadShaders(int width, int height, bool recompileAAShaders)
	{
		TENLog("Loading shaders...", LogLevel::Info);

		// Unbind any currently bound shader.
		Bind(Shader::None, true);

		// Reset compile counter.
		_compileCounter = 0;

		// LoadAAShaders should always be the first in the list, so that when AA settings are changed,
		// they recompile with the same index as before.

		LoadAAShaders(width, height, recompileAAShaders); 
		LoadCommonShaders();
		LoadPostprocessShaders();
	}

	void ShaderManager::Bind(Shader shader, bool forceNull)
	{
		int shaderIndex = (int)shader;

		if (shaderIndex >= _shaders.size())
		{
			TENLog("Attempt to access nonexistent shader with index " + std::to_string(shaderIndex), LogLevel::Error);
			return;
		}

		const auto& shaderObj = _shaders[shaderIndex];

		_graphicsDevice->BindShader(ShaderStage::VertexShader, shaderObj.get(), forceNull);
		_graphicsDevice->BindShader(ShaderStage::GeometryShader, shaderObj.get(), forceNull);
		_graphicsDevice->BindShader(ShaderStage::PixelShader, shaderObj.get(), forceNull);
	}

	std::unique_ptr<IShader> ShaderManager::LoadOrCompile(const std::string& fileName, const std::string& funcName, ShaderType type, std::map<std::string, std::string> defines, bool forceRecompile)
	{
		// Define paths for native (uncompiled) shaders and compiled shaders.
		auto shaderPath = GetAssetPath(L"Shaders/");
		auto compiledShaderPath = shaderPath + L"Bin/" + ToWString(TEN_VERSION_STRING) + L"/";
		auto wideFileName = ToWString(fileName);

		// Ensure the /Bin subdirectory exists.
		std::filesystem::create_directories(compiledShaderPath);

		ShaderCompileRequest request;
		request.BinaryDirectory = compiledShaderPath;
		request.FileName = wideFileName;
		request.CompileIndex = _compileCounter;
		request.EntryPoint = TEN::Utils::ToWString(funcName);
		request.ForceRecompile = forceRecompile;
		request.Macros = defines;
		request.SourceDirectory = shaderPath;
		request.Type = type;

		auto shader = _graphicsDevice->CreateShader(request);

		_compileCounter++;

		return shader;
	}

	void ShaderManager::Load(Shader shader, const std::string& fileName, const std::string& funcName, ShaderType type, std::map<std::string, std::string> defines, bool forceRecompile)
	{
		Destroy(shader);
		_shaders[(int)shader] = LoadOrCompile(fileName, funcName, type, defines, forceRecompile);
	}

	void ShaderManager::Destroy(Shader shader)
	{
		SAFE_DELETE(_shaders[(int)shader]);
	}
}
