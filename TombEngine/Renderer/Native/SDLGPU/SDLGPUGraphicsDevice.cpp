#include "framework.h"

#ifdef HAS_SDLGPU

#include "Renderer/Native/SDLGPU/SDLGPUGraphicsDevice.h"
#include "Renderer/Native/SDLGPU/SDLGPUUtils.h"
#include "Renderer/Graphics/VRAMAllocation.h"
#include "Specific/trutils.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3_shadercross/SDL_shadercross.h>
#include <stb_image_write.h>
#include <stdexcept>
#include <fstream>
#include <filesystem>

using namespace TEN::Renderer::Native::SDLGPU;

namespace TEN::Renderer::Native::SDLGPU
{
	// -----------------------------------------------------------------------
	// Lifecycle
	// -----------------------------------------------------------------------

	SDLGPUGraphicsDevice::SDLGPUGraphicsDevice(const char* preferredDriver)
	{
		if (preferredDriver)
			_preferredDriver = preferredDriver;
	}

	SDLGPUGraphicsDevice::~SDLGPUGraphicsDevice()
	{
		_pipelineCache.Clear();

		for (auto& [reg, sampler] : _samplers)
		{
			if (sampler)
				SDL_ReleaseGPUSampler(_device, sampler);
		}
		_samplers.clear();

		if (_dummyTexture2D)
		{
			SDL_ReleaseGPUTexture(_device, _dummyTexture2D);
			_dummyTexture2D = nullptr;
		}
		if (_dummyTexture2DArray)
		{
			SDL_ReleaseGPUTexture(_device, _dummyTexture2DArray);
			_dummyTexture2DArray = nullptr;
		}
		// _dummySampler is owned by _samplers map, released below.

		if (_backbufferTexture)
		{
			SDL_ReleaseGPUTexture(_device, _backbufferTexture);
			_backbufferTexture = nullptr;
		}

		if (_device)
		{
			SDL_ReleaseWindowFromGPUDevice(_device, _window);
			SDL_DestroyGPUDevice(_device);
			_device = nullptr;
		}

		SDL_ShaderCross_Quit();
	}

	void SDLGPUGraphicsDevice::CreateDevice()
	{
		// Enable verbose GPU logging for diagnostics.
		SDL_SetLogPriority(SDL_LOG_CATEGORY_GPU, SDL_LOG_PRIORITY_VERBOSE);

		// Suppress DXC HLSL compilation warnings emitted by SDL_ShaderCross via SDL_LogWarn.
		SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_ERROR);

		// Initialize SDL_ShaderCross for HLSL → SPIRV compilation.
		TENLog("SDL_GPU: Initializing ShaderCross...", LogLevel::Info);
		if (!SDL_ShaderCross_Init())
			throw std::runtime_error(std::string("SDL_ShaderCross_Init failed: ") + SDL_GetError());

		TENLog("SDL_GPU: ShaderCross OK. Getting window...", LogLevel::Info);
		_window = SDL_GetWindows(nullptr)[0]; // Get the existing SDL window.

		const char* driverName = _preferredDriver.empty() ? nullptr : _preferredDriver.c_str();

		TENLog(std::string("SDL_GPU: Creating device, driver=") + (driverName ? driverName : "auto"), LogLevel::Info);

		auto requestedFormats = SDL_ShaderCross_GetSPIRVShaderFormats();

		// Isolated SEH wrapper — SDL_CreateGPUDevice can raise structured exceptions
		// (e.g. D3D12 without Graphics Tools installed). Can't use __try in functions
		// with C++ destructors, so isolate it.
		struct DeviceCreator
		{
			static SDL_GPUDevice* TryCreate(SDL_GPUShaderFormat fmt, bool debug, const char* driver)
			{
#ifdef SDL_PLATFORM_WIN32
				__try { return SDL_CreateGPUDevice(fmt, debug, driver); }
				__except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
#else
				return SDL_CreateGPUDevice(fmt, debug, driver);
#endif
			}
		};

		_device = DeviceCreator::TryCreate(requestedFormats, false, driverName);

		if (!_device && driverName)
		{
			TENLog(std::string("SDL_GPU: ") + driverName + " failed (" + SDL_GetError()
				+ "), retrying with auto driver selection...", LogLevel::Warning);
			_device = SDL_CreateGPUDevice(requestedFormats, false, nullptr);
		}

		if (!_device)
			throw std::runtime_error(std::string("SDL_CreateGPUDevice failed: ") + SDL_GetError());

		auto* actualDriver = SDL_GetGPUDeviceDriver(_device);
		TENLog(std::string("SDL_GPU driver: ") + (actualDriver ? actualDriver : "unknown"), LogLevel::Info);

		auto shaderFormats = SDL_GetGPUShaderFormats(_device);
		TENLog("SDL_GPU shader formats: " + std::to_string(shaderFormats)
			+ " (SPIRV=" + std::to_string(!!(shaderFormats & SDL_GPU_SHADERFORMAT_SPIRV))
			+ " DXBC=" + std::to_string(!!(shaderFormats & SDL_GPU_SHADERFORMAT_DXBC))
			+ " DXIL=" + std::to_string(!!(shaderFormats & SDL_GPU_SHADERFORMAT_DXIL))
			+ " MSL=" + std::to_string(!!(shaderFormats & SDL_GPU_SHADERFORMAT_MSL)) + ")",
			LogLevel::Info);

		if (!SDL_ClaimWindowForGPUDevice(_device, _window))
			throw std::runtime_error(std::string("SDL_ClaimWindowForGPUDevice failed: ") + SDL_GetError());

		// Query swapchain format.
		_swapchainFormat = SDL_GetGPUSwapchainTextureFormat(_device, _window);
		TENLog("Swapchain format: " + std::to_string((int)_swapchainFormat), LogLevel::Info);

		_depthStencilFormat = ChooseDepthStencilFormat();
		_pipelineCache.Initialize(_device);
		CreateSamplers();

		// Create dummy textures for filling unbound sampler slots.
		{
			uint32_t white = 0xFFFFFFFF;

			// 1x1 white 2D texture.
			{
				SDL_GPUTextureCreateInfo tci = {};
				tci.type = SDL_GPU_TEXTURETYPE_2D;
				tci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
				tci.width = 1;
				tci.height = 1;
				tci.layer_count_or_depth = 1;
				tci.num_levels = 1;
				tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
				_dummyTexture2D = SDL_CreateGPUTexture(_device, &tci);
				UploadToTexture(_dummyTexture2D, 1, 1, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, &white);
			}

			// 1x1x1 white 2D array texture (1 layer).
			{
				SDL_GPUTextureCreateInfo tci = {};
				tci.type = SDL_GPU_TEXTURETYPE_2D_ARRAY;
				tci.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
				tci.width = 1;
				tci.height = 1;
				tci.layer_count_or_depth = 1;
				tci.num_levels = 1;
				tci.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
				_dummyTexture2DArray = SDL_CreateGPUTexture(_device, &tci);
				UploadToTexture(_dummyTexture2DArray, 1, 1, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, &white);
			}

			// Submit immediately (no render loop yet).
			if (_commandBuffer)
			{
				SDL_SubmitGPUCommandBuffer(_commandBuffer);
				_commandBuffer = nullptr;
			}

			_dummySampler = GetSampler(SamplerStateRegister::LinearClamp);
		}
	}

	SDL_GPUTextureFormat SDLGPUGraphicsDevice::ChooseDepthStencilFormat()
	{
		// Prefer D32_S8 — universally supported. D24_S8 is unsupported on some
		// NVIDIA D3D12 configurations and can cause PSO creation failures.
		if (SDL_GPUTextureSupportsFormat(_device,
				SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT,
				SDL_GPU_TEXTURETYPE_2D,
				SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
		{
			TENLog("Using depth format: D32_FLOAT_S8_UINT", LogLevel::Info);
			return SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
		}

		if (SDL_GPUTextureSupportsFormat(_device,
				SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT,
				SDL_GPU_TEXTURETYPE_2D,
				SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET))
		{
			TENLog("Using depth format: D32_FLOAT_S8_UINT (D24_S8 not supported)", LogLevel::Info);
			return SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
		}

		TENLog("Using depth format: D32_FLOAT (no stencil formats supported)", LogLevel::Warning);
		return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
	}

	void SDLGPUGraphicsDevice::Initialize()
	{
		// Nothing extra needed; CreateDevice handles everything.
	}

	std::unique_ptr<IRenderSurface2D> SDLGPUGraphicsDevice::InitializeSwapChain(int width, int height)
	{
		_screenWidth = width;
		_screenHeight = height;

		// Create backbuffer render target.
		auto rt = std::make_unique<SDLGPURenderTarget2D>(
			_device, width, height, _swapchainFormat, false);
		auto depth = std::make_unique<SDLGPUDepthTarget>(
			_device, width, height, _depthStencilFormat);

		_backbufferTexture = rt->GetGPUTexture();

		return std::make_unique<IRenderSurface2D>(std::move(rt), std::move(depth));
	}

	void SDLGPUGraphicsDevice::ResizeSwapChain(int width, int height)
	{
		_screenWidth = width;
		_screenHeight = height;
		// Backbuffer recreation is handled by the renderer via InitializeSwapChain.
	}

	std::string SDLGPUGraphicsDevice::GetDefaultAdapterName()
	{
		auto driver = SDL_GetGPUDeviceDriver(_device);
		return driver ? std::string(driver) : "SDL_GPU";
	}

	AdapterInfo SDLGPUGraphicsDevice::GetAdapterInfo()
	{
		AdapterInfo info;
		auto driver = SDL_GetGPUDeviceDriver(_device);
		info.Name = driver ? std::string(driver) : "SDL_GPU";
		return info;
	}

	int SDLGPUGraphicsDevice::GetScreenWidth()  { return _screenWidth; }
	int SDLGPUGraphicsDevice::GetScreenHeight() { return _screenHeight; }
	int SDLGPUGraphicsDevice::GetRefreshRate()  { return _refreshRate; }

	// -----------------------------------------------------------------------
	// Samplers
	// -----------------------------------------------------------------------

	void SDLGPUGraphicsDevice::CreateSamplers()
	{
		auto createSampler = [&](SDL_GPUFilter filter, SDL_GPUSamplerMipmapMode mipmap,
		                         SDL_GPUSamplerAddressMode addressMode, bool enableAnisotropy,
		                         float maxAnisotropy, bool enableCompare) -> SDL_GPUSampler*
		{
			SDL_GPUSamplerCreateInfo info = {};
			info.min_filter = filter;
			info.mag_filter = filter;
			info.mipmap_mode = mipmap;
			info.address_mode_u = addressMode;
			info.address_mode_v = addressMode;
			info.address_mode_w = addressMode;
			info.enable_anisotropy = enableAnisotropy;
			info.max_anisotropy = maxAnisotropy;
			info.min_lod = 0.0f;
			info.max_lod = 1000.0f;
			if (enableCompare)
			{
				info.enable_compare = true;
				info.compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
			}
			return SDL_CreateGPUSampler(_device, &info);
		};

		_samplers[SamplerStateRegister::PointWrap] =
			createSampler(SDL_GPU_FILTER_NEAREST, SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
			              SDL_GPU_SAMPLERADDRESSMODE_REPEAT, false, 1.0f, false);

		_samplers[SamplerStateRegister::LinearWrap] =
			createSampler(SDL_GPU_FILTER_LINEAR, SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
			              SDL_GPU_SAMPLERADDRESSMODE_REPEAT, false, 1.0f, false);

		_samplers[SamplerStateRegister::LinearClamp] =
			createSampler(SDL_GPU_FILTER_LINEAR, SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
			              SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE, false, 1.0f, false);

		_samplers[SamplerStateRegister::AnisotropicWrap] =
			createSampler(SDL_GPU_FILTER_LINEAR, SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
			              SDL_GPU_SAMPLERADDRESSMODE_REPEAT, true, 16.0f, false);

		_samplers[SamplerStateRegister::AnisotropicClamp] =
			createSampler(SDL_GPU_FILTER_LINEAR, SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
			              SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE, true, 16.0f, false);

		_samplers[SamplerStateRegister::ShadowMap] =
			createSampler(SDL_GPU_FILTER_LINEAR, SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
			              SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE, false, 1.0f, true);
	}

	SDL_GPUSampler* SDLGPUGraphicsDevice::GetSampler(SamplerStateRegister reg)
	{
		auto it = _samplers.find(reg);
		if (it != _samplers.end())
			return it->second;
		return _samplers[SamplerStateRegister::LinearClamp];
	}

	// -----------------------------------------------------------------------
	// Command buffer helpers
	// -----------------------------------------------------------------------

	void SDLGPUGraphicsDevice::EnsureCommandBuffer()
	{
		if (!_commandBuffer)
			_commandBuffer = SDL_AcquireGPUCommandBuffer(_device);
	}

	SDL_GPUCopyPass* SDLGPUGraphicsDevice::BeginCopyPass()
	{
		EnsureCommandBuffer();

		// If we're inside a render pass, end it first (will resume later).
		if (_inRenderPass && _renderPass)
		{
			SDL_EndGPURenderPass(_renderPass);
			_renderPass = nullptr;
			// _inRenderPass stays true so ResumeRenderPass knows to re-begin.
		}

		return SDL_BeginGPUCopyPass(_commandBuffer);
	}

	void SDLGPUGraphicsDevice::EndCopyPass(SDL_GPUCopyPass* copyPass)
	{
		SDL_EndGPUCopyPass(copyPass);

		// If we interrupted a render pass, resume it.
		if (_inRenderPass)
			ResumeRenderPass();
	}

	void SDLGPUGraphicsDevice::ResumeRenderPass()
	{
		// Re-begin the saved render pass with Load actions (data already in targets).
		auto desc = _savedRenderPassDesc;
		for (auto& ca : desc.ColorAttachments)
			ca.LoadAction = LoadAction::Load;
		desc.DepthAttachment.LoadAction = LoadAction::Load;

		BeginRenderPass(desc);
	}

	void SDLGPUGraphicsDevice::UploadToGPUBuffer(SDL_GPUBuffer* buffer, const void* data, size_t size)
	{
		SDL_GPUTransferBufferCreateInfo tbci = {};
		tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
		tbci.size = (Uint32)size;
		auto transferBuffer = SDL_CreateGPUTransferBuffer(_device, &tbci);

		void* mapped = SDL_MapGPUTransferBuffer(_device, transferBuffer, false);
		std::memcpy(mapped, data, size);
		SDL_UnmapGPUTransferBuffer(_device, transferBuffer);

		auto copyPass = BeginCopyPass();

		SDL_GPUTransferBufferLocation src = {};
		src.transfer_buffer = transferBuffer;
		src.offset = 0;

		SDL_GPUBufferRegion dst = {};
		dst.buffer = buffer;
		dst.offset = 0;
		dst.size = (Uint32)size;

		SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
		EndCopyPass(copyPass);

		SDL_ReleaseGPUTransferBuffer(_device, transferBuffer);
	}

	void SDLGPUGraphicsDevice::UploadToTexture(SDL_GPUTexture* texture, int width, int height,
	                                             SDL_GPUTextureFormat format, const void* data)
	{
		size_t dataSize = ComputeTextureSize(width, height, format);

		SDL_GPUTransferBufferCreateInfo tbci = {};
		tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
		tbci.size = (Uint32)dataSize;
		auto transferBuffer = SDL_CreateGPUTransferBuffer(_device, &tbci);

		void* mapped = SDL_MapGPUTransferBuffer(_device, transferBuffer, false);
		std::memcpy(mapped, data, dataSize);
		SDL_UnmapGPUTransferBuffer(_device, transferBuffer);

		auto copyPass = BeginCopyPass();

		SDL_GPUTextureTransferInfo src = {};
		src.transfer_buffer = transferBuffer;
		src.offset = 0;

		SDL_GPUTextureRegion dst = {};
		dst.texture = texture;
		dst.w = width;
		dst.h = height;
		dst.d = 1;

		SDL_UploadToGPUTexture(copyPass, &src, &dst, false);
		EndCopyPass(copyPass);

		SDL_ReleaseGPUTransferBuffer(_device, transferBuffer);
	}

	// -----------------------------------------------------------------------
	// Public helpers for batch renderers
	// -----------------------------------------------------------------------

	void SDLGPUGraphicsDevice::UploadBufferDataForBatch(SDL_GPUBuffer* buffer, const void* data, Uint32 size, Uint32 offset)
	{
		SDL_GPUTransferBufferCreateInfo tbci = {};
		tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
		tbci.size = size;
		auto transferBuffer = SDL_CreateGPUTransferBuffer(_device, &tbci);

		void* mapped = SDL_MapGPUTransferBuffer(_device, transferBuffer, false);
		std::memcpy(mapped, data, size);
		SDL_UnmapGPUTransferBuffer(_device, transferBuffer);

		auto copyPass = BeginCopyPass();

		SDL_GPUTransferBufferLocation src = {};
		src.transfer_buffer = transferBuffer;
		src.offset = 0;

		SDL_GPUBufferRegion dst = {};
		dst.buffer = buffer;
		dst.offset = offset;
		dst.size = size;

		SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
		EndCopyPass(copyPass);

		SDL_ReleaseGPUTransferBuffer(_device, transferBuffer);
	}

	void SDLGPUGraphicsDevice::DrawPrimitivesForBatch(SDL_GPUBuffer* vertexBuffer, Uint32 vertexCount)
	{
		if (!_renderPass)
			return;

		auto* pipeline = GetCurrentPipeline();
		if (!pipeline)
			return;

		SDL_BindGPUGraphicsPipeline(_renderPass, pipeline);

		// Bind the batch's vertex buffer instead of the engine's current one.
		SDL_GPUBufferBinding vbBinding = {};
		vbBinding.buffer = vertexBuffer;
		vbBinding.offset = 0;
		SDL_BindGPUVertexBuffers(_renderPass, 0, &vbBinding, 1);

		BindTexturesForDraw();
		BindStorageBuffersForDraw();
		PushUniformsForDraw();

		SDL_DrawGPUPrimitives(_renderPass, vertexCount, 1, 0, 0);
	}

	SDL_GPUTextureFormat SDLGPUGraphicsDevice::GetCurrentColorFormat() const
	{
		return (_numCurrentColorTargets > 0) ? _currentColorFormats[0] : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	}

	SDL_GPUTextureFormat SDLGPUGraphicsDevice::GetCurrentDepthFormat() const
	{
		return _hasCurrentDepthTarget ? _currentDepthFormat : SDL_GPU_TEXTUREFORMAT_INVALID;
	}

	// -----------------------------------------------------------------------
	// Vertex Buffer
	// -----------------------------------------------------------------------

	std::unique_ptr<IVertexBuffer> SDLGPUGraphicsDevice::CreateVertexBuffer(int numVertices, int vertexSize, void* data)
	{
		return std::make_unique<SDLGPUVertexBuffer>(_device, numVertices, vertexSize, data);
	}

	void SDLGPUGraphicsDevice::UpdateVertexBuffer(IVertexBuffer* vertexBuffer, int startVertex, int count, void* data)
	{
		auto* vb = static_cast<SDLGPUVertexBuffer*>(vertexBuffer);
		size_t offset = (size_t)startVertex * vb->GetStride();
		size_t size = (size_t)count * vb->GetStride();

		// Upload via transfer buffer + copy pass.
		SDL_GPUTransferBufferCreateInfo tbci = {};
		tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
		tbci.size = (Uint32)size;
		auto transferBuffer = SDL_CreateGPUTransferBuffer(_device, &tbci);

		void* mapped = SDL_MapGPUTransferBuffer(_device, transferBuffer, false);
		std::memcpy(mapped, data, size);
		SDL_UnmapGPUTransferBuffer(_device, transferBuffer);

		auto copyPass = BeginCopyPass();

		SDL_GPUTransferBufferLocation src = {};
		src.transfer_buffer = transferBuffer;
		src.offset = 0;

		SDL_GPUBufferRegion dst = {};
		dst.buffer = vb->GetGPUBuffer();
		dst.offset = (Uint32)offset;
		dst.size = (Uint32)size;

		SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
		EndCopyPass(copyPass);

		SDL_ReleaseGPUTransferBuffer(_device, transferBuffer);
	}

	void SDLGPUGraphicsDevice::BindVertexBuffer(IVertexBuffer* vertexBuffer)
	{
		_currentVertexBuffer = static_cast<SDLGPUVertexBuffer*>(vertexBuffer);
	}

	// -----------------------------------------------------------------------
	// Index Buffer
	// -----------------------------------------------------------------------

	std::unique_ptr<IIndexBuffer> SDLGPUGraphicsDevice::CreateIndexBuffer(int numIndices, int* data)
	{
		return std::make_unique<SDLGPUIndexBuffer>(_device, numIndices, data);
	}

	void SDLGPUGraphicsDevice::UpdateIndexBuffer(IIndexBuffer* indexBuffer, int numIndices, int startIndex, int* data)
	{
		auto* ib = static_cast<SDLGPUIndexBuffer*>(indexBuffer);
		size_t offset = (size_t)startIndex * sizeof(int);
		size_t size = (size_t)numIndices * sizeof(int);

		SDL_GPUTransferBufferCreateInfo tbci = {};
		tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
		tbci.size = (Uint32)size;
		auto transferBuffer = SDL_CreateGPUTransferBuffer(_device, &tbci);

		void* mapped = SDL_MapGPUTransferBuffer(_device, transferBuffer, false);
		std::memcpy(mapped, data, size);
		SDL_UnmapGPUTransferBuffer(_device, transferBuffer);

		auto copyPass = BeginCopyPass();

		SDL_GPUTransferBufferLocation src = {};
		src.transfer_buffer = transferBuffer;
		src.offset = 0;

		SDL_GPUBufferRegion dst = {};
		dst.buffer = ib->GetGPUBuffer();
		dst.offset = (Uint32)offset;
		dst.size = (Uint32)size;

		SDL_UploadToGPUBuffer(copyPass, &src, &dst, false);
		EndCopyPass(copyPass);

		SDL_ReleaseGPUTransferBuffer(_device, transferBuffer);
	}

	void SDLGPUGraphicsDevice::BindIndexBuffer(IIndexBuffer* indexBuffer)
	{
		_currentIndexBuffer = static_cast<SDLGPUIndexBuffer*>(indexBuffer);
	}

	// -----------------------------------------------------------------------
	// Render Surface / Render Target / Depth Target
	// -----------------------------------------------------------------------

	std::unique_ptr<IRenderSurface2D> SDLGPUGraphicsDevice::CreateRenderSurface2D(
		int width, int height, SurfaceFormat colorFormat, bool isTypeless, DepthFormat depthFormat)
	{
		auto sdlColorFmt = GetSDLGPUTextureFormat(colorFormat);
		// Use device-validated depth format (D24_S8 may not be supported).
		auto sdlDepthFmt = (depthFormat == DepthFormat::Depth32)
			? SDL_GPU_TEXTUREFORMAT_D32_FLOAT : _depthStencilFormat;

		auto rt = std::make_unique<SDLGPURenderTarget2D>(_device, width, height, sdlColorFmt, isTypeless);

		std::unique_ptr<SDLGPUDepthTarget> depth = nullptr;
		if (depthFormat != DepthFormat::None)
			depth = std::make_unique<SDLGPUDepthTarget>(_device, width, height, sdlDepthFmt);

		return std::make_unique<IRenderSurface2D>(std::move(rt), std::move(depth));
	}

	std::unique_ptr<IRenderSurface2D> SDLGPUGraphicsDevice::CreateRenderSurface2D(
		int width, int height, int arraySize, SurfaceFormat colorFormat, DepthFormat depthFormat)
	{
		auto sdlColorFmt = GetSDLGPUTextureFormat(colorFormat);
		auto sdlDepthFmt = (depthFormat == DepthFormat::Depth32)
			? SDL_GPU_TEXTUREFORMAT_D32_FLOAT : _depthStencilFormat;

		auto rt = std::make_unique<SDLGPURenderTarget2D>(_device, width, height, arraySize, sdlColorFmt);

		std::unique_ptr<SDLGPUDepthTarget> depth = nullptr;
		if (depthFormat != DepthFormat::None)
			depth = std::make_unique<SDLGPUDepthTarget>(_device, width, height, arraySize, sdlDepthFmt);

		return std::make_unique<IRenderSurface2D>(std::move(rt), std::move(depth));
	}

	std::unique_ptr<IRenderSurface2D> SDLGPUGraphicsDevice::CreateRenderSurface2D(
		IRenderSurface2D* parentSurface, SurfaceFormat colorFormat)
	{
		auto* parentRT = static_cast<SDLGPURenderTarget2D*>(parentSurface->GetRenderTarget());
		auto sdlFmt = GetSDLGPUTextureFormat(colorFormat);

		auto rt = std::make_unique<SDLGPURenderTarget2D>(_device, parentRT, sdlFmt);

		// Share parent depth target (no new depth).
		return std::make_unique<IRenderSurface2D>(std::move(rt), nullptr);
	}

	IRenderTargetCube* SDLGPUGraphicsDevice::CreateRenderTargetCube(int size, SurfaceFormat colorFormat)
	{
		// Not implemented — same as GL backend. Point light shadow maps use 2D arrays instead.
		return nullptr;
	}

	// -----------------------------------------------------------------------
	// Texture2D
	// -----------------------------------------------------------------------

	std::unique_ptr<ITexture2D> SDLGPUGraphicsDevice::CreateTexture2D(int width, int height, SurfaceFormat format, void* data, bool isDynamic)
	{
		auto sdlFmt = GetSDLGPUTextureFormat(format);
		return std::make_unique<SDLGPUTexture2D>(_device, width, height, sdlFmt, data);
	}

	std::unique_ptr<ITexture2D> SDLGPUGraphicsDevice::CreateTexture2DFromFile(const std::string fileName)
	{
		return std::make_unique<SDLGPUTexture2D>(_device, fileName);
	}

	std::unique_ptr<ITexture2D> SDLGPUGraphicsDevice::CreateTexture2DFromFileInMemory(int dataSize, unsigned char* data)
	{
		return std::make_unique<SDLGPUTexture2D>(_device, dataSize, data);
	}

	void SDLGPUGraphicsDevice::UpdateTexture2D(ITexture2D* texture, std::vector<char> data)
	{
		auto* tex = static_cast<SDLGPUTexture2D*>(texture);
		UploadToTexture(tex->GetGPUTexture(), tex->GetWidth(), tex->GetHeight(), tex->GetFormat(), data.data());
	}

	// -----------------------------------------------------------------------
	// State management
	// -----------------------------------------------------------------------

	void SDLGPUGraphicsDevice::SetBlendMode(BlendMode blendMode)
	{
		_currentPipelineState.Blend = blendMode;
	}

	void SDLGPUGraphicsDevice::SetDepthState(DepthState depthState)
	{
		_currentPipelineState.Depth = depthState;
	}

	void SDLGPUGraphicsDevice::SetCullMode(CullMode cullMode)
	{
		_currentPipelineState.Cull = cullMode;
	}

	void SDLGPUGraphicsDevice::SetScissor(RendererRectangle rectangle)
	{
		if (_renderPass)
		{
			SDL_Rect scissor;
			scissor.x = rectangle.Left;
			scissor.y = rectangle.Top;
			scissor.w = rectangle.Right - rectangle.Left;
			scissor.h = rectangle.Bottom - rectangle.Top;
			SDL_SetGPUScissor(_renderPass, &scissor);
		}
	}

	void SDLGPUGraphicsDevice::SetScissor(RendererViewport viewport)
	{
		if (_renderPass)
		{
			SDL_Rect scissor;
			scissor.x = viewport.X;
			scissor.y = viewport.Y;
			scissor.w = viewport.Width;
			scissor.h = viewport.Height;
			SDL_SetGPUScissor(_renderPass, &scissor);
		}
	}

	void SDLGPUGraphicsDevice::SetViewport(RendererViewport viewport)
	{
		if (_renderPass)
		{
			SDL_GPUViewport vp = {};
			vp.x = (float)viewport.X;
			vp.y = (float)viewport.Y;
			vp.w = (float)viewport.Width;
			vp.h = (float)viewport.Height;
			vp.min_depth = viewport.MinDepth;
			vp.max_depth = viewport.MaxDepth;
			SDL_SetGPUViewport(_renderPass, &vp);
		}
	}

	void SDLGPUGraphicsDevice::SetPrimitiveType(PrimitiveType primitiveType)
	{
		_currentPrimitiveType = primitiveType;
	}

	// -----------------------------------------------------------------------
	// Texture binding
	// -----------------------------------------------------------------------

	void SDLGPUGraphicsDevice::BindTexture(TextureRegister registerType, ITextureBase* texture, SamplerStateRegister samplerType)
	{
		if (!texture)
		{
			_boundTextures.erase(static_cast<int>(registerType));
			return;
		}

		BoundTexture bt;

		if (auto* tex = dynamic_cast<SDLGPUTexture2D*>(texture))
		{
			bt.Texture = tex->GetGPUTexture();
		} 
		else if (auto* rt = dynamic_cast<SDLGPURenderTarget2D*>(texture))
		{
			bt.Texture = rt->GetGPUTexture();
		} 

		bt.Sampler = GetSampler(samplerType);
		_boundTextures[static_cast<int>(registerType)] = bt;
	}

	// -----------------------------------------------------------------------
	// Constant Buffer
	// -----------------------------------------------------------------------

	std::unique_ptr<IConstantBuffer> SDLGPUGraphicsDevice::CreateConstantBuffer(int size, std::string name)
	{
		return std::make_unique<SDLGPUConstantBuffer>(size, std::move(name));
	}

	void SDLGPUGraphicsDevice::UpdateConstantBuffer(IConstantBuffer* constantBuffer, void* data)
	{
		auto* cb = static_cast<SDLGPUConstantBuffer*>(constantBuffer);
		cb->UpdateData(data);
		// Reset push size to full capacity. Callers that need a smaller push
		// should use Renderer::UpdateConstantBuffer(data, cb, usedSize).
		cb->SetPushSize(cb->GetCapacity());
	}

	void SDLGPUGraphicsDevice::BindConstantBuffer(ShaderStage shaderStage, ConstantBufferRegister constantBufferType, IConstantBuffer* buffer)
	{
		auto* cb = static_cast<SDLGPUConstantBuffer*>(buffer);
		int slot = static_cast<int>(constantBufferType);

		BoundCB bound;
		bound.Buffer = cb;

		if (shaderStage == ShaderStage::VertexShader)
			_boundVertexCBs[slot] = bound;
		else if (shaderStage == ShaderStage::PixelShader)
			_boundFragmentCBs[slot] = bound;
	}

	// -----------------------------------------------------------------------
	// Input Layout
	// -----------------------------------------------------------------------

	void SDLGPUGraphicsDevice::SetInputLayout(IInputLayout* inputLayout)
	{
		_currentInputLayout = static_cast<SDLGPUInputLayout*>(inputLayout);
	}

	std::unique_ptr<IInputLayout> SDLGPUGraphicsDevice::CreateInputLayout(std::vector<RendererInputLayoutField> fields, IShader* shader)
	{
		return std::make_unique<SDLGPUInputLayout>(std::move(fields));
	}

	// -----------------------------------------------------------------------
	// Shader
	// -----------------------------------------------------------------------

	std::unique_ptr<IShader> SDLGPUGraphicsDevice::CreateShader(ShaderCompileRequest& request)
	{
		auto nativeShader = std::make_unique<SDLGPUShader>();
		nativeShader->SetDevice(_device);

		// Build HLSL source path: insert "HLSL/" after "Shaders/".
		auto hlslSourceDir = request.SourceDirectory;
		auto pos = hlslSourceDir.rfind("Shaders/");
		if (pos != std::string::npos)
			hlslSourceDir.insert(pos + 8, "HLSL/");

		auto hlslFile = hlslSourceDir + request.FileName + ".hlsl";
		TENLog("Loading HLSL: " + hlslFile, LogLevel::Info);

		// Read HLSL source.
		if (!std::filesystem::exists(hlslFile))
		{
			TENLog("HLSL file not found: " + hlslFile, LogLevel::Error);
			return nativeShader;
		}

		std::ifstream ifs(hlslFile, std::ios::binary);
		std::string hlslSource((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

		auto entryPoint = request.EntryPoint;

		// Check if we're running on Vulkan (native SPIRV) vs D3D12/Metal (needs cross-compilation).
		// Can't use SDL_GetGPUShaderFormats because SDL_ShaderCross makes all formats appear supported.
		auto* driverName2 = SDL_GetGPUDeviceDriver(_device);
		bool isNativeSPIRV = driverName2 && std::string(driverName2) == "vulkan";

		// Lambda: compile one shader stage.
		// Compiles HLSL -> SPIRV, remaps descriptor sets, builds binding maps,
		// and creates the GPU shader (SPIRV for Vulkan, DXIL/MSL via ShaderCross for others).
		struct CompileResult { SDL_GPUShader* shader; SPIRVRemapResult remap; };

		auto compileStage = [&](const std::string& stagePrefix, SDL_ShaderCross_ShaderStage stage)
			-> CompileResult
		{
			CompileResult cr = {};
			std::string entry = stagePrefix + entryPoint;

			// Build per-stage defines: add VERTEX_SHADER for VS.
			struct DefineStorage { std::string name; std::string value; };
			std::vector<DefineStorage> defineStrings;
			for (auto& [key, value] : request.Macros)
				defineStrings.push_back({ key, value });
			if (stage == SDL_SHADERCROSS_SHADERSTAGE_VERTEX)
				defineStrings.push_back({ "VERTEX_SHADER", "1" });

			std::vector<SDL_ShaderCross_HLSL_Define> defines;
			for (auto& ds : defineStrings)
			{
				SDL_ShaderCross_HLSL_Define def;
				def.name = const_cast<char*>(ds.name.c_str());
				def.value = const_cast<char*>(ds.value.c_str());
				defines.push_back(def);
			}
			defines.push_back({ nullptr, nullptr }); // Null sentinel.

			SDL_ShaderCross_HLSL_Info hlslInfo = {};
			hlslInfo.source = hlslSource.c_str();
			hlslInfo.entrypoint = entry.c_str();
			hlslInfo.shader_stage = stage;
			hlslInfo.defines = defines.data();
			hlslInfo.include_dir = hlslSourceDir.c_str();
			hlslInfo.props = 0;

			// Compile HLSL -> SPIRV.
			size_t spirvSize = 0;
			void* spirvData = SDL_ShaderCross_CompileSPIRVFromHLSL(&hlslInfo, &spirvSize);

			if (!spirvData)
			{
				TENLog("Failed to compile HLSL to SPIRV: " + entry + " in " + hlslFile
					+ " -- " + SDL_GetError(), LogLevel::Error);
				return cr;
			}

			// Verify SPIRV magic number.
			if (spirvSize >= 4)
			{
				uint32_t magic = *(const uint32_t*)spirvData;
				if (magic != 0x07230203)
					TENLog("WARNING: SPIRV magic mismatch for " + entry + ": 0x"
						+ std::to_string(magic), LogLevel::Warning);
			}

			// Remap SPIRV descriptor sets and bindings for SDL_GPU layout.
			// This also gives us the old->new binding maps and resource counts.
			cr.remap = RemapSPIRVDescriptorSets(spirvData, spirvSize, stage);
			if (!cr.remap.success)
				TENLog("WARNING: SPIRV binding remap failed for " + entry, LogLevel::Warning);

			if (cr.remap.numUniformBuffers > 4)
			{
				TENLog("ERROR: Shader " + entry + " declares " + std::to_string(cr.remap.numUniformBuffers)
					+ " UBOs, exceeding SDL_GPU's 4-slot limit!", LogLevel::Error);
			}

			TENLog("Shader " + entry + ": SPIRV=" + std::to_string(spirvSize) + "B"
				+ " smp=" + std::to_string(cr.remap.numSamplerSlots)
				+ " sTex=" + std::to_string(cr.remap.numStorageTextures)
				+ " sBuf=" + std::to_string(cr.remap.numStorageBuffers)
				+ " UBO=" + std::to_string(cr.remap.numUniformBuffers),
				LogLevel::Info);

			// Set resource counts on the shader object.
			if (stage == SDL_SHADERCROSS_SHADERSTAGE_VERTEX)
			{
				nativeShader->SetVertexResourceCounts(
					cr.remap.numSamplerSlots,
					cr.remap.numUniformBuffers,
					cr.remap.numStorageBuffers);
			}
			else if (stage == SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT)
			{
				nativeShader->SetFragmentResourceCounts(
					cr.remap.numSamplerSlots,
					cr.remap.numUniformBuffers,
					cr.remap.numStorageBuffers);
			}

			// Create GPU shader.
			if (isNativeSPIRV)
			{
				// Vulkan: use our remapped SPIRV directly.
				SDL_GPUShaderCreateInfo shaderCI = {};
				shaderCI.entrypoint = entry.c_str();
				shaderCI.stage = (stage == SDL_SHADERCROSS_SHADERSTAGE_VERTEX)
					? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
				shaderCI.num_samplers = cr.remap.numSamplerSlots;
				shaderCI.num_storage_textures = cr.remap.numStorageTextures;
				shaderCI.num_storage_buffers = cr.remap.numStorageBuffers;
				shaderCI.num_uniform_buffers = cr.remap.numUniformBuffers;
				shaderCI.code = (const Uint8*)spirvData;
				shaderCI.code_size = spirvSize;
				shaderCI.format = SDL_GPU_SHADERFORMAT_SPIRV;

				cr.shader = SDL_CreateGPUShader(_device, &shaderCI);
			}
			else
			{
				// D3D12/Metal: use SDL_ShaderCross to cross-compile from SPIRV.
				SDL_ShaderCross_SPIRV_Info sci = {};
				sci.bytecode = (const Uint8*)spirvData;
				sci.bytecode_size = spirvSize;
				sci.entrypoint = entry.c_str();
				sci.shader_stage = stage;
				sci.props = 0;

				SDL_ShaderCross_GraphicsShaderResourceInfo resInfo = {};
				resInfo.num_samplers = cr.remap.numSamplerSlots;
				resInfo.num_storage_textures = cr.remap.numStorageTextures;
				resInfo.num_storage_buffers = cr.remap.numStorageBuffers;
				resInfo.num_uniform_buffers = cr.remap.numUniformBuffers;

				cr.shader = SDL_ShaderCross_CompileGraphicsShaderFromSPIRV(_device, &sci, &resInfo, 0);
			}

			if (!cr.shader)
			{
				TENLog("Failed to create GPU shader: " + entry + " in " + hlslFile
					+ " -- " + SDL_GetError(), LogLevel::Error);
			}
			else
			{
				TENLog("GPU shader OK: " + entry + (isNativeSPIRV ? " (Vulkan SPIRV)" : " (cross-compiled)"), LogLevel::Info);
			}

			SDL_free(spirvData);
			return cr;
		};

		// Compile vertex shader.
		if (request.Type == ShaderType::Vertex || request.Type == ShaderType::PixelAndVertex)
		{
			auto cr = compileStage("VS", SDL_SHADERCROSS_SHADERSTAGE_VERTEX);
			nativeShader->SetVertexShader(cr.shader);

			// UBO mapping: HLSL register -> SPIRV binding (from remap).
			// Since shaders now use b0-b3 directly, the HLSL register IS the engine slot.
			nativeShader->SetVertexUBOMapping(cr.remap.uboOldToNewBinding);
			nativeShader->SetVertexArrayedBindings(cr.remap.arrayedSamplerBindings);

			{
				std::string mapLog = "VS UBO map:";
				for (auto& [hlslReg, spirvBind] : cr.remap.uboOldToNewBinding)
					mapLog += " b" + std::to_string(hlslReg) + "->" + std::to_string(spirvBind);
				TENLog(mapLog, LogLevel::Info);
			}
		}

		// Compile pixel/fragment shader.
		if (request.Type == ShaderType::Pixel || request.Type == ShaderType::PixelAndVertex)
		{
			auto cr = compileStage("PS", SDL_SHADERCROSS_SHADERSTAGE_FRAGMENT);
			nativeShader->SetFragmentShader(cr.shader);

			// Sampler mapping: HLSL register -> SPIRV binding.
			// UBO mapping: HLSL register -> SPIRV binding.
			nativeShader->SetFragmentSamplerMapping(cr.remap.samplerOldToNewBinding, cr.remap.arrayedSamplerBindings);
			nativeShader->SetFragmentUBOMapping(cr.remap.uboOldToNewBinding);

			{
				std::string mapLog = "FS sampler map:";
				for (auto& [hlslReg, spirvBind] : cr.remap.samplerOldToNewBinding)
					mapLog += " t" + std::to_string(hlslReg) + "->" + std::to_string(spirvBind);
				mapLog += " arrayed:";
				for (auto b : cr.remap.arrayedSamplerBindings)
					mapLog += " " + std::to_string(b);
				mapLog += " | FS UBO map:";
				for (auto& [hlslReg, spirvBind] : cr.remap.uboOldToNewBinding)
					mapLog += " b" + std::to_string(hlslReg) + "->" + std::to_string(spirvBind);
				TENLog(mapLog, LogLevel::Info);
			}
		}

		// Note: SDL_GPU does not support geometry shaders.

		return nativeShader;
	}

	void SDLGPUGraphicsDevice::BindShader(ShaderStage shaderStage, IShader* shader, bool forceNull)
	{
		auto* gpuShader = forceNull ? nullptr : static_cast<SDLGPUShader*>(shader);

		if (shaderStage == ShaderStage::VertexShader)
		{
			// Only update if nulling out or shader actually has a vertex stage.
			// This prevents a PS-only shader from overwriting _currentVSShader
			// when ShaderManager::Bind() broadcasts to all stages.
			if (!gpuShader || gpuShader->GetVertexShader())
				_currentVSShader = gpuShader;
		}
		else if (shaderStage == ShaderStage::PixelShader)
		{
			// Only update if nulling out or shader actually has a fragment stage.
			if (!gpuShader || gpuShader->GetFragmentShader())
				_currentPSShader = gpuShader;
		}
		// Geometry shaders not supported by SDL_GPU.
	}

	// -----------------------------------------------------------------------
	// Draw calls
	// -----------------------------------------------------------------------

	void SDLGPUGraphicsDevice::BindTexturesForDraw()
	{
		if (!_renderPass)
			return;

		// Bind fragment stage samplers: build a properly-sized array matching
		// the shader's declared sampler count.
		// Uses the HLSL-register → SPIRV-binding map to place textures at the
		// correct slot, and picks the right dummy texture type (2D vs 2D_ARRAY).
		if (_currentPSShader)
		{
			unsigned int numFS = _currentPSShader->GetNumFragmentSamplers();
			if (numFS > 0)
			{
				std::vector<SDL_GPUTextureSamplerBinding> fsBindings(numFS);
				for (unsigned int i = 0; i < numFS; ++i)
				{
					// Choose dummy type based on whether this binding expects an array texture.
					fsBindings[i].texture = _currentPSShader->IsArrayedSamplerBinding((int)i)
						? _dummyTexture2DArray : _dummyTexture2D;
					fsBindings[i].sampler = _dummySampler;
				}
				for (auto& [engineSlot, bt] : _boundTextures)
				{
					if (!bt.Texture || !bt.Sampler)
						continue;
					// Translate engine register number to SPIRV binding index.
					int spirvSlot = _currentPSShader->MapFragmentSamplerSlot(engineSlot);
					if (spirvSlot >= 0 && spirvSlot < (int)numFS)
					{
						fsBindings[spirvSlot].texture = bt.Texture;
						fsBindings[spirvSlot].sampler = bt.Sampler;
					}
				}
				SDL_BindGPUFragmentSamplers(_renderPass, 0, fsBindings.data(), numFS);
			}
		}

		// Bind vertex stage samplers (rare, but some shaders may use them).
		if (_currentVSShader)
		{
			unsigned int numVS = _currentVSShader->GetNumVertexSamplers();
			if (numVS > 0)
			{
				std::vector<SDL_GPUTextureSamplerBinding> vsBindings(numVS);
				for (unsigned int i = 0; i < numVS; ++i)
				{
					vsBindings[i].texture = _currentVSShader->IsVSArrayedSamplerBinding((int)i)
						? _dummyTexture2DArray : _dummyTexture2D;
					vsBindings[i].sampler = _dummySampler;
				}
				SDL_BindGPUVertexSamplers(_renderPass, 0, vsBindings.data(), numVS);
			}
		}
	}

	void SDLGPUGraphicsDevice::PushUniformsForDraw()
	{
		if (!_renderPass)
			return;

		// SDL_GPU requires data pushed to ALL uniform buffer slots declared by the shader
		// before every draw call. Since shaders use fixed b0-b3 registers, the engine's
		// ConstantBufferRegister maps directly to HLSL register N, and the SPIRV remap
		// provides the HLSL register -> SPIRV binding map.
		static const char s_zeroBuf[2048] = {};

		// Push uniforms for a single stage (vertex or fragment).
		auto pushStage = [&](
			SDLGPUShader* shader,
			const std::map<int, BoundCB>& boundCBs,
			bool isVertex)
		{
			if (!shader)
				return;

			unsigned int numExpected = isVertex
				? shader->GetNumVertexUBOs()
				: shader->GetNumFragmentUBOs();
			if (numExpected == 0)
				return;

			auto pushFn = isVertex
				? SDL_PushGPUVertexUniformData
				: SDL_PushGPUFragmentUniformData;

			std::vector<bool> pushed(numExpected, false);

			// Push each bound CB to its corresponding SPIRV slot.
			for (auto& [engineSlot, bound] : boundCBs)
			{
				int spirvSlot = isVertex
					? shader->MapVertexUBOSlot(engineSlot)
					: shader->MapFragmentUBOSlot(engineSlot);
				if (spirvSlot < 0 || spirvSlot >= (int)numExpected || pushed[spirvSlot])
					continue;

				if (bound.Buffer)
					pushFn(_commandBuffer, spirvSlot, bound.Buffer->GetData(), bound.Buffer->GetSize());
				else
					pushFn(_commandBuffer, spirvSlot, s_zeroBuf, 16);

				pushed[spirvSlot] = true;
			}

			// Fill any remaining slots with zeros.
			for (unsigned int i = 0; i < numExpected; ++i)
			{
				if (!pushed[i])
					pushFn(_commandBuffer, i, s_zeroBuf, 16);
			}
		};

		pushStage(_currentVSShader, _boundVertexCBs, true);
		pushStage(_currentPSShader, _boundFragmentCBs, false);
	}

	SDL_GPUGraphicsPipeline* SDLGPUGraphicsDevice::GetCurrentPipeline()
	{
		if (!_currentVSShader || !_currentPSShader)
			return nullptr;

		PipelineCacheKey key;
		key.VertexShader = _currentVSShader->GetVertexShader();
		key.FragmentShader = _currentPSShader->GetFragmentShader();
		key.InputLayout = _currentInputLayout;
		key.PipelineState = _currentPipelineState;
		key.Primitive = _currentPrimitiveType;
		key.NumColorTargets = _numCurrentColorTargets;
		key.HasDepthTarget = _hasCurrentDepthTarget;
		key.DepthFormat = _currentDepthFormat;
		for (int i = 0; i < _numCurrentColorTargets; ++i)
			key.ColorFormats[i] = _currentColorFormats[i];

		return _pipelineCache.GetOrCreatePipeline(key);
	}

	// -----------------------------------------------------------------------
	// Storage buffer binding for draw calls
	// -----------------------------------------------------------------------

	void SDLGPUGraphicsDevice::BindStorageBuffersForDraw()
	{
		if (!_renderPass)
			return;

		// Bind vertex stage storage buffers.
		if (!_boundVertexStorageBuffers.empty())
		{
			std::vector<SDL_GPUBuffer*> buffers;
			for (auto& [slot, bound] : _boundVertexStorageBuffers)
			{
				if (bound.Buffer && bound.Buffer->GetGPUBuffer())
					buffers.push_back(bound.Buffer->GetGPUBuffer());
			}
			if (!buffers.empty())
				SDL_BindGPUVertexStorageBuffers(_renderPass, 0, buffers.data(), (Uint32)buffers.size());
		}

		// Bind fragment stage storage buffers.
		if (!_boundFragmentStorageBuffers.empty())
		{
			std::vector<SDL_GPUBuffer*> buffers;
			for (auto& [slot, bound] : _boundFragmentStorageBuffers)
			{
				if (bound.Buffer && bound.Buffer->GetGPUBuffer())
					buffers.push_back(bound.Buffer->GetGPUBuffer());
			}
			if (!buffers.empty())
				SDL_BindGPUFragmentStorageBuffers(_renderPass, 0, buffers.data(), (Uint32)buffers.size());
		}
	}

	// -----------------------------------------------------------------------
	// Storage buffer API
	// -----------------------------------------------------------------------

	std::unique_ptr<SDLGPUStorageBuffer> SDLGPUGraphicsDevice::CreateStorageBuffer(int capacity)
	{
		return std::make_unique<SDLGPUStorageBuffer>(_device, capacity);
	}

	void SDLGPUGraphicsDevice::BindStorageBuffer(ShaderStage stage, int slot, SDLGPUStorageBuffer* buffer)
	{
		if (stage == ShaderStage::VertexShader)
			_boundVertexStorageBuffers[slot] = { buffer };
		else if (stage == ShaderStage::PixelShader)
			_boundFragmentStorageBuffers[slot] = { buffer };
	}

	void SDLGPUGraphicsDevice::DrawIndexedTriangles(int count, int baseIndex, int baseVertex)
	{
		if (!_renderPass || !_currentVertexBuffer || !_currentIndexBuffer)
			return;

		auto* pipeline = GetCurrentPipeline();
		if (!pipeline)
			return;

		SDL_BindGPUGraphicsPipeline(_renderPass, pipeline);

		SDL_GPUBufferBinding vbBinding = {};
		vbBinding.buffer = _currentVertexBuffer->GetGPUBuffer();
		vbBinding.offset = 0;
		SDL_BindGPUVertexBuffers(_renderPass, 0, &vbBinding, 1);

		SDL_GPUBufferBinding ibBinding = {};
		ibBinding.buffer = _currentIndexBuffer->GetGPUBuffer();
		ibBinding.offset = 0;
		SDL_BindGPUIndexBuffer(_renderPass, &ibBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

		BindTexturesForDraw();
		BindStorageBuffersForDraw();
		PushUniformsForDraw();

		SDL_DrawGPUIndexedPrimitives(_renderPass, count, 1, baseIndex, baseVertex, 0);
	}

	void SDLGPUGraphicsDevice::DrawIndexedInstancedTriangles(int count, int instances, int baseIndex, int baseVertex)
	{
		if (!_renderPass || !_currentVertexBuffer || !_currentIndexBuffer)
			return;

		auto* pipeline = GetCurrentPipeline();
		if (!pipeline)
			return;

		SDL_BindGPUGraphicsPipeline(_renderPass, pipeline);

		SDL_GPUBufferBinding vbBinding = {};
		vbBinding.buffer = _currentVertexBuffer->GetGPUBuffer();
		vbBinding.offset = 0;
		SDL_BindGPUVertexBuffers(_renderPass, 0, &vbBinding, 1);

		SDL_GPUBufferBinding ibBinding = {};
		ibBinding.buffer = _currentIndexBuffer->GetGPUBuffer();
		ibBinding.offset = 0;
		SDL_BindGPUIndexBuffer(_renderPass, &ibBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);

		BindTexturesForDraw();
		BindStorageBuffersForDraw();
		PushUniformsForDraw();

		SDL_DrawGPUIndexedPrimitives(_renderPass, count, instances, baseIndex, baseVertex, 0);
	}

	void SDLGPUGraphicsDevice::DrawInstancedTriangles(int count, int instances, int baseVertex)
	{
		if (!_renderPass || !_currentVertexBuffer)
			return;

		auto* pipeline = GetCurrentPipeline();
		if (!pipeline)
			return;

		SDL_BindGPUGraphicsPipeline(_renderPass, pipeline);

		SDL_GPUBufferBinding vbBinding = {};
		vbBinding.buffer = _currentVertexBuffer->GetGPUBuffer();
		vbBinding.offset = 0;
		SDL_BindGPUVertexBuffers(_renderPass, 0, &vbBinding, 1);

		BindTexturesForDraw();
		BindStorageBuffersForDraw();
		PushUniformsForDraw();

		SDL_DrawGPUPrimitives(_renderPass, count, instances, baseVertex, 0);
	}

	void SDLGPUGraphicsDevice::DrawTriangles(int count, int baseVertex)
	{
		if (!_renderPass || !_currentVertexBuffer)
			return;

		auto* pipeline = GetCurrentPipeline();
		if (!pipeline)
			return;

		SDL_BindGPUGraphicsPipeline(_renderPass, pipeline);

		SDL_GPUBufferBinding vbBinding = {};
		vbBinding.buffer = _currentVertexBuffer->GetGPUBuffer();
		vbBinding.offset = 0;
		SDL_BindGPUVertexBuffers(_renderPass, 0, &vbBinding, 1);

		BindTexturesForDraw();
		BindStorageBuffersForDraw();
		PushUniformsForDraw();

		SDL_DrawGPUPrimitives(_renderPass, count, 1, baseVertex, 0);
	}

	// -----------------------------------------------------------------------
	// Clear operations
	// -----------------------------------------------------------------------

	void SDLGPUGraphicsDevice::ClearRenderTarget2D(IRenderTarget2D* renderTarget, Vector4 clearColor)
	{
		// Clearing is handled by render pass LoadAction::Clear.
		// Standalone clears outside render passes would need a dedicated pass.
	}

	void SDLGPUGraphicsDevice::ClearRenderTarget2D(IRenderTarget2D* renderTarget, int arrayIndex, Vector4 clearColor)
	{
		// Handled by render pass LoadAction::Clear with array index.
	}

	void SDLGPUGraphicsDevice::ClearDepthStencil(IDepthTarget* depthTarget, DepthStencilClearFlags clearFlags, float depth, unsigned char stencil)
	{
		// Handled by render pass LoadAction::Clear.
	}

	void SDLGPUGraphicsDevice::ClearDepthStencil(IDepthTarget* depthTarget, int arrayIndex, DepthStencilClearFlags clearFlags, float depth, unsigned char stencil)
	{
		// Handled by render pass LoadAction::Clear.
	}

	// -----------------------------------------------------------------------
	// Render target binding (legacy API — forwarded to render pass)
	// -----------------------------------------------------------------------

	void SDLGPUGraphicsDevice::BindRenderTarget(IRenderTarget2D* renderTarget, IDepthTarget* depthTarget)
	{
		// Legacy path; in SDL_GPU, render targets are bound via BeginRenderPass.
		// Store state for PSO cache key.
		auto* rt = static_cast<SDLGPURenderTarget2D*>(renderTarget);
		_numCurrentColorTargets = 1;
		_currentColorFormats[0] = rt ? rt->GetFormat() : _swapchainFormat;

		if (depthTarget)
		{
			auto* dt = static_cast<SDLGPUDepthTarget*>(depthTarget);
			_currentDepthFormat = dt->GetFormat();
			_hasCurrentDepthTarget = true;
		}
		else
		{
			_currentDepthFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
			_hasCurrentDepthTarget = false;
		}
	}

	void SDLGPUGraphicsDevice::BindRenderTarget(IRenderTargetBinding renderTarget, IDepthTargetBinding depthTarget)
	{
		BindRenderTarget(renderTarget.RenderTarget, depthTarget.DepthTarget);
	}

	void SDLGPUGraphicsDevice::BindRenderTargets(std::vector<IRenderTarget2D*> renderTargets, IDepthTarget* depthTarget)
	{
		_numCurrentColorTargets = (int)renderTargets.size();
		for (int i = 0; i < _numCurrentColorTargets && i < 4; ++i)
		{
			auto* rt = static_cast<SDLGPURenderTarget2D*>(renderTargets[i]);
			_currentColorFormats[i] = rt ? rt->GetFormat() : _swapchainFormat;
		}

		if (depthTarget)
		{
			auto* dt = static_cast<SDLGPUDepthTarget*>(depthTarget);
			_currentDepthFormat = dt->GetFormat();
			_hasCurrentDepthTarget = true;
		}
		else
		{
			_currentDepthFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
			_hasCurrentDepthTarget = false;
		}
	}

	void SDLGPUGraphicsDevice::BindRenderTargets(std::vector<IRenderTargetBinding> renderTargets, IDepthTargetBinding depthTarget)
	{
		std::vector<IRenderTarget2D*> rts;
		for (auto& rt : renderTargets)
			rts.push_back(rt.RenderTarget);
		BindRenderTargets(rts, depthTarget.DepthTarget);
	}

	// -----------------------------------------------------------------------
	// Render Pass API
	// -----------------------------------------------------------------------

	void SDLGPUGraphicsDevice::BeginRenderPass(const RenderPassDescriptor& desc)
	{
		EnsureCommandBuffer();

		// End previous render pass if still active.
		if (_renderPass)
		{
			SDL_EndGPURenderPass(_renderPass);
			_renderPass = nullptr;
		}

		_savedRenderPassDesc = desc;

		// Build color target infos.
		std::vector<SDL_GPUColorTargetInfo> colorInfos;
		for (auto& ca : desc.ColorAttachments)
		{
			if (!ca.RenderTarget) continue;

			auto* rt = static_cast<SDLGPURenderTarget2D*>(ca.RenderTarget);
			SDL_GPUColorTargetInfo info = {};
			info.texture = rt->GetGPUTexture();

			// Validate texture.
			if (!info.texture)
			{
				TENLog("BeginRenderPass '" + desc.Name + "': NULL color texture at index " + std::to_string(colorInfos.size()), LogLevel::Error);
				continue;
			}
			info.layer_or_depth_plane = ca.ArrayIndex;
			info.clear_color.r = ca.ClearColor.x;
			info.clear_color.g = ca.ClearColor.y;
			info.clear_color.b = ca.ClearColor.z;
			info.clear_color.a = ca.ClearColor.w;

			switch (ca.LoadAction)
			{
			case LoadAction::Clear:    info.load_op = SDL_GPU_LOADOP_CLEAR; break;
			case LoadAction::Load:     info.load_op = SDL_GPU_LOADOP_LOAD; break;
			case LoadAction::DontCare: info.load_op = SDL_GPU_LOADOP_DONT_CARE; break;
			}

			switch (ca.StoreAction)
			{
			case StoreAction::Store:    info.store_op = SDL_GPU_STOREOP_STORE; break;
			case StoreAction::DontCare: info.store_op = SDL_GPU_STOREOP_DONT_CARE; break;
			}

			colorInfos.push_back(info);
		}

		// Track formats for PSO cache.
		_numCurrentColorTargets = (int)colorInfos.size();
		for (int i = 0; i < _numCurrentColorTargets && i < 4; ++i)
		{
			auto* rt = static_cast<SDLGPURenderTarget2D*>(desc.ColorAttachments[i].RenderTarget);
			_currentColorFormats[i] = rt->GetFormat();
		}

		// Build depth target info.
		SDL_GPUDepthStencilTargetInfo depthInfo = {};
		bool hasDepth = (desc.DepthAttachment.DepthTarget != nullptr);

		if (hasDepth)
		{
			auto* dt = static_cast<SDLGPUDepthTarget*>(desc.DepthAttachment.DepthTarget);
			// Each layer is a separate 2D texture (SDL_GPU forbids depth array textures).
			depthInfo.texture = dt->GetGPUTexture(desc.DepthAttachment.ArrayIndex);
			if (!depthInfo.texture)
				TENLog("BeginRenderPass '" + desc.Name + "': NULL depth texture (arrayIdx=" + std::to_string(desc.DepthAttachment.ArrayIndex) + ")", LogLevel::Error);
			depthInfo.clear_depth = desc.DepthAttachment.ClearDepth;
			depthInfo.clear_stencil = desc.DepthAttachment.ClearStencil;

			switch (desc.DepthAttachment.LoadAction)
			{
			case LoadAction::Clear:    depthInfo.load_op = SDL_GPU_LOADOP_CLEAR; break;
			case LoadAction::Load:     depthInfo.load_op = SDL_GPU_LOADOP_LOAD; break;
			case LoadAction::DontCare: depthInfo.load_op = SDL_GPU_LOADOP_DONT_CARE; break;
			}

			switch (desc.DepthAttachment.StoreAction)
			{
			case StoreAction::Store:    depthInfo.store_op = SDL_GPU_STOREOP_STORE; break;
			case StoreAction::DontCare: depthInfo.store_op = SDL_GPU_STOREOP_DONT_CARE; break;
			}

			// Stencil ops mirror depth ops.
			depthInfo.stencil_load_op = depthInfo.load_op;
			depthInfo.stencil_store_op = depthInfo.store_op;

			_currentDepthFormat = dt->GetFormat();
			_hasCurrentDepthTarget = true;
		}
		else
		{
			_currentDepthFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
			_hasCurrentDepthTarget = false;
		}

		_renderPass = SDL_BeginGPURenderPass(
			_commandBuffer,
			colorInfos.empty() ? nullptr : colorInfos.data(),
			(Uint32)colorInfos.size(),
			hasDepth ? &depthInfo : nullptr);

		if (!_renderPass)
			TENLog("BeginRenderPass '" + desc.Name + "': SDL_BeginGPURenderPass FAILED — " + SDL_GetError(), LogLevel::Error);

		_inRenderPass = true;

		// Set viewport and scissor.
		SetViewport(desc.Viewport);
		SetScissor(desc.Viewport);
	}

	void SDLGPUGraphicsDevice::EndRenderPass()
	{
		if (_renderPass)
		{
			SDL_EndGPURenderPass(_renderPass);
			_renderPass = nullptr;
		}
		_inRenderPass = false;
	}

	// -----------------------------------------------------------------------
	// Pipeline state
	// -----------------------------------------------------------------------

	void SDLGPUGraphicsDevice::BindPipeline(const RenderPipelineState& state)
	{
		_currentPipelineState = state;
	}

	// -----------------------------------------------------------------------
	// Present
	// -----------------------------------------------------------------------

	void SDLGPUGraphicsDevice::Present()
	{
		EnsureCommandBuffer();

		// End any active render pass.
		if (_renderPass)
		{
			SDL_EndGPURenderPass(_renderPass);
			_renderPass = nullptr;
			_inRenderPass = false;
		}

		// Acquire swapchain texture.
		SDL_GPUTexture* swapchainTexture = nullptr;
		Uint32 swapW = 0, swapH = 0;
		if (!SDL_AcquireGPUSwapchainTexture(_commandBuffer, _window, &swapchainTexture, &swapW, &swapH))
		{
			// Swapchain not ready (e.g., minimized). Submit empty command buffer.
			SDL_SubmitGPUCommandBuffer(_commandBuffer);
			_commandBuffer = nullptr;
			return;
		}

		if (swapchainTexture && _backbufferTexture)
		{
			// Blit backbuffer to swapchain.
			SDL_GPUBlitInfo blitInfo = {};
			blitInfo.source.texture = _backbufferTexture;
			blitInfo.source.w = _screenWidth;
			blitInfo.source.h = _screenHeight;

			blitInfo.destination.texture = swapchainTexture;
			blitInfo.destination.w = swapW;
			blitInfo.destination.h = swapH;

			blitInfo.filter = SDL_GPU_FILTER_LINEAR;
			blitInfo.load_op = SDL_GPU_LOADOP_DONT_CARE;

			SDL_BlitGPUTexture(_commandBuffer, &blitInfo);
		}

		SDL_SubmitGPUCommandBuffer(_commandBuffer);
		_commandBuffer = nullptr;
	}

	// -----------------------------------------------------------------------
	// Misc
	// -----------------------------------------------------------------------

	void SDLGPUGraphicsDevice::ClearState()
	{
		_currentVertexBuffer = nullptr;
		_currentIndexBuffer = nullptr;
		_currentInputLayout = nullptr;
		_currentVSShader = nullptr;
		_currentPSShader = nullptr;
		_currentPipelineState = {};
		_boundVertexCBs.clear();
		_boundFragmentCBs.clear();
		_boundTextures.clear();
	}

	void SDLGPUGraphicsDevice::Flush()
	{
		if (_commandBuffer)
		{
			if (_renderPass)
			{
				SDL_EndGPURenderPass(_renderPass);
				_renderPass = nullptr;
				_inRenderPass = false;
			}
			SDL_SubmitGPUCommandBuffer(_commandBuffer);
			_commandBuffer = nullptr;
		}
	}

	void SDLGPUGraphicsDevice::UnbindAllRenderTargets()
	{
		if (_renderPass)
		{
			SDL_EndGPURenderPass(_renderPass);
			_renderPass = nullptr;
			_inRenderPass = false;
		}

		_numCurrentColorTargets = 0;
		_hasCurrentDepthTarget = false;
	}

	Vector3 SDLGPUGraphicsDevice::Unproject(Vector3 position, Matrix projection, Matrix view, Matrix world)
	{
		// Match DX11 Viewport::Unproject behavior: input is in pixel coordinates,
		// not [0,1] normalized. Convert pixels to NDC using screen dimensions.
		Viewport vp(0.0f, 0.0f, (float)_screenWidth, (float)_screenHeight, 0.0f, 1.0f);
		return vp.Unproject(position, projection, view, world);
	}

	void SDLGPUGraphicsDevice::SaveScreenshot(IRenderTarget2D* renderTarget, std::string path)
	{
		auto* rt = static_cast<SDLGPURenderTarget2D*>(renderTarget);
		if (!rt || !rt->GetGPUTexture())
			return;

		int w = rt->GetWidth();
		int h = rt->GetHeight();
		Uint32 bufSize = (Uint32)(w * h * 4);

		// Create a download transfer buffer.
		SDL_GPUTransferBufferCreateInfo tbci = {};
		tbci.usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
		tbci.size = bufSize;
		auto* transferBuffer = SDL_CreateGPUTransferBuffer(_device, &tbci);
		if (!transferBuffer)
			return;

		// Submit a copy pass to download the texture.
		auto* cmdBuf = SDL_AcquireGPUCommandBuffer(_device);
		auto* copyPass = SDL_BeginGPUCopyPass(cmdBuf);

		SDL_GPUTextureRegion src = {};
		src.texture = rt->GetGPUTexture();
		src.w = w;
		src.h = h;
		src.d = 1;

		SDL_GPUTextureTransferInfo dst = {};
		dst.transfer_buffer = transferBuffer;
		dst.offset = 0;

		SDL_DownloadFromGPUTexture(copyPass, &src, &dst);
		SDL_EndGPUCopyPass(copyPass);

		// Submit and wait for completion via fence.
		auto* fence = SDL_SubmitGPUCommandBufferAndAcquireFence(cmdBuf);
		SDL_WaitForGPUFences(_device, true, &fence, 1);
		SDL_ReleaseGPUFence(_device, fence);

		// Map transfer buffer and save as PNG.
		void* mapped = SDL_MapGPUTransferBuffer(_device, transferBuffer, false);
		if (mapped)
		{
			// SDL_GPU textures are top-down, no flip needed (unlike OpenGL).
			stbi_write_png(path.c_str(), w, h, 4, mapped, w * 4);
			SDL_UnmapGPUTransferBuffer(_device, transferBuffer);
		}

		SDL_ReleaseGPUTransferBuffer(_device, transferBuffer);
	}

	std::unique_ptr<ISpriteFont> SDLGPUGraphicsDevice::InitializeSpriteFont(std::string fontPath)
	{
		return std::make_unique<SDLGPUSpriteFont>(this, _device, fontPath);
	}

	std::unique_ptr<ISpriteBatch> SDLGPUGraphicsDevice::InitializeSpriteBatch()
	{
		return std::make_unique<SDLGPUSpriteBatch>(this);
	}

	std::unique_ptr<IPrimitiveBatch> SDLGPUGraphicsDevice::InitializePrimitiveBatch()
	{
		return std::make_unique<SDLGPUPrimitiveBatch>(this);
	}

	// -----------------------------------------------------------------------
	// Debug annotations
	// -----------------------------------------------------------------------

	void SDLGPUGraphicsDevice::BeginDebugEvent(const std::string& name)
	{
		if (_commandBuffer)
			SDL_PushGPUDebugGroup(_commandBuffer, name.c_str());
	}

	void SDLGPUGraphicsDevice::EndDebugEvent()
	{
		if (_commandBuffer)
			SDL_PopGPUDebugGroup(_commandBuffer);
	}
}

#endif
