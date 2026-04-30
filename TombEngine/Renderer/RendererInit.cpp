#include "framework.h"
#include "Renderer/Renderer.h"

#include "Renderer/RendererUtils.h"
#include "Renderer/SMAA/AreaTex.h"
#include "Renderer/SMAA/SearchTex.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/configuration.h"
#include "Specific/memory/Vector.h"
#include "Specific/trutils.h"
#include "Specific/Video/Video.h"
#include "Specific/EngineMain.h"
#include "Renderer/Native/DirectX11/DX11GraphicsDevice.h"
#include "Renderer/Graphics/VRAMTracker.h"

extern GameConfiguration g_Configuration;

using namespace TEN::Renderer::Utils;
using namespace TEN::Video;

namespace TEN::Renderer
{
	void Renderer::Initialize(const std::string& gameDir, int w, int h, bool windowed)
	{
		TENLog("Initializing renderer...", LogLevel::Info);

		_isWindowed = windowed;

		_graphicsDevice->Initialize();
		InitializeScreen(w, h, false);
		InitializeCommonTextures();

		// Load shaders.
		_shaders.LoadShaders(w, h);

		// Initialize input layout using first vertex shader.
		std::vector<RendererInputLayoutField> inputLayoutItems;
		inputLayoutItems.push_back({ VertexInputFormat::VI_RGB32_Float, 0, "POSITION" });
		inputLayoutItems.push_back({ VertexInputFormat::VI_RGBA8_Snorm, 0, "NORMAL" });
		inputLayoutItems.push_back({ VertexInputFormat::VI_RG32_Float, 0, "TEXCOORD" });
		inputLayoutItems.push_back({ VertexInputFormat::VI_RGBA8_Unorm, 0, "COLOR" });
		inputLayoutItems.push_back({ VertexInputFormat::VI_RGBA8_Snorm, 0, "TANGENT" });
		inputLayoutItems.push_back({ VertexInputFormat::VI_RGBA8_Snorm, 1, "NORMAL" });
		inputLayoutItems.push_back({ VertexInputFormat::VI_RGBA8_Uint, 0, "BONEINDICES" });
		inputLayoutItems.push_back({ VertexInputFormat::VI_RGBA8_Uint, 0, "BONEWEIGHTS" });
		inputLayoutItems.push_back({ VertexInputFormat::VI_R32_Uint, 0, "EFFECTS" });
		inputLayoutItems.push_back({ VertexInputFormat::VI_R32_Uint, 1, "EFFECTS" });

		auto roomShader = _shaders.Get(Shader::Rooms);
		_vertexInputLayout = _graphicsDevice->CreateInputLayout(inputLayoutItems, (IShader*)roomShader);
		
		// Initialize constant buffers.
		_cbCameraMatrices = CreateConstantBuffer<CItemBuffer>();
		_cbItem = CreateConstantBuffer<CItemBuffer>();
		_cbSky = CreateConstantBuffer<CSkyBuffer>();
		_cbShadowMap = CreateConstantBuffer<CShadowLightBuffer>();
		_cbRoom = CreateConstantBuffer<CRoomBuffer>();
		_cbAnimated = CreateConstantBuffer<CAnimatedBuffer>();
		_cbPostProcessBuffer = CreateConstantBuffer<CPostProcessBuffer>();
		_cbBlending = CreateConstantBuffer<CBlendingBuffer>();
		_cbInstancedSpriteBuffer = CreateConstantBuffer<CInstancedSpriteBuffer>();
		_cbInstancedStaticMeshBuffer = CreateConstantBuffer<CInstancedStaticMeshBuffer>();
		_cbSMAABuffer = CreateConstantBuffer<CSMAABuffer>();
		_cbMaterial = CreateConstantBuffer<CMaterialBuffer>();

		// Prepare HUD Constant buffer.
		_cbHUDBar = CreateConstantBuffer<CHUDBarBuffer>();
		_cbHUD = CreateConstantBuffer<CHUDBuffer>();
		_stHUD.View = Matrix::CreateLookAt(Vector3::Zero, Vector3(0, 0, 1), Vector3(0, -1, 0));
		_stHUD.Projection = Matrix::CreateOrthographicOffCenter(0, DISPLAY_SPACE_RES.x, 0, DISPLAY_SPACE_RES.y, 0, 1.0f);
		UpdateConstantBuffer(&_stHUD, _cbHUD.get());
		_currentCausticsFrame = 0;

		// Preallocate lists.
		_lines2DToDraw = createVector<RendererLine2D>(MAX_LINES_2D);
		_lines3DToDraw = createVector<RendererLine3D>(MAX_LINES_3D);
		_triangles3DToDraw = createVector<RendererTriangle3D>(MAX_TRIANGLES_3D);

		for (auto& dynamicLightList : _dynamicLights)
			dynamicLightList = createVector<RendererLight>(MAX_DYNAMIC_LIGHTS);

		for (auto& item : _items)
			item.LightsToDraw = createVector<RendererLight*>(MAX_LIGHTS_PER_ITEM);

		for (auto& effect : _effects)
			effect.LightsToDraw = createVector<RendererLight*>(MAX_LIGHTS_PER_ITEM);

		_SMAAAreaTexture = _graphicsDevice->CreateTexture2D(AREATEX_WIDTH, AREATEX_HEIGHT, SurfaceFormat::SF_RG8_Unorm, (unsigned char*)areaTexBytes);
		_SMAASearchTexture = _graphicsDevice->CreateTexture2D(SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, SurfaceFormat::SF_R8_Unorm, (unsigned char*)searchTexBytes);

		CreateSSAONoiseTexture();
		InitializePostProcess();
		InitializeGameBars();
		InitializeSpriteQuad();
		InitializeSky();

		_sortedPolygonsVertices.reserve(MAX_TRANSPARENT_VERTICES);
		_sortedPolygonsIndices.reserve(MAX_TRANSPARENT_VERTICES);
		_sortedPolygonsVertexBuffer = _graphicsDevice->CreateVertexBuffer(MAX_TRANSPARENT_VERTICES, sizeof(Vertex), _sortedPolygonsVertices.data());
		_sortedPolygonsIndexBuffer = _graphicsDevice->CreateIndexBuffer(MAX_TRANSPARENT_VERTICES, _sortedPolygonsIndices.data());

		_spriteVertices.reserve(MAX_SPRITE_VERTICES);
		_spriteVertexBuffer = _graphicsDevice->CreateVertexBuffer(MAX_SPRITE_VERTICES, sizeof(Vertex), _spriteVertices.data());

		// Initialize video player.
		g_VideoPlayer.Initialize(gameDir, _graphicsDevice.get());

		_primitiveBatch = _graphicsDevice->InitializePrimitiveBatch();
		_spriteBatch = _graphicsDevice->InitializeSpriteBatch();
	}

	void Renderer::InitializePostProcess()
	{
		PostProcessVertex vertices[3];

		vertices[0].Position = Vector3(-1.0f, -1.0f, 1.0f);
		vertices[1].Position = Vector3(-1.0f, 3.0f, 1.0f);
		vertices[2].Position = Vector3(3.0f, -1.0f, 1.0f);

		vertices[0].UV = Vector2(0.0f, 1.0f);
		vertices[1].UV = Vector2(0.0f, -1.0f);
		vertices[2].UV = Vector2(2.0f, 1.0f);

		_fullscreenTriangleVertexBuffer = _graphicsDevice->CreateVertexBuffer(3, sizeof(PostProcessVertex), vertices);

		D3D11_INPUT_ELEMENT_DESC postProcessInputLayoutItems[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		std::vector<RendererInputLayoutField> fields;

		fields.push_back({ VertexInputFormat::VI_RGB32_Float, 0, "POSITION" });
		fields.push_back({ VertexInputFormat::VI_RG32_Float, 0, "TEXCOORD" });
		fields.push_back({ VertexInputFormat::VI_RGBA32_Float, 0, "COLOR" });

		auto ppShader = _shaders.Get(Shader::PostProcess);

		_fullScreenVertexInputLayout = _graphicsDevice->CreateInputLayout(fields, (IShader*)ppShader);
	}

	void Renderer::CreateSSAONoiseTexture()
	{
		std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
		std::default_random_engine generator;
		for (unsigned int i = 0; i < 64; ++i)
		{
			Vector4 sample(
				randomFloats(generator) * 2.0 - 1.0,
				randomFloats(generator) * 2.0 - 1.0,
				randomFloats(generator),
				1.0f
			);
			sample.Normalize();
			sample *= randomFloats(generator);

			float scale = (float)i / 64.0;
			scale = Lerp(0.1f, 1.0f, scale * scale);
			sample *= scale;
			sample.w = 1.0f;

			_SSAOKernel.push_back(sample);
		}

		std::vector<Vector4> SSAONoise;
		for (unsigned int i = 0; i < 16; i++)
		{
			Vector4 noise(
				randomFloats(generator) * 2.0 - 1.0,
				randomFloats(generator) * 2.0 - 1.0,
				0.0f,
				1.0f);
			SSAONoise.push_back(noise);
		}

		_SSAONoiseTexture = _graphicsDevice->CreateTexture2D(4, 4, SurfaceFormat::SF_RGBA32_Float, SSAONoise.data());
	}

	void Renderer::InitializeSpriteQuad()
	{
		std::array<Vertex, 4> quadVertices;

		//Bottom Left
		quadVertices[0].Position = Vector3(-0.5, -0.5, 0);
		auto normal = Vector3(-1, -1, 1);
		normal.Normalize();
		quadVertices[0].Normal = PackVector3(normal);
		quadVertices[0].UV = Vector2(0, 1);
		quadVertices[0].Color = VectorColorToRGBA(NEUTRAL_COLOR);
		quadVertices[0].Effects = 3 << INDEX_IN_POLY_VERTEX_SHIFT;

		//Top Left 
		quadVertices[1].Position = Vector3(-0.5, 0.5, 0);
		normal = Vector3(-1, 1, 1);
		normal.Normalize();
		quadVertices[1].Normal = PackVector3(normal);
		quadVertices[1].UV = Vector2(0, 0);
		quadVertices[1].Color = VectorColorToRGBA(NEUTRAL_COLOR);
		quadVertices[1].Effects = 0 << INDEX_IN_POLY_VERTEX_SHIFT;

		//Top Right
		quadVertices[3].Position = Vector3(0.5, 0.5, 0);
		normal = Vector3(1, 1, 1);
		normal.Normalize();
		quadVertices[3].Normal = PackVector3(normal);
		quadVertices[3].UV = Vector2(1, 0);
		quadVertices[3].Color = VectorColorToRGBA(NEUTRAL_COLOR);
		quadVertices[3].Effects = 1 << INDEX_IN_POLY_VERTEX_SHIFT;

		//Bottom Right
		quadVertices[2].Position = Vector3(0.5, -0.5, 0);
		normal = Vector3(1, -1, 1);
		normal.Normalize();
		quadVertices[2].Normal = PackVector3(normal);
		quadVertices[2].UV = Vector2(1, 1);
		quadVertices[2].Color = VectorColorToRGBA(NEUTRAL_COLOR);
		quadVertices[2].Effects = 2 << INDEX_IN_POLY_VERTEX_SHIFT;

		_quadVertexBuffer = _graphicsDevice->CreateVertexBuffer(4, sizeof(Vertex), quadVertices.data());
	}

	void Renderer::InitializeSky()
	{
		auto vertices = std::vector<Vertex>(SKY_VERTICES_COUNT);
		auto indices = std::vector<int>(SKY_INDICES_COUNT);

		int size = SKY_SIZE;

		int lastVertex = 0;
		int lastIndex = 0;

		for (int x = 0; x < SKY_TILES_COUNT; x++)
		{
			for (int z = 0; z < SKY_TILES_COUNT; z++)
			{
				indices[lastIndex + 0] = lastVertex + 0;
				indices[lastIndex + 1] = lastVertex + 1;
				indices[lastIndex + 2] = lastVertex + 2;
				indices[lastIndex + 3] = lastVertex + 0;
				indices[lastIndex + 4] = lastVertex + 2;
				indices[lastIndex + 5] = lastVertex + 3;

				lastIndex += 6;

				vertices[lastVertex].Position.x = -size / 2.0f + x * 512.0f;
				vertices[lastVertex].Position.y = 0.0f;
				vertices[lastVertex].Position.z = -size / 2.0f + (z + 1) * 512.0f;
				vertices[lastVertex].UV.x = x / 20.0f;
				vertices[lastVertex].UV.y = (z + 1) / 20.0f;
				vertices[lastVertex].Color = VectorColorToRGBA(Vector4::One);

				lastVertex++;

				vertices[lastVertex].Position.x = -size / 2.0f + (x + 1) * 512.0f;
				vertices[lastVertex].Position.y = 0.0f;
				vertices[lastVertex].Position.z = -size / 2.0f + (z + 1) * 512.0f;
				vertices[lastVertex].UV.x = (x + 1) / 20.0f;
				vertices[lastVertex].UV.y = (z + 1) / 20.0f;
				vertices[lastVertex].Color = VectorColorToRGBA(Vector4::One);

				lastVertex++;

				vertices[lastVertex].Position.x = -size / 2.0f + (x + 1) * 512.0f;
				vertices[lastVertex].Position.y = 0.0f;
				vertices[lastVertex].Position.z = -size / 2.0f + z * 512.0f;
				vertices[lastVertex].UV.x = (x + 1) / 20.0f;
				vertices[lastVertex].UV.y = z / 20.0f;
				vertices[lastVertex].Color = VectorColorToRGBA(Vector4::One);

				lastVertex++;

				vertices[lastVertex].Position.x = -size / 2.0f + x * 512.0f;
				vertices[lastVertex].Position.y = 0.0f;
				vertices[lastVertex].Position.z = -size / 2.0f + z * 512.0f;
				vertices[lastVertex].UV.x = x / 20.0f;
				vertices[lastVertex].UV.y = z / 20.0f;
				vertices[lastVertex].Color = VectorColorToRGBA(Vector4::One);

				lastVertex++;
			}
		}

		_skyVertexBuffer = _graphicsDevice->CreateVertexBuffer(SKY_VERTICES_COUNT, sizeof(Vertex), vertices.data());
		_skyIndexBuffer = _graphicsDevice->CreateIndexBuffer(SKY_INDICES_COUNT, indices.data());
	}

	void Renderer::InitializeScreen(int w, int h, bool reset)
	{
		// Cleanup resources
		SAFE_DELETE(_backBuffer);
		SAFE_DELETE(_renderTarget);
		SAFE_DELETE(_postProcessRenderTarget[0]);
		SAFE_DELETE(_postProcessRenderTarget[1]);
		SAFE_DELETE(_dumpScreenRenderTarget);
		SAFE_DELETE(_shadowMap);
		SAFE_DELETE(_depthRenderTarget);
		SAFE_DELETE(_normalsAndMaterialIndexRenderTarget);
		SAFE_DELETE(_emissiveAndRoughnessRenderTarget);
		SAFE_DELETE(_SSAORenderTarget);
		SAFE_DELETE(_SSAOBlurredRenderTarget);
		SAFE_DELETE(_glowRenderTarget[0]);
		SAFE_DELETE(_glowRenderTarget[1]);
		SAFE_DELETE(_legacyReflectionsRenderTarget);
		SAFE_DELETE(_skyboxRenderTarget);
		SAFE_DELETE(_SMAASceneRenderTarget);
		SAFE_DELETE(_SMAASceneSRGBRenderTarget);
		SAFE_DELETE(_SMAAEdgesRenderTarget);
		SAFE_DELETE(_SMAABlendRenderTarget);
		
		_backBuffer = _graphicsDevice->InitializeSwapChain(w, h);
		                
		_renderTarget = _graphicsDevice->CreateRenderSurface2D(w, h, SurfaceFormat::SF_RGBA8_Unorm, false, DepthFormat::Depth32);

		_postProcessRenderTarget[0] = _graphicsDevice->CreateRenderSurface2D(w, h, SurfaceFormat::SF_RGBA8_Unorm, false, DepthFormat::None);
		_postProcessRenderTarget[1] = _graphicsDevice->CreateRenderSurface2D(w, h, SurfaceFormat::SF_RGBA8_Unorm, false, DepthFormat::None);
		
		_dumpScreenRenderTarget = _graphicsDevice->CreateRenderSurface2D(w, h, SurfaceFormat::SF_RGBA8_Unorm, false, DepthFormat::Depth32);  
		
		_shadowMap = _graphicsDevice->CreateRenderSurface2D(g_Configuration.ShadowMapSize, g_Configuration.ShadowMapSize, 6, SurfaceFormat::SF_R32_Float, DepthFormat::Depth32);
		
		_depthRenderTarget = _graphicsDevice->CreateRenderSurface2D(w, h, SurfaceFormat::SF_R32_Float, false, DepthFormat::None);
		_normalsAndMaterialIndexRenderTarget = _graphicsDevice->CreateRenderSurface2D(w, h, SurfaceFormat::SF_RGBA8_Unorm, false, DepthFormat::None);
		_emissiveAndRoughnessRenderTarget = _graphicsDevice->CreateRenderSurface2D(w, h, SurfaceFormat::SF_RGBA8_Unorm, false, DepthFormat::None);
		
		_SSAORenderTarget = _graphicsDevice->CreateRenderSurface2D(w, h, SurfaceFormat::SF_RGBA8_Unorm, false, DepthFormat::None);
		_SSAOBlurredRenderTarget = _graphicsDevice->CreateRenderSurface2D(w, h, SurfaceFormat::SF_RGBA8_Unorm, false, DepthFormat::None);
		
		_glowRenderTarget[0] = _graphicsDevice->CreateRenderSurface2D(w / GLOW_DOWNSCALE_FACTOR, h / GLOW_DOWNSCALE_FACTOR, SurfaceFormat::SF_RGBA8_Unorm, false, DepthFormat::None);
		_glowRenderTarget[1] = _graphicsDevice->CreateRenderSurface2D(w / GLOW_DOWNSCALE_FACTOR, h / GLOW_DOWNSCALE_FACTOR, SurfaceFormat::SF_RGBA8_Unorm, false, DepthFormat::None);
		
		_legacyReflectionsRenderTarget = _graphicsDevice->CreateRenderSurface2D(w / LEGACY_REFLECTIONS_DOWNSCALE_FACTOR, h / LEGACY_REFLECTIONS_DOWNSCALE_FACTOR, SurfaceFormat::SF_RGBA8_Unorm, false, DepthFormat::None);
		
		_skyboxRenderTarget = _graphicsDevice->CreateRenderSurface2D(ROOM_AMBIENT_MAP_SIZE, ROOM_AMBIENT_MAP_SIZE, 2, SurfaceFormat::SF_RGBA8_Unorm, DepthFormat::Depth32);

		// Initialize viewport
		_viewport = { 0, 0, w, h, 0.0f, 1.0f };
		_shadowMapViewport = { 0, 0, g_Configuration.ShadowMapSize, g_Configuration.ShadowMapSize, 0.0f, 1.0f };

		InitializeSMAA();
		SetFullScreen();
	}

	void Renderer::InitializeSMAA()
	{
		int w = _graphicsDevice->GetScreenWidth();
		int h = _graphicsDevice->GetScreenHeight();

		_SMAASceneRenderTarget = _graphicsDevice->CreateRenderSurface2D(w, h, SurfaceFormat::SF_RGBA8_Unorm, true, DepthFormat::None);
		_SMAASceneSRGBRenderTarget = _graphicsDevice->CreateRenderSurface2D(_SMAASceneRenderTarget.get(), SurfaceFormat::SF_RGBA8_Unorm_Srgb);
		_SMAAEdgesRenderTarget = _graphicsDevice->CreateRenderSurface2D(w, h, SurfaceFormat::SF_RG8_Unorm, false, DepthFormat::None);
		_SMAABlendRenderTarget = _graphicsDevice->CreateRenderSurface2D(w, h, SurfaceFormat::SF_RGBA8_Unorm, false, DepthFormat::None);
	}

	void Renderer::InitializeCommonTextures()
	{
		// Initialize font.
		auto fontPath = GetAssetPath(L"Textures/Font.spritefont");
		if (!std::filesystem::is_regular_file(fontPath))
			throw std::runtime_error("Font not found; path " + TEN::Utils::ToString(fontPath) + " is missing.");
		     
		_gameFont = _graphicsDevice->InitializeSpriteFont(fontPath);

		// Initialize common textures.
		_logo = SetTextureOrDefault(GetAssetPath(L"Textures/Logo.png"));
		_loadingBarBorder = SetTextureOrDefault(GetAssetPath(L"Textures/LoadingBarBorder.png"));
		_loadingBarInner = SetTextureOrDefault(GetAssetPath(L"Textures/LoadingBarInner.png"));
		_whiteTexture = SetTextureOrDefault(GetAssetPath(L"Textures/WhiteSprite.png"));

		_whiteSprite.Height = _whiteTexture->GetHeight();
		_whiteSprite.Width = _whiteTexture->GetWidth();
		_whiteSprite.UV[0] = Vector2(0.0f, 0.0f);
		_whiteSprite.UV[1] = Vector2(1.0f, 0.0f);
		_whiteSprite.UV[2] = Vector2(1.0f, 1.0f);
		_whiteSprite.UV[3] = Vector2(0.0f, 1.0f);
		_whiteSprite.Texture = _whiteTexture.get();
	}

	void Renderer::Create()
	{
		TENLog("Creating renderer native device...", LogLevel::Info);

		_graphicsDevice = std::make_unique<TEN::Renderer::Native::DirectX11::DX11GraphicsDevice>();
		_graphicsDevice->CreateDevice();

		// Populate adapter info and store in VRAM tracker.
		_adapterInfo = _graphicsDevice->GetAdapterInfo();
		Graphics::VRAMTracker::Get().SetAdapterInfo(_adapterInfo);

		// Initialize shader manager.
		_shaders.Initialize(_graphicsDevice.get());
	}

	void Renderer::ToggleFullScreen(bool force)
	{
		_isWindowed = force ? false : !_isWindowed;
		SetFullScreen();
	}

	void Renderer::SetFullScreen()
	{
		auto window = g_Platform->GetSDL3Window();

		if (!_isWindowed)
		{
			SDL_SetWindowAlwaysOnTop(window, true);
			SDL_SetWindowFullscreen(window, true);

			SDL_SyncWindow(window);
		}
		else
		{
			SDL_SetWindowFullscreen(window, false);
			SDL_SetWindowAlwaysOnTop(window, false);
			SDL_SetWindowBordered(window, true);

			SDL_SetWindowSize(window, _graphicsDevice->GetScreenWidth(), _graphicsDevice->GetScreenHeight());
			SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

			SDL_RestoreWindow(window);
			SDL_RaiseWindow(window);
			SDL_SyncWindow(window);
		}
	}
}
