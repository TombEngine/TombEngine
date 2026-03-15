#include "framework.h"

#ifdef HAS_SDLGPU

#include "Renderer/Native/SDLGPU/SDLGPUSpriteBatch.h"
#include "Renderer/Native/SDLGPU/SDLGPUGraphicsDevice.h"
#include "Renderer/Native/SDLGPU/SDLGPUTexture2D.h"
#include "Renderer/Native/SDLGPU/SDLGPURenderTarget2D.h"
#include "Renderer/Native/SDLGPU/SDLGPUUtils.h"

#include <SDL3_shadercross/SDL_shadercross.h>
#include <algorithm>
#include <cstring>

namespace TEN::Renderer::Native::SDLGPU
{
	// Inline HLSL for 2D sprite rendering.
	static const char* s_spriteBatchHLSL = R"(
		cbuffer SpriteBatchUniforms : register(b0)
		{
			float2 ScreenSize;
		};

		struct VSInput
		{
			float2 Position : TEXCOORD0;
			float2 UV       : TEXCOORD1;
			float4 Color    : TEXCOORD2;
		};

		struct VSOutput
		{
			float4 Position : SV_Position;
			float2 UV       : TEXCOORD0;
			float4 Color    : TEXCOORD1;
		};

		VSOutput VSSpriteBatch(VSInput input)
		{
			VSOutput output;
			output.Position = float4(
				input.Position.x / ScreenSize.x * 2.0 - 1.0,
				-(input.Position.y / ScreenSize.y * 2.0 - 1.0),
				0.0, 1.0);
			output.UV = input.UV;
			output.Color = input.Color;
			return output;
		}

		Texture2D SpriteTexture : register(t0);
		SamplerState SpriteSampler : register(s0);

		float4 PSSpriteBatch(VSOutput input) : SV_Target0
		{
			return SpriteTexture.Sample(SpriteSampler, input.UV) * input.Color;
		}
	)";

	static SDL_GPUShader* CompileInlineShader(SDL_GPUDevice* device, const char* hlslSource,
	                                           const char* entryPoint, SDL_ShaderCross_ShaderStage stage)
	{
		SDL_ShaderCross_HLSL_Info hlslInfo = {};
		hlslInfo.source = hlslSource;
		hlslInfo.entrypoint = entryPoint;
		hlslInfo.shader_stage = stage;
		hlslInfo.defines = nullptr;
		hlslInfo.include_dir = nullptr;
		hlslInfo.props = 0;

		size_t spirvSize = 0;
		void* spirvData = SDL_ShaderCross_CompileSPIRVFromHLSL(&hlslInfo, &spirvSize);
		if (!spirvData)
		{
			TENLog(std::string("SpriteBatch HLSL compile failed: ") + SDL_GetError(), LogLevel::Error);
			return nullptr;
		}

		// Remap SPIRV descriptor sets for SDL_GPU's Vulkan backend convention.
		// Also returns corrected resource counts.
		auto remapResult = RemapSPIRVDescriptorSets(spirvData, spirvSize, stage);

		// Use corrected resource counts from our SPIRV analysis.
		SDL_ShaderCross_GraphicsShaderResourceInfo resourceInfo = {};
		resourceInfo.num_samplers = remapResult.numSamplerSlots;
		resourceInfo.num_uniform_buffers = remapResult.numUniformBuffers;
		resourceInfo.num_storage_textures = remapResult.numStorageTextures;
		resourceInfo.num_storage_buffers = remapResult.numStorageBuffers;

		TENLog(std::string("SpriteBatch ") + entryPoint + ": SPIRV=" + std::to_string(spirvSize) + "B"
			+ " samplers=" + std::to_string(resourceInfo.num_samplers)
			+ " storageTex=" + std::to_string(resourceInfo.num_storage_textures)
			+ " storageBuf=" + std::to_string(resourceInfo.num_storage_buffers)
			+ " UBOs=" + std::to_string(resourceInfo.num_uniform_buffers),
			LogLevel::Info);

		// Try creating the shader directly via SDL_CreateGPUShader to bypass SDL_ShaderCross
		// SPIRV-direct path issues. This gives us full control over resource counts.
		SDL_GPUShaderCreateInfo sci2 = {};
		sci2.code = (const Uint8*)spirvData;
		sci2.code_size = spirvSize;
		sci2.entrypoint = entryPoint; // DXC uses the HLSL function name as SPIRV entry point.
		sci2.format = SDL_GPU_SHADERFORMAT_SPIRV;
		sci2.stage = (stage == SDL_SHADERCROSS_SHADERSTAGE_VERTEX)
			? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
		sci2.num_samplers = resourceInfo.num_samplers;
		sci2.num_storage_textures = resourceInfo.num_storage_textures;
		sci2.num_storage_buffers = resourceInfo.num_storage_buffers;
		sci2.num_uniform_buffers = resourceInfo.num_uniform_buffers;

		TENLog(std::string("SpriteBatch creating GPU shader ") + entryPoint
			+ " stage=" + std::to_string((int)sci2.stage)
			+ " samplers=" + std::to_string(sci2.num_samplers)
			+ " storageTex=" + std::to_string(sci2.num_storage_textures)
			+ " storageBuf=" + std::to_string(sci2.num_storage_buffers)
			+ " UBOs=" + std::to_string(sci2.num_uniform_buffers),
			LogLevel::Info);

		auto* gpuShader = SDL_CreateGPUShader(device, &sci2);

		SDL_free(spirvData);

		if (!gpuShader)
			TENLog(std::string("SpriteBatch shader creation failed: ") + SDL_GetError(), LogLevel::Error);

		return gpuShader;
	}

	SDLGPUSpriteBatch::SDLGPUSpriteBatch(SDLGPUGraphicsDevice* device)
		: _device(device)
	{
		auto* gpuDevice = device->GetDevice();

		// Compile inline shaders.
		_vertexShader = CompileInlineShader(gpuDevice, s_spriteBatchHLSL,
			"VSSpriteBatch", SDL_SHADERCROSS_SHADERSTAGE_VERTEX);
		_fragmentShader = CompileInlineShader(gpuDevice, s_spriteBatchHLSL,
			"PSSpriteBatch", SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);

		// Create vertex buffer (dynamic, re-uploaded each frame).
		Uint32 vbSize = MAX_BATCH_QUADS * VERTICES_PER_QUAD * FLOATS_PER_VERTEX * sizeof(float);

		SDL_GPUBufferCreateInfo bci = {};
		bci.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
		bci.size = vbSize;
		_vertexBuffer = SDL_CreateGPUBuffer(gpuDevice, &bci);

		// Create sampler (linear, clamp).
		SDL_GPUSamplerCreateInfo sci = {};
		sci.min_filter = SDL_GPU_FILTER_LINEAR;
		sci.mag_filter = SDL_GPU_FILTER_LINEAR;
		sci.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
		sci.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		sci.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		sci.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		_sampler = SDL_CreateGPUSampler(gpuDevice, &sci);

		_vertices.reserve(MAX_BATCH_QUADS * VERTICES_PER_QUAD * FLOATS_PER_VERTEX);
	}

	SDLGPUSpriteBatch::~SDLGPUSpriteBatch()
	{
		auto* gpuDevice = _device ? _device->GetDevice() : nullptr;
		if (gpuDevice)
		{
			if (_pipeline)
				SDL_ReleaseGPUGraphicsPipeline(gpuDevice, _pipeline);
			if (_vertexBuffer)
				SDL_ReleaseGPUBuffer(gpuDevice, _vertexBuffer);
			if (_vertexShader)
				SDL_ReleaseGPUShader(gpuDevice, _vertexShader);
			if (_fragmentShader)
				SDL_ReleaseGPUShader(gpuDevice, _fragmentShader);
			if (_sampler)
				SDL_ReleaseGPUSampler(gpuDevice, _sampler);
		}
	}

	SDL_GPUTexture* SDLGPUSpriteBatch::GetTextureHandle(ITextureBase* texture)
	{
		if (auto* tex = dynamic_cast<SDLGPUTexture2D*>(texture))
			return tex->GetGPUTexture();
		if (auto* rt = dynamic_cast<SDLGPURenderTarget2D*>(texture))
			return rt->GetGPUTexture();
		return nullptr;
	}

	void SDLGPUSpriteBatch::CreatePipeline(SDL_GPUTextureFormat colorFormat, SDL_GPUTextureFormat depthFormat)
	{
		if (!_vertexShader || !_fragmentShader)
			return;

		auto* gpuDevice = _device->GetDevice();

		if (_pipeline)
		{
			SDL_ReleaseGPUGraphicsPipeline(gpuDevice, _pipeline);
			_pipeline = nullptr;
		}

		SDL_GPUGraphicsPipelineCreateInfo pci = {};

		// Vertex input: float2 pos, float2 uv, float4 color.
		SDL_GPUVertexBufferDescription vbDesc = {};
		vbDesc.slot = 0;
		vbDesc.pitch = FLOATS_PER_VERTEX * sizeof(float);
		vbDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

		SDL_GPUVertexAttribute attrs[3] = {};
		attrs[0].location = 0;
		attrs[0].buffer_slot = 0;
		attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
		attrs[0].offset = 0;
		attrs[1].location = 1;
		attrs[1].buffer_slot = 0;
		attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
		attrs[1].offset = 2 * sizeof(float);
		attrs[2].location = 2;
		attrs[2].buffer_slot = 0;
		attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
		attrs[2].offset = 4 * sizeof(float);

		pci.vertex_input_state.vertex_buffer_descriptions = &vbDesc;
		pci.vertex_input_state.num_vertex_buffers = 1;
		pci.vertex_input_state.vertex_attributes = attrs;
		pci.vertex_input_state.num_vertex_attributes = 3;

		pci.vertex_shader = _vertexShader;
		pci.fragment_shader = _fragmentShader;
		pci.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

		// Rasterizer: no culling.
		pci.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
		pci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
		pci.rasterizer_state.front_face = SDL_GPU_FRONTFACE_CLOCKWISE;

		// Depth disabled.
		pci.depth_stencil_state.enable_depth_test = false;
		pci.depth_stencil_state.enable_depth_write = false;
		pci.depth_stencil_state.enable_stencil_test = false;

		// Color target with blend.
		SDL_GPUColorTargetDescription colorTarget = {};
		colorTarget.format = colorFormat;
		colorTarget.blend_state = GetSDLGPUBlendState(_blendMode);

		pci.target_info.color_target_descriptions = &colorTarget;
		pci.target_info.num_color_targets = 1;

		if (depthFormat != SDL_GPU_TEXTUREFORMAT_INVALID)
		{
			pci.target_info.has_depth_stencil_target = true;
			pci.target_info.depth_stencil_format = depthFormat;
		}

		TENLog(std::string("SpriteBatch pipeline create:")
			+ " VS=" + std::to_string((uintptr_t)pci.vertex_shader)
			+ " FS=" + std::to_string((uintptr_t)pci.fragment_shader)
			+ " vtxBufs=" + std::to_string(pci.vertex_input_state.num_vertex_buffers)
			+ " vtxAttrs=" + std::to_string(pci.vertex_input_state.num_vertex_attributes)
			+ " depthClip=" + std::to_string(pci.rasterizer_state.enable_depth_clip)
			+ " depthTest=" + std::to_string(pci.depth_stencil_state.enable_depth_test)
			+ " colors=" + std::to_string(pci.target_info.num_color_targets)
			+ " colorFmt=" + std::to_string((int)colorFormat)
			+ " hasDepth=" + std::to_string(pci.target_info.has_depth_stencil_target)
			+ " depthFmt=" + std::to_string((int)depthFormat)
			+ " blend=" + std::to_string(pci.target_info.color_target_descriptions[0].blend_state.enable_blend),
			LogLevel::Info);

		_pipeline = SDL_CreateGPUGraphicsPipeline(gpuDevice, &pci);
		if (!_pipeline)
			TENLog(std::string("SpriteBatch pipeline creation failed: ") + SDL_GetError(), LogLevel::Error);

		_pipelineColorFmt = colorFormat;
		_pipelineDepthFmt = depthFormat;
		_pipelineBlendMode = _blendMode;
	}

	void SDLGPUSpriteBatch::Begin(SpriteSortingMode sortingMode, BlendMode blendMode)
	{
		_queue.clear();
		_blendMode = blendMode;
	}

	void SDLGPUSpriteBatch::Draw(ITextureBase* texture, RendererRectangle area, Vector4 color)
	{
		SpriteBatchQuad quad;
		quad.TextureHandle = GetTextureHandle(texture);
		quad.Left = (float)area.Left;
		quad.Top = (float)area.Top;
		quad.Right = (float)area.Right;
		quad.Bottom = (float)area.Bottom;
		quad.Color = color;
		_queue.push_back(quad);
	}

	void SDLGPUSpriteBatch::DrawWithUV(SDL_GPUTexture* textureHandle, RendererRectangle area,
	                                    float u0, float v0, float u1, float v1, Vector4 color)
	{
		SpriteBatchQuad quad;
		quad.TextureHandle = textureHandle;
		quad.Left = (float)area.Left;
		quad.Top = (float)area.Top;
		quad.Right = (float)area.Right;
		quad.Bottom = (float)area.Bottom;
		quad.U0 = u0;
		quad.V0 = v0;
		quad.U1 = u1;
		quad.V1 = v1;
		quad.Color = color;
		_queue.push_back(quad);
	}

	void SDLGPUSpriteBatch::Flush()
	{
		if (_queue.empty() || !_device)
			return;

		auto* cmdBuf = _device->GetCommandBuffer();
		if (!cmdBuf)
			return;

		// Sort by texture for batching. Stable sort preserves draw order within same texture.
		std::stable_sort(_queue.begin(), _queue.end(),
			[](const SpriteBatchQuad& a, const SpriteBatchQuad& b)
			{ return a.TextureHandle < b.TextureHandle; });

		// Build vertex data: 6 vertices per quad, 8 floats each.
		_vertices.clear();
		_vertices.reserve(_queue.size() * VERTICES_PER_QUAD * FLOATS_PER_VERTEX);

		for (const auto& q : _queue)
		{
			auto pushVert = [&](float x, float y, float u, float v)
			{
				_vertices.push_back(x); _vertices.push_back(y);
				_vertices.push_back(u); _vertices.push_back(v);
				_vertices.push_back(q.Color.x); _vertices.push_back(q.Color.y);
				_vertices.push_back(q.Color.z); _vertices.push_back(q.Color.w);
			};

			pushVert(q.Left,  q.Top,    q.U0, q.V0);
			pushVert(q.Right, q.Top,    q.U1, q.V0);
			pushVert(q.Right, q.Bottom, q.U1, q.V1);
			pushVert(q.Left,  q.Top,    q.U0, q.V0);
			pushVert(q.Right, q.Bottom, q.U1, q.V1);
			pushVert(q.Left,  q.Bottom, q.U0, q.V1);
		}

		// Upload vertex data via the device's copy pass infrastructure.
		// This properly handles render pass interruption and resumption.
		Uint32 dataSize = (Uint32)(_vertices.size() * sizeof(float));
		_device->UploadBufferDataForBatch(_vertexBuffer, _vertices.data(), dataSize);

		// After copy pass, render pass has been resumed. Get fresh handle.
		auto* renderPass = _device->GetRenderPass();
		if (!renderPass)
			return;

		// Ensure pipeline exists for current render target format and blend mode.
		auto colorFmt = _device->GetCurrentColorFormat();
		auto depthFmt = _device->GetCurrentDepthFormat();
		bool formatsChanged = colorFmt != _pipelineColorFmt ||
		                      depthFmt != _pipelineDepthFmt ||
		                      _blendMode != _pipelineBlendMode;
		if (formatsChanged)
		{
			CreatePipeline(colorFmt, depthFmt);
		}

		if (!_pipeline)
			return;

		SDL_BindGPUGraphicsPipeline(renderPass, _pipeline);

		// Bind vertex buffer.
		SDL_GPUBufferBinding vbBinding = {};
		vbBinding.buffer = _vertexBuffer;
		vbBinding.offset = 0;
		SDL_BindGPUVertexBuffers(renderPass, 0, &vbBinding, 1);

		// Push screen size uniform.
		float screenSize[2] = { (float)_device->GetScreenWidth(), (float)_device->GetScreenHeight() };
		SDL_PushGPUVertexUniformData(cmdBuf, 0, screenSize, sizeof(screenSize));

		// Draw batched by texture.
		int vertexOffset = 0;
		SDL_GPUTexture* currentTex = _queue[0].TextureHandle;

		for (size_t i = 0; i <= _queue.size(); i++)
		{
			bool flush = (i == _queue.size()) || (_queue[i].TextureHandle != currentTex);
			if (flush)
			{
				int count = ((int)i * VERTICES_PER_QUAD) - vertexOffset;

				if (currentTex)
				{
					SDL_GPUTextureSamplerBinding tsb = {};
					tsb.texture = currentTex;
					tsb.sampler = _sampler;
					SDL_BindGPUFragmentSamplers(renderPass, 0, &tsb, 1);
				}

				SDL_DrawGPUPrimitives(renderPass, count, 1, vertexOffset, 0);

				if (i < _queue.size())
				{
					currentTex = _queue[i].TextureHandle;
					vertexOffset = (int)i * VERTICES_PER_QUAD;
				}
			}
		}
	}

	void SDLGPUSpriteBatch::End()
	{
		Flush();
	}
}

#endif
