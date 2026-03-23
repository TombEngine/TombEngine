#include "framework.h"
#include "Renderer/ShaderManager/ShaderManager.h"

#include <SDL3/SDL_events.h>

#include "Renderer/RendererUtils.h"
#include "Specific/configuration.h"
#include "Specific/trutils.h"
#include "Version.h"

using namespace TEN::Renderer::Structures;
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
		Load(Shader::GBufferRooms, "GBufferVSRooms", "", ShaderType::Vertex, {});
		Load(Shader::GBufferItems, "GBufferVSItems", "", ShaderType::Vertex, {});
		Load(Shader::GBufferInstancedStatics, "GBufferVSStatics", "", ShaderType::Vertex, {});
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
		auto shaderPath = GetAssetPath("Shaders/");
		auto compiledShaderPath = shaderPath + "Bin/" + TEN_VERSION_STRING + "/";

		// Ensure the /Bin subdirectory exists.
		std::filesystem::create_directories(compiledShaderPath);

		ShaderCompileRequest request;
		request.BinaryDirectory = compiledShaderPath;
		request.FileName = fileName;
		request.CompileIndex = _compileCounter;
		request.EntryPoint = funcName;
		request.ForceRecompile = forceRecompile;
		request.Macros = defines;
		request.SourceDirectory = shaderPath;
		request.Type = type;

		auto shader = _graphicsDevice->CreateShader(request);

		_compileCounter++;

		return shader;

		// Helper function to load or compile a shader.
		/*auto loadOrCompileShader = [this, type, defines, forceRecompile, shaderPath, compiledShaderPath]
			(const std::wstring& baseFileName, const std::string& shaderType, const std::string& functionName, const char* model, ComPtr<ID3D10Blob>& bytecode)
		{
			// Construct full paths using GetAssetPath.
			auto prefix = ((_compileCounter < 10) ? L"0" : L"") + std::to_wstring(_compileCounter) + L"_";
			auto csoFileName = compiledShaderPath + prefix + baseFileName + L"." + std::wstring(shaderType.begin(), shaderType.end()) + L".cso";
			auto srcFileName = shaderPath + baseFileName;

			// Try both .hlsl and .fx extensions for source shader.
			auto srcFileNameWithExtension = srcFileName + L".hlsl";
			if (!std::filesystem::exists(srcFileNameWithExtension))
			{
				srcFileNameWithExtension = srcFileName + L".fx";
				if (!std::filesystem::exists(srcFileNameWithExtension))
				{
					TENLog("Shader source file not found: " + ToString(srcFileNameWithExtension), LogLevel::Error);
					throw std::runtime_error("Shader source file not found.");
				}
			}

			// Check modification dates of source and compiled files.
			if (!forceRecompile && std::filesystem::exists(csoFileName))
			{
				auto csoTime = std::filesystem::last_write_time(csoFileName);
				auto srcTime = std::filesystem::last_write_time(srcFileNameWithExtension);

				// Load compiled shader if it exists and is up-to-date.
				if (srcTime < csoTime)
				{
					auto csoFile = std::ifstream(csoFileName, std::ios::binary);

					if (csoFile.is_open())
					{
						// Load compiled shader.
						csoFile.seekg(0, std::ios::end);
						auto fileSize = csoFile.tellg();
						csoFile.seekg(0, std::ios::beg);

						auto buffer = std::vector<char>(fileSize);
						csoFile.read(buffer.data(), fileSize);
						csoFile.close();

						D3DCreateBlob(fileSize, &bytecode);
						memcpy(bytecode->GetBufferPointer(), buffer.data(), fileSize);

						return;
					}
				}
			}

			// Set up compilation flags according to build configuration.
			unsigned int flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
			if constexpr (DEBUG_BUILD)
			{
				flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
			}
			else
			{
				flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_IEEE_STRICTNESS;
			}

			auto trimmedFileName = std::filesystem::path(srcFileNameWithExtension).filename().string();
			TENLog("Compiling shader: " + trimmedFileName, LogLevel::Info);

			// Compile shader.
			auto errors = ComPtr<ID3D10Blob>{};
			HRESULT res = D3DCompileFromFile(srcFileNameWithExtension.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
											 (shaderType + functionName).c_str(), model, flags, 0, bytecode.GetAddressOf(), errors.GetAddressOf());

			if (FAILED(res))
			{
				if (errors)
				{
					auto error = std::string((const char*)(errors->GetBufferPointer()));
					TENLog(error, LogLevel::Error);
					throw std::runtime_error(error);
				}
				else
				{
					TENLog("Error while compiling shader: " + trimmedFileName, LogLevel::Error);
					throwIfFailed(res);
				}
			}

			// Save compiled shader to .cso file.
			auto outCsoFile = std::ofstream(csoFileName, std::ios::binary);
			if (outCsoFile.is_open())
			{
				outCsoFile.write((const char*)(bytecode->GetBufferPointer()), bytecode->GetBufferSize());
				outCsoFile.close();
			}
		};

		// Load or compile and create pixel shader.
		if (type == ShaderType::Pixel || type == ShaderType::PixelAndVertex)
		{
			loadOrCompileShader(wideFileName, "PS", funcName, "ps_5_0", rendererShader.Pixel.Blob);
			throwIfFailed(_device->CreatePixelShader(rendererShader.Pixel.Blob->GetBufferPointer(), rendererShader.Pixel.Blob->GetBufferSize(),
													 nullptr, rendererShader.Pixel.Shader.GetAddressOf()));
		}

		// Load or compile and create vertex shader.
		if (type == ShaderType::Vertex || type == ShaderType::PixelAndVertex)
		{
			loadOrCompileShader(wideFileName, "VS", funcName, "vs_5_0", rendererShader.Vertex.Blob);
			throwIfFailed(_device->CreateVertexShader(rendererShader.Vertex.Blob->GetBufferPointer(), rendererShader.Vertex.Blob->GetBufferSize(),
													  nullptr, rendererShader.Vertex.Shader.GetAddressOf()));
		}

		// Load or compile and create compute shader.
		if (type == ShaderType::Compute)
		{
			loadOrCompileShader(wideFileName, "CS", funcName, "cs_5_0", rendererShader.Compute.Blob);
			throwIfFailed(_device->CreateComputeShader(rendererShader.Compute.Blob->GetBufferPointer(), rendererShader.Compute.Blob->GetBufferSize(),
													   nullptr, rendererShader.Compute.Shader.GetAddressOf()));
		}

		// Increment compile counter.
		_compileCounter++;

		return rendererShader;*/
	}

	void ShaderManager::Load(Shader shader, const std::string& fileName, const std::string& funcName, ShaderType type, std::map<std::string, std::string> defines, bool forceRecompile)
	{
		Destroy(shader);
		_shaders[(int)shader] = std::move(LoadOrCompile(fileName, funcName, type, defines, forceRecompile));

		// Pump SDL events between shader compilations to prevent the window
		// from being marked "Not Responding" by the OS during the lengthy
		// HLSL → SPIRV compilation phase.
		SDL_PumpEvents();
	}

	void ShaderManager::Destroy(Shader shader)
	{
		SAFE_DELETE(_shaders[(int)shader]);
	}
}
