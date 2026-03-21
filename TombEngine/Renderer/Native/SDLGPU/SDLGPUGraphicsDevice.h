#pragma once

#ifdef HAS_SDLGPU

#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>
#include <map>

#include "Renderer/Graphics/IGraphicsDevice.h"
#include "Renderer/Native/SDLGPU/SDLGPUVertexBuffer.h"
#include "Renderer/Native/SDLGPU/SDLGPUIndexBuffer.h"
#include "Renderer/Native/SDLGPU/SDLGPUConstantBuffer.h"
#include "Renderer/Native/SDLGPU/SDLGPUTexture2D.h"
#include "Renderer/Native/SDLGPU/SDLGPURenderTarget2D.h"
#include "Renderer/Native/SDLGPU/SDLGPUDepthTarget.h"
#include "Renderer/Native/SDLGPU/SDLGPUInputLayout.h"
#include "Renderer/Native/SDLGPU/SDLGPUShader.h"
#include "Renderer/Native/SDLGPU/SDLGPUPipelineCache.h"
#include "Renderer/Native/SDLGPU/SDLGPUStorageBuffer.h"
#include "Renderer/Native/SDLGPU/SDLGPUSpriteBatch.h"
#include "Renderer/Native/SDLGPU/SDLGPUPrimitiveBatch.h"
#include "Renderer/Native/SDLGPU/SDLGPUSpriteFont.h"

using namespace TEN::Renderer::Graphics;
using namespace TEN::Renderer::Structures;
using namespace TEN::Math::Library;

namespace TEN::Renderer::Native::SDLGPU
{
	class SDLGPUGraphicsDevice final : public IGraphicsDevice
	{
	private:
		SDL_GPUDevice*         _device        = nullptr;
		SDL_Window*            _window        = nullptr;
		SDL_GPUCommandBuffer*  _commandBuffer = nullptr;
		SDL_GPURenderPass*     _renderPass    = nullptr;

		// Pipeline cache for lazy PSO creation.
		SDLGPUPipelineCache _pipelineCache;

		// Sampler objects, keyed by SamplerStateRegister.
		std::map<SamplerStateRegister, SDL_GPUSampler*> _samplers;

		// Currently bound state for draw calls.
		SDLGPUVertexBuffer*   _currentVertexBuffer  = nullptr;
		SDLGPUIndexBuffer*    _currentIndexBuffer   = nullptr;
		SDLGPUInputLayout*    _currentInputLayout   = nullptr;
		SDLGPUShader*         _currentVSShader      = nullptr;
		SDLGPUShader*         _currentPSShader      = nullptr;
		RenderPipelineState   _currentPipelineState;
		PrimitiveType         _currentPrimitiveType = PrimitiveType::TriangleList;

		// Bound constant buffers per stage+slot.
		struct BoundCB
		{
			SDLGPUConstantBuffer* Buffer = nullptr;
		};
		std::map<int, BoundCB> _boundVertexCBs;   // Key = slot.
		std::map<int, BoundCB> _boundFragmentCBs;

		// Bound textures per register.
		struct BoundTexture
		{
			SDL_GPUTexture*  Texture = nullptr;
			SDL_GPUSampler*  Sampler = nullptr;
		};
		std::map<int, BoundTexture> _boundTextures;

		// Storage buffers bound per stage for draw-time binding.
		struct BoundStorageBuffer
		{
			SDLGPUStorageBuffer* Buffer = nullptr;
		};
		std::map<int, BoundStorageBuffer> _boundVertexStorageBuffers;   // Key = slot.
		std::map<int, BoundStorageBuffer> _boundFragmentStorageBuffers;

		// Current render target info (needed for PSO cache key).
		SDL_GPUTextureFormat _currentColorFormats[4] = {};
		int                  _numCurrentColorTargets = 0;
		SDL_GPUTextureFormat _currentDepthFormat     = SDL_GPU_TEXTUREFORMAT_INVALID;
		bool                 _hasCurrentDepthTarget  = false;

		// Current render pass state for copy pass interruption.
		bool _inRenderPass = false;
		RenderPassDescriptor _savedRenderPassDesc;

		// Backbuffer render target for Present() blit.
		SDL_GPUTexture* _backbufferTexture  = nullptr;
		SDL_GPUTextureFormat _swapchainFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

		// Dummy 1x1 white textures + sampler for filling unbound sampler slots.
		SDL_GPUTexture* _dummyTexture2D      = nullptr;
		SDL_GPUTexture* _dummyTexture2DArray = nullptr;
		SDL_GPUSampler* _dummySampler        = nullptr;

		int _screenWidth  = 0;
		int _screenHeight = 0;
		int _refreshRate  = 60;

		// Preferred SDL_GPU driver name ("vulkan", "d3d12", "metal", or nullptr for auto).
		std::string _preferredDriver;

		// Best supported depth-stencil format (chosen at init time).
		SDL_GPUTextureFormat _depthStencilFormat = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;

		// --- Private helpers ---
		SDL_GPUTextureFormat ChooseDepthStencilFormat();
		void CreateSamplers();
		SDL_GPUSampler* GetSampler(SamplerStateRegister reg);
		void EnsureCommandBuffer();
		SDL_GPUCopyPass* BeginCopyPass();
		void EndCopyPass(SDL_GPUCopyPass* copyPass);
		void UploadToGPUBuffer(SDL_GPUBuffer* buffer, const void* data, size_t size);
		void UploadToTexture(SDL_GPUTexture* texture, int width, int height, SDL_GPUTextureFormat format, const void* data);
		void PushUniformsForDraw();
		void BindTexturesForDraw();
		void BindStorageBuffersForDraw();
		SDL_GPUGraphicsPipeline* GetCurrentPipeline();

		// Re-begin the render pass after a copy pass interruption.
		void ResumeRenderPass();

	public:
		SDLGPUGraphicsDevice(const char* preferredDriver = nullptr);
		~SDLGPUGraphicsDevice();

		// --- Accessors for batch renderers ---
		SDL_GPUDevice* GetDevice() const { return _device; }
		SDL_GPUCommandBuffer* GetCommandBuffer() const { return _commandBuffer; }
		SDL_GPURenderPass* GetRenderPass() const { return _renderPass; }
		int GetScreenWidth() override;
		int GetScreenHeight() override;

		// Public helpers for batch renderers (SpriteBatch, PrimitiveBatch).
		void UploadBufferDataForBatch(SDL_GPUBuffer* buffer, const void* data, Uint32 size, Uint32 offset = 0);
		void DrawPrimitivesForBatch(SDL_GPUBuffer* vertexBuffer, Uint32 vertexCount);
		SDL_GPUTextureFormat GetCurrentColorFormat() const;
		SDL_GPUTextureFormat GetCurrentDepthFormat() const;

		// --- IGraphicsDevice implementation ---

		std::unique_ptr<IVertexBuffer> CreateVertexBuffer(int numVertices, int vertexSize, void* data) override;
		void UpdateVertexBuffer(IVertexBuffer* vertexBuffer, int startVertex, int count, void* data) override;
		void BindVertexBuffer(IVertexBuffer* vertexBuffer) override;

		std::unique_ptr<IIndexBuffer> CreateIndexBuffer(int numIndices, int* data) override;
		void UpdateIndexBuffer(IIndexBuffer* indexBuffer, int numIndices, int startIndex, int* data) override;
		void BindIndexBuffer(IIndexBuffer* indexBuffer) override;

		std::unique_ptr<IRenderSurface2D> CreateRenderSurface2D(int width, int height, SurfaceFormat colorFormat, bool isTypeless, DepthFormat depthFormat) override;
		std::unique_ptr<IRenderSurface2D> CreateRenderSurface2D(int width, int height, int arraySize, SurfaceFormat colorFormat, DepthFormat depthFormat) override;
		std::unique_ptr<IRenderSurface2D> CreateRenderSurface2D(IRenderSurface2D* parentRenderTarget, SurfaceFormat colorFormat) override;

		IRenderTargetCube* CreateRenderTargetCube(int size, SurfaceFormat colorFormat) override;

		std::unique_ptr<ITexture2D> CreateTexture2D(int width, int height, SurfaceFormat format, void* data, bool isDynamic = false) override;
		std::unique_ptr<ITexture2D> CreateTexture2DFromFile(const std::string fileName) override;
		std::unique_ptr<ITexture2D> CreateTexture2DFromFileInMemory(int dataSize, unsigned char* data) override;
		void UpdateTexture2D(ITexture2D* texture, std::vector<char> data) override;

		void SetBlendMode(BlendMode blendMode) override;
		void SetDepthState(DepthState depthState) override;
		void SetCullMode(CullMode cullMode) override;
		void SetScissor(RendererRectangle rectangle) override;
		void SetScissor(RendererViewport viewport) override;

		void BindTexture(TextureRegister registerType, ITextureBase* texture, SamplerStateRegister samplerType) override;

		std::unique_ptr<IConstantBuffer> CreateConstantBuffer(int size, std::string name) override;
		void UpdateConstantBuffer(IConstantBuffer* constantBuffer, void* data) override;
		void BindConstantBuffer(ShaderStage shaderStage, ConstantBufferRegister constantBufferType, IConstantBuffer* buffer) override;

		void DrawIndexedTriangles(int count, int baseIndex, int baseVertex) override;
		void DrawIndexedInstancedTriangles(int count, int instances, int baseIndex, int baseVertex) override;
		void DrawInstancedTriangles(int count, int instances, int baseVertex) override;
		void DrawTriangles(int count, int baseVertex) override;

		void ClearRenderTarget2D(IRenderTarget2D* renderTarget, Vector4 clearColor) override;
		void ClearRenderTarget2D(IRenderTarget2D* renderTarget, int arrayIndex, Vector4 clearColor) override;

		void ClearDepthStencil(IDepthTarget* depthTarget, DepthStencilClearFlags clearFlags, float depth, unsigned char stencil) override;
		void ClearDepthStencil(IDepthTarget* depthTarget, int arrayIndex, DepthStencilClearFlags clearFlags, float depth, unsigned char stencil) override;

		void BindRenderTarget(IRenderTarget2D* renderTarget, IDepthTarget* depthTarget) override;
		void BindRenderTarget(IRenderTargetBinding renderTarget, IDepthTargetBinding depthTarget) override;
		void BindRenderTargets(std::vector<IRenderTarget2D*> renderTargets, IDepthTarget* depthTarget) override;
		void BindRenderTargets(std::vector<IRenderTargetBinding> renderTargets, IDepthTargetBinding depthTarget) override;

		void SetPrimitiveType(PrimitiveType primitiveType) override;

		void SetInputLayout(IInputLayout* inputLayout) override;
		std::unique_ptr<IInputLayout> CreateInputLayout(std::vector<RendererInputLayoutField> fields, IShader* shader) override;

		void CreateDevice() override;
		void Initialize() override;
		std::unique_ptr<IRenderSurface2D> InitializeSwapChain(int width, int height) override;
		std::string GetDefaultAdapterName() override;
		AdapterInfo GetAdapterInfo() override;
		void ResizeSwapChain(int width, int height) override;

		std::unique_ptr<IShader> CreateShader(ShaderCompileRequest& request) override;
		void BindShader(ShaderStage shaderStage, IShader* shader, bool forceNull) override;

		void Present() override;
		void ClearState() override;

		std::unique_ptr<ISpriteFont> InitializeSpriteFont(std::string fontPath) override;
		std::unique_ptr<ISpriteBatch> InitializeSpriteBatch() override;
		std::unique_ptr<IPrimitiveBatch> InitializePrimitiveBatch() override;

		void SetViewport(RendererViewport viewport) override;
		Vector3 Unproject(Vector3 position, Matrix projection, Matrix view, Matrix world) override;

		void SaveScreenshot(IRenderTarget2D* renderTarget, std::string path) override;

		void Flush() override;
		void UnbindAllRenderTargets() override;

		int GetRefreshRate() override;

		bool NeedsFBOYFlip() const override { return false; }

		void BeginDebugEvent(const std::string& name) override;
		void EndDebugEvent() override;

		// Render pass API override.
		void BeginRenderPass(const RenderPassDescriptor& desc) override;
		void EndRenderPass() override;

		// Pipeline state API override.
		void BindPipeline(const RenderPipelineState& state) override;

		// Storage buffer API for large data (bypasses push uniform ring buffer).
		std::unique_ptr<SDLGPUStorageBuffer> CreateStorageBuffer(int capacity);
		void BindStorageBuffer(ShaderStage stage, int slot, SDLGPUStorageBuffer* buffer);
	};
}

#endif
