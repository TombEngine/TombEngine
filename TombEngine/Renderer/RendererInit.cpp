#include "framework.h"

// TODO: To framework.
#include <random>
#include <d3dcompiler.h>

#include "Renderer/Renderer.h"
#include "Renderer/RendererUtils.h"
#include "Renderer/SMAA/AreaTex.h"
#include "Renderer/SMAA/SearchTex.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Specific/configuration.h"
#include "Specific/memory/Vector.h"
#include "Specific/trutils.h"
#include "Specific/Video/Video.h"
#include "Specific/winmain.h"

extern GameConfiguration g_Configuration;

using namespace TEN::Renderer::Utils;
using namespace TEN::Video;

namespace TEN::Renderer
{
	void Renderer::Initialize(const std::string& gameDir, int w, int h, bool windowed, HWND handle)
	{
		TENLog("Initializing DX11...", LogLevel::Info);

		_screenWidth = w;
		_screenHeight = h;
		_isWindowed = windowed;

		InitializeScreen(w, h, handle, false);
		InitializeCommonTextures();

		// Load shaders.
		_shaders.LoadShaders(w, h);

		// Initialize render states.
		_renderStates = std::make_unique<CommonStates>(_device.Get());

		// Initialize input layout using first vertex shader.
		D3D11_INPUT_ELEMENT_DESC inputLayoutItems[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "BINORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "ANIMATIONFRAMEOFFSET", 0, DXGI_FORMAT_R32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "EFFECTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "BLENDINDICES", 0, DXGI_FORMAT_R32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "POLYINDEX", 0, DXGI_FORMAT_R32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "DRAWINDEX", 0, DXGI_FORMAT_R32_UINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "HASH", 0, DXGI_FORMAT_R32_SINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		const auto& roomShader = _shaders.Get(Shader::Rooms);
		Utils::throwIfFailed(_device->CreateInputLayout(inputLayoutItems, 12, roomShader.Vertex.Blob->GetBufferPointer(), roomShader.Vertex.Blob->GetBufferSize(), &_inputLayout));

		// Initialize constant buffers.
		_cbCameraMatrices = CreateConstantBuffer<CCameraMatrixBuffer>();
		_cbItem = CreateConstantBuffer<CItemBuffer>();
		_cbStatic = CreateConstantBuffer<CStaticBuffer>();
		_cbLights = CreateConstantBuffer<CLightBuffer>();
		_cbShadowMap = CreateConstantBuffer<CShadowLightBuffer>();
		_cbRoom = CreateConstantBuffer<CRoomBuffer>();
		_cbAnimated = CreateConstantBuffer<CAnimatedBuffer>();
		_cbPostProcessBuffer = CreateConstantBuffer<CPostProcessBuffer>();
		_cbBlending = CreateConstantBuffer<CBlendingBuffer>();
		_cbInstancedSpriteBuffer = CreateConstantBuffer<CInstancedSpriteBuffer>();
		_cbInstancedStaticMeshBuffer = CreateConstantBuffer<CInstancedStaticMeshBuffer>();
		_cbSMAABuffer = CreateConstantBuffer<CSMAABuffer>();

		// Prepare HUD Constant buffer.
		_cbHUDBar = CreateConstantBuffer<CHUDBarBuffer>();
		_cbHUD = CreateConstantBuffer<CHUDBuffer>();
		_cbSprite = CreateConstantBuffer<CSpriteBuffer>();
		_stHUD.View = Matrix::CreateLookAt(Vector3::Zero, Vector3(0, 0, 1), Vector3(0, -1, 0));
		_stHUD.Projection = Matrix::CreateOrthographicOffCenter(0, DISPLAY_SPACE_RES.x, 0, DISPLAY_SPACE_RES.y, 0, 1.0f);
		_cbHUD.UpdateData(_stHUD, _context.Get());
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

		D3D11_BLEND_DESC blendStateDesc{};
		blendStateDesc.AlphaToCoverageEnable = false;
		blendStateDesc.IndependentBlendEnable = false;
		blendStateDesc.RenderTarget[0].BlendEnable = true;
		blendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_REV_SUBTRACT;
		blendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		Utils::throwIfFailed(_device->CreateBlendState(&blendStateDesc, _subtractiveBlendState.GetAddressOf()));

		blendStateDesc.AlphaToCoverageEnable = false;
		blendStateDesc.IndependentBlendEnable = false;
		blendStateDesc.RenderTarget[0].BlendEnable = true;
		blendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_INV_DEST_COLOR;
		blendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_DEST_ALPHA;
		blendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		Utils::throwIfFailed(_device->CreateBlendState(&blendStateDesc, _screenBlendState.GetAddressOf()));

		blendStateDesc.AlphaToCoverageEnable = false;
		blendStateDesc.IndependentBlendEnable = false;
		blendStateDesc.RenderTarget[0].BlendEnable = true;
		blendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_MAX;
		blendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_DEST_ALPHA;
		blendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		Utils::throwIfFailed(_device->CreateBlendState(&blendStateDesc, _lightenBlendState.GetAddressOf()));
		 
		blendStateDesc.AlphaToCoverageEnable = false;
		blendStateDesc.IndependentBlendEnable = false;
		blendStateDesc.RenderTarget[0].BlendEnable = true;
		blendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_REV_SUBTRACT;
		blendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_DEST_ALPHA;
		blendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		Utils::throwIfFailed(_device->CreateBlendState(&blendStateDesc, _excludeBlendState.GetAddressOf()));

		blendStateDesc.AlphaToCoverageEnable = false;
		blendStateDesc.IndependentBlendEnable = true;
		blendStateDesc.RenderTarget[0].BlendEnable = true;
		blendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		blendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		blendStateDesc.RenderTarget[1].BlendEnable = true;
		blendStateDesc.RenderTarget[1].SrcBlend = D3D11_BLEND_ZERO;
		blendStateDesc.RenderTarget[1].DestBlend = D3D11_BLEND_INV_SRC_COLOR;
		blendStateDesc.RenderTarget[1].BlendOp = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[1].SrcBlendAlpha = D3D11_BLEND_ZERO;
		blendStateDesc.RenderTarget[1].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		blendStateDesc.RenderTarget[1].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[1].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED;
		Utils::throwIfFailed(_device->CreateBlendState(&blendStateDesc, _transparencyBlendState.GetAddressOf()));

		blendStateDesc.AlphaToCoverageEnable = false;
		blendStateDesc.IndependentBlendEnable = false;
		blendStateDesc.RenderTarget[0].BlendEnable = true;
		blendStateDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		blendStateDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendStateDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		Utils::throwIfFailed(_device->CreateBlendState(&blendStateDesc, _finalTransparencyBlendState.GetAddressOf()));

		D3D11_SAMPLER_DESC shadowSamplerDesc = {};
		shadowSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		shadowSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		shadowSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		shadowSamplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
		shadowSamplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		Utils::throwIfFailed(_device->CreateSamplerState(&shadowSamplerDesc, _shadowSampler.GetAddressOf()));
		_shadowSampler->SetPrivateData(WKPDID_D3DDebugObjectName, sizeof("ShadowSampler") - 1, "ShadowSampler");

		D3D11_RASTERIZER_DESC rasterizerStateDesc = {};

		rasterizerStateDesc.CullMode = D3D11_CULL_BACK;
		rasterizerStateDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerStateDesc.DepthClipEnable = true;
		rasterizerStateDesc.MultisampleEnable = true;
		rasterizerStateDesc.AntialiasedLineEnable = true;
		rasterizerStateDesc.ScissorEnable = true;
		Utils::throwIfFailed(_device->CreateRasterizerState(&rasterizerStateDesc, _cullCounterClockwiseRasterizerState.GetAddressOf()));

		rasterizerStateDesc.CullMode = D3D11_CULL_FRONT;
		rasterizerStateDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerStateDesc.DepthClipEnable = true;
		rasterizerStateDesc.MultisampleEnable = true;
		rasterizerStateDesc.AntialiasedLineEnable = true;
		rasterizerStateDesc.ScissorEnable = true;
		Utils::throwIfFailed(_device->CreateRasterizerState(&rasterizerStateDesc, _cullClockwiseRasterizerState.GetAddressOf()));

		rasterizerStateDesc.CullMode = D3D11_CULL_NONE;
		rasterizerStateDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerStateDesc.DepthClipEnable = true;
		rasterizerStateDesc.MultisampleEnable = true;
		rasterizerStateDesc.AntialiasedLineEnable = true;
		rasterizerStateDesc.ScissorEnable = true;
		Utils::throwIfFailed(_device->CreateRasterizerState(&rasterizerStateDesc, _cullNoneRasterizerState.GetAddressOf()));

		//_tempRoomAmbientRenderTarget1 = RenderTarget2D(_device.Get(), ROOM_AMBIENT_MAP_SIZE, ROOM_AMBIENT_MAP_SIZE, DXGI_FORMAT_R8G8B8A8_UNORM, false, DXGI_FORMAT_D24_UNORM_S8_UINT);
		//_tempRoomAmbientRenderTarget2 = RenderTarget2D(_device.Get(), ROOM_AMBIENT_MAP_SIZE, ROOM_AMBIENT_MAP_SIZE, DXGI_FORMAT_R8G8B8A8_UNORM, false, DXGI_FORMAT_D24_UNORM_S8_UINT);
		//_tempRoomAmbientRenderTarget3 = RenderTarget2D(_device.Get(), ROOM_AMBIENT_MAP_SIZE, ROOM_AMBIENT_MAP_SIZE, DXGI_FORMAT_R8G8B8A8_UNORM, false, DXGI_FORMAT_D24_UNORM_S8_UINT);
		//_tempRoomAmbientRenderTarget4 = RenderTarget2D(_device.Get(), ROOM_AMBIENT_MAP_SIZE, ROOM_AMBIENT_MAP_SIZE, DXGI_FORMAT_R8G8B8A8_UNORM, false, DXGI_FORMAT_D24_UNORM_S8_UINT);
		 
		_SMAAAreaTexture = Texture2D(_device.Get(), AREATEX_WIDTH, AREATEX_HEIGHT, DXGI_FORMAT_R8G8_UNORM, AREATEX_PITCH, areaTexBytes);
		_SMAASearchTexture = Texture2D(_device.Get(), SEARCHTEX_WIDTH, SEARCHTEX_HEIGHT, DXGI_FORMAT_R8_UNORM, SEARCHTEX_PITCH, searchTexBytes);

		CreateSSAONoiseTexture();
		InitializePostProcess();
		InitializeGameBars();
		InitializeSpriteQuad();
		InitializeSky();

		_roomAmbientMapFront = RenderTarget2D(_device.Get(), ROOM_AMBIENT_MAP_SIZE, ROOM_AMBIENT_MAP_SIZE, DXGI_FORMAT_R8G8B8A8_UNORM, false, DXGI_FORMAT_D32_FLOAT);
		_roomAmbientMapBack = RenderTarget2D(_device.Get(), ROOM_AMBIENT_MAP_SIZE, ROOM_AMBIENT_MAP_SIZE, DXGI_FORMAT_R8G8B8A8_UNORM, false, DXGI_FORMAT_D32_FLOAT);

		_sortedPolygonsVertices.reserve(MAX_TRANSPARENT_VERTICES);
		_sortedPolygonsIndices.reserve(MAX_TRANSPARENT_VERTICES);
		_sortedPolygonsVertexBuffer = VertexBuffer<Vertex>(_device.Get(), MAX_TRANSPARENT_VERTICES, _sortedPolygonsVertices);
		_sortedPolygonsIndexBuffer = IndexBuffer(_device.Get(), MAX_TRANSPARENT_VERTICES, _sortedPolygonsIndices);

		// Initialize video player.
		g_VideoPlayer.Initialize(gameDir, _device.Get(), _context.Get());
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

		_fullscreenTriangleVertexBuffer = VertexBuffer<PostProcessVertex>(_device.Get(), 3, &vertices[0]);

		D3D11_INPUT_ELEMENT_DESC postProcessInputLayoutItems[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		const auto& ppShader = _shaders.Get(Shader::PostProcess);
		Utils::throwIfFailed(_device->CreateInputLayout(postProcessInputLayoutItems, 3, 
							 ppShader.Vertex.Blob->GetBufferPointer(), ppShader.Vertex.Blob->GetBufferSize(), &_fullscreenTriangleInputLayout));
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

		_SSAONoiseTexture = Texture2D(_device.Get(), 4, 4, DXGI_FORMAT_R32G32B32A32_FLOAT, 4 * sizeof(Vector4), SSAONoise.data());
	}

	void Renderer::InitializeSpriteQuad()
	{
		std::array<Vertex, 4> quadVertices;

		//Bottom Left
		quadVertices[0].Position = Vector3(-0.5, -0.5, 0);
		quadVertices[0].Normal = Vector3(-1, -1, 1);
		quadVertices[0].Normal.Normalize();
		quadVertices[0].UV = Vector2(0, 1);
		quadVertices[0].Color = Vector4(1, 1, 1, 1);
		quadVertices[0].IndexInPoly = 3;

		//Top Left 
		quadVertices[1].Position = Vector3(-0.5, 0.5, 0);
		quadVertices[1].Normal = Vector3(-1, 1, 1);
		quadVertices[1].Normal.Normalize();
		quadVertices[1].UV = Vector2(0, 0);
		quadVertices[1].Color = Vector4(1, 1, 1, 1);
		quadVertices[1].IndexInPoly = 0;

		//Top Right
		quadVertices[3].Position = Vector3(0.5, 0.5, 0);
		quadVertices[3].Normal = Vector3(1, 1, 1);
		quadVertices[3].Normal.Normalize();
		quadVertices[3].UV = Vector2(1, 0);
		quadVertices[3].Color = Vector4(1, 1, 1, 1);
		quadVertices[3].IndexInPoly = 1;

		//Bottom Right
		quadVertices[2].Position = Vector3(0.5, -0.5, 0);
		quadVertices[2].Normal = Vector3(1, -1, 1);
		quadVertices[2].Normal.Normalize();
		quadVertices[2].UV = Vector2(1, 1);
		quadVertices[2].Color = Vector4(1, 1, 1, 1);
		quadVertices[2].IndexInPoly = 2;

		_quadVertexBuffer = VertexBuffer<Vertex>(_device.Get(), 4, quadVertices.data());
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
				vertices[lastVertex].Color.x = 1.0f;
				vertices[lastVertex].Color.y = 1.0f;
				vertices[lastVertex].Color.z = 1.0f;
				vertices[lastVertex].Color.w = 1.0f;

				lastVertex++;

				vertices[lastVertex].Position.x = -size / 2.0f + (x + 1) * 512.0f;
				vertices[lastVertex].Position.y = 0.0f;
				vertices[lastVertex].Position.z = -size / 2.0f + (z + 1) * 512.0f;
				vertices[lastVertex].UV.x = (x + 1) / 20.0f;
				vertices[lastVertex].UV.y = (z + 1) / 20.0f;
				vertices[lastVertex].Color.x = 1.0f;
				vertices[lastVertex].Color.y = 1.0f;
				vertices[lastVertex].Color.z = 1.0f;
				vertices[lastVertex].Color.w = 1.0f;

				lastVertex++;

				vertices[lastVertex].Position.x = -size / 2.0f + (x + 1) * 512.0f;
				vertices[lastVertex].Position.y = 0.0f;
				vertices[lastVertex].Position.z = -size / 2.0f + z * 512.0f;
				vertices[lastVertex].UV.x = (x + 1) / 20.0f;
				vertices[lastVertex].UV.y = z / 20.0f;
				vertices[lastVertex].Color.x = 1.0f;
				vertices[lastVertex].Color.y = 1.0f;
				vertices[lastVertex].Color.z = 1.0f;
				vertices[lastVertex].Color.w = 1.0f;

				lastVertex++;

				vertices[lastVertex].Position.x = -size / 2.0f + x * 512.0f;
				vertices[lastVertex].Position.y = 0.0f;
				vertices[lastVertex].Position.z = -size / 2.0f + z * 512.0f;
				vertices[lastVertex].UV.x = x / 20.0f;
				vertices[lastVertex].UV.y = z / 20.0f;
				vertices[lastVertex].Color.x = 1.0f;
				vertices[lastVertex].Color.y = 1.0f;
				vertices[lastVertex].Color.z = 1.0f;
				vertices[lastVertex].Color.w = 1.0f;

				lastVertex++;
			}
		}

		_skyVertexBuffer = VertexBuffer<Vertex>(_device.Get(), SKY_VERTICES_COUNT, vertices.data());
		_skyIndexBuffer = IndexBuffer(_device.Get(), SKY_INDICES_COUNT, indices.data());
	}

	void Renderer::InitializeScreen(int w, int h, HWND handle, bool reset)
	{
		DXGI_SWAP_CHAIN_DESC sd;
		sd.BufferDesc.Width = w;
		sd.BufferDesc.Height = h;
		if (!g_Configuration.EnableHighFramerate)
		{
			_refreshRate = 30;

			sd.BufferDesc.RefreshRate.Numerator = 0;
			sd.BufferDesc.RefreshRate.Denominator = 0;
		}
		else
		{
			_refreshRate = GetCurrentScreenRefreshRate();
			if (_refreshRate == 0)
			{
				_refreshRate = 60;
			}
			
			sd.BufferDesc.RefreshRate.Numerator = _refreshRate;
			sd.BufferDesc.RefreshRate.Denominator = 1;
		}
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		sd.BufferDesc.Scaling = DXGI_MODE_SCALING_STRETCHED;
		sd.Windowed = true;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		sd.Flags = 0;
		sd.OutputWindow = handle;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.BufferCount = 1;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		ComPtr<IDXGIDevice> dxgiDevice;
		Utils::throwIfFailed(_device.As(&dxgiDevice));

		ComPtr<IDXGIAdapter> dxgiAdapter;
		Utils::throwIfFailed(dxgiDevice->GetParent(__uuidof(IDXGIAdapter), &dxgiAdapter));

		ComPtr<IDXGIFactory> dxgiFactory;
		Utils::throwIfFailed(dxgiAdapter->GetParent(__uuidof(IDXGIFactory), &dxgiFactory));

		Utils::throwIfFailed(dxgiFactory->CreateSwapChain(_device.Get(), &sd, &_swapChain));

		dxgiFactory->MakeWindowAssociation(handle, DXGI_MWA_NO_ALT_ENTER);
 
		// Initialize render targets
		ID3D11Texture2D* backBufferTexture = NULL;
		Utils::throwIfFailed(_swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast <void**>(&backBufferTexture)));
		_backBuffer = RenderTarget2D(_device.Get(), backBufferTexture, DXGI_FORMAT_D24_UNORM_S8_UINT);
		                
		_renderTarget = RenderTarget2D(_device.Get(), w, h, DXGI_FORMAT_R8G8B8A8_UNORM, false, DXGI_FORMAT_D24_UNORM_S8_UINT);
		_postProcessRenderTarget[0] = RenderTarget2D(_device.Get(), w, h, DXGI_FORMAT_R8G8B8A8_UNORM, false, DXGI_FORMAT_UNKNOWN);
		_postProcessRenderTarget[1] = RenderTarget2D(_device.Get(), w, h, DXGI_FORMAT_R8G8B8A8_UNORM, false, DXGI_FORMAT_UNKNOWN);
		_tempRenderTarget = RenderTarget2D(_device.Get(), w, h, DXGI_FORMAT_R8G8B8A8_UNORM, false, DXGI_FORMAT_UNKNOWN);
		_dumpScreenRenderTarget = RenderTarget2D(_device.Get(), w, h, DXGI_FORMAT_R8G8B8A8_UNORM, false, DXGI_FORMAT_D24_UNORM_S8_UINT);
		_shadowMap = Texture2DArray(_device.Get(), g_Configuration.ShadowMapSize, 6, DXGI_FORMAT_R32_FLOAT, DXGI_FORMAT_D24_UNORM_S8_UINT);
		_depthRenderTarget = RenderTarget2D(_device.Get(), w, h, DXGI_FORMAT_R32_FLOAT, false, DXGI_FORMAT_UNKNOWN);
		_normalsRenderTarget = RenderTarget2D(_device.Get(), w, h, DXGI_FORMAT_R8G8B8A8_UNORM, false, DXGI_FORMAT_UNKNOWN);
		_SSAORenderTarget = RenderTarget2D(_device.Get(), w, h, DXGI_FORMAT_R8G8B8A8_UNORM, false, DXGI_FORMAT_UNKNOWN);
		_SSAOBlurredRenderTarget = RenderTarget2D(_device.Get(), w, h, DXGI_FORMAT_R8G8B8A8_UNORM, false, DXGI_FORMAT_UNKNOWN);

		// Initialize sprite and primitive batches
		_spriteBatch = std::make_unique<SpriteBatch>(_context.Get());
		_primitiveBatch = std::make_unique<PrimitiveBatch<Vertex>>(_context.Get());

		// Initialize viewport
		_viewport.TopLeftX = 0;
		_viewport.TopLeftY = 0;
		_viewport.Width = w;
		_viewport.Height = h;
		_viewport.MinDepth = 0.0f;
		_viewport.MaxDepth = 1.0f;

		_shadowMapViewport.TopLeftX = 0;
		_shadowMapViewport.TopLeftY = 0;
		_shadowMapViewport.Width = g_Configuration.ShadowMapSize;
		_shadowMapViewport.Height = g_Configuration.ShadowMapSize;
		_shadowMapViewport.MinDepth = 0.0f;
		_shadowMapViewport.MaxDepth = 1.0f;

		_viewportToolkit = Viewport(_viewport.TopLeftX, _viewport.TopLeftY, _viewport.Width, _viewport.Height,
			_viewport.MinDepth, _viewport.MaxDepth);

		// Low AA is done with FXAA, Medium - High AA are done with SMAA.
		if (g_Configuration.AntialiasingMode > AntialiasingMode::Low)
		{
			InitializeSMAA();
		}

		SetFullScreen();
	}

	void Renderer::InitializeSMAA()
	{
		int w = _screenWidth;
		int h = _screenHeight;

		_SMAASceneRenderTarget = RenderTarget2D(_device.Get(), w, h, DXGI_FORMAT_R8G8B8A8_UNORM, true, DXGI_FORMAT_UNKNOWN);
		_SMAASceneSRGBRenderTarget = RenderTarget2D(_device.Get(), &_SMAASceneRenderTarget, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
		_SMAAEdgesRenderTarget = RenderTarget2D(_device.Get(), w, h, DXGI_FORMAT_R8G8_UNORM, false, DXGI_FORMAT_UNKNOWN);
		_SMAABlendRenderTarget = RenderTarget2D(_device.Get(), w, h, DXGI_FORMAT_R8G8B8A8_UNORM, false, DXGI_FORMAT_UNKNOWN);
	}

	void Renderer::InitializeCommonTextures()
	{
		// Initialize font.
		auto fontPath = GetAssetPath(L"Textures/Font.spritefont");
		if (!std::filesystem::is_regular_file(fontPath))
			throw std::runtime_error("Font not found; path " + TEN::Utils::ToString(fontPath) + " is missing.");
		     
		_gameFont = std::make_unique<SpriteFont>(_device.Get(), fontPath.c_str());

		// Initialize common textures.
		SetTextureOrDefault(_logo, GetAssetPath(L"Textures/Logo.png"));
		SetTextureOrDefault(_loadingBarBorder, GetAssetPath(L"Textures/LoadingBarBorder.png"));
		SetTextureOrDefault(_loadingBarInner, GetAssetPath(L"Textures/LoadingBarInner.png"));
		SetTextureOrDefault(_whiteTexture, GetAssetPath(L"Textures/WhiteSprite.png")); 

		_whiteSprite.Height = _whiteTexture.Height;
		_whiteSprite.Width = _whiteTexture.Width;
		_whiteSprite.UV[0] = Vector2(0.0f, 0.0f);
		_whiteSprite.UV[1] = Vector2(1.0f, 0.0f);
		_whiteSprite.UV[2] = Vector2(1.0f, 1.0f);
		_whiteSprite.UV[3] = Vector2(0.0f, 1.0f);
		_whiteSprite.Texture = &_whiteTexture;
	}

	void Renderer::Create()
	{
		TENLog("Creating DX11 renderer device...", LogLevel::Info);

		D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
		D3D_FEATURE_LEVEL featureLevel;
		HRESULT res; 

		if constexpr (DEBUG_BUILD)
		{
			res = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, D3D11_CREATE_DEVICE_DEBUG,
									levels, 1, D3D11_SDK_VERSION, &_device, &featureLevel, &_context);
		}
		else
		{
			res = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, NULL,
									levels, 1, D3D11_SDK_VERSION, &_device, &featureLevel, &_context);
		}

		Utils::throwIfFailed(res);

		// Initialize shader manager.
		_shaders.Initialize(_device, _context);
	}

	void Renderer::ToggleFullScreen(bool force)
	{
		_isWindowed = force ? false : !_isWindowed;
		SetFullScreen();
	}

	void Renderer::SetFullScreen()
	{
		if (!_isWindowed)
		{
			SetWindowLongPtr(WindowsHandle, GWL_STYLE, 0);
			SetWindowLongPtr(WindowsHandle, GWL_EXSTYLE, WS_EX_TOPMOST);
			SetWindowPos(WindowsHandle, HWND_TOP, 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN), SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
			ShowWindow(WindowsHandle, SW_SHOWMAXIMIZED);
		}
		else
		{
			int frameW = GetSystemMetrics(SM_CXPADDEDBORDER);
			int frameX = GetSystemMetrics(SM_CXSIZEFRAME);
			int frameY = GetSystemMetrics(SM_CYSIZEFRAME);
			int frameC = GetSystemMetrics(SM_CYCAPTION);

			int borderWidth = (frameX + frameW) * 2;
			int borderHeight = (frameY + frameW) * 2 + frameC;

			SetWindowLongPtr(WindowsHandle, GWL_STYLE, WS_OVERLAPPEDWINDOW);
			SetWindowLongPtr(WindowsHandle, GWL_EXSTYLE, 0);
			ShowWindow(WindowsHandle, SW_SHOWNORMAL);
			SetWindowPos(WindowsHandle, HWND_TOP, 0, 0, _screenWidth + borderWidth, _screenHeight + borderHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);
		}

		UpdateWindow(WindowsHandle);
	}
}
