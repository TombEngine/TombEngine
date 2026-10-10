#pragma once

#include "Renderer/RHI/RHITypes.h"

struct SDL_Window;

namespace TEN::Renderer::RHI
{
    class IResource
    {
    public:
        virtual ~IResource() = default;
    };

    class IBuffer : public IResource
    {
    public:
        virtual int          GetSize() const = 0;
        virtual unsigned int GetUsage() const = 0;
    };

    class ITexture : public IResource
    {
    public:
        virtual const TextureDesc& GetDesc() const = 0;
    };

    class ISampler : public IResource {};

    class IShader : public IResource
    {
    public:
        virtual const ShaderDesc& GetDesc() const = 0;
    };

    class IGraphicsPipeline : public IResource {};
    class IComputePipeline : public IResource {};
    class IFence : public IResource {};

    class ITransferBuffer : public IResource
    {
    public:
        virtual int GetSize() const = 0;
    };

    // Commands recorded between BeginRenderPass and EndRenderPass. Resources cannot be uploaded
    // and render targets cannot change while a render pass is open.
    class IRenderPass
    {
    public:
        virtual ~IRenderPass() = default;

        virtual void BindPipeline(IGraphicsPipeline* pipeline) = 0;
        virtual void SetViewport(const Viewport& viewport) = 0;
        virtual void SetScissor(const ScissorRect& scissor) = 0;

        virtual void BindVertexBuffers(int firstSlot, const BufferBinding* bindings, int count) = 0;
        virtual void BindIndexBuffer(const BufferBinding& binding, IndexSize indexSize) = 0;

        virtual void BindVertexSamplers(int firstSlot, const TextureSamplerBinding* bindings, int count) = 0;
        virtual void BindVertexStorageBuffers(int firstSlot, IBuffer* const* buffers, int count) = 0;
        virtual void BindPixelSamplers(int firstSlot, const TextureSamplerBinding* bindings, int count) = 0;
        virtual void BindPixelStorageBuffers(int firstSlot, IBuffer* const* buffers, int count) = 0;

        virtual void Draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance) = 0;
        virtual void DrawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance) = 0;
    };

    class ICopyPass
    {
    public:
        virtual ~ICopyPass() = default;

        virtual void UploadToBuffer(ITransferBuffer* source, int sourceOffset, IBuffer* destination, int destinationOffset, int size, bool cycle) = 0;
        virtual void UploadToTexture(const TextureTransferInfo& source, const TextureRegion& destination, bool cycle) = 0;
        virtual void CopyTexture(const TextureRegion& source, const TextureRegion& destination) = 0;
        virtual void DownloadFromTexture(const TextureRegion& source, const TextureTransferInfo& destination) = 0;
    };

    class IComputePass
    {
    public:
        virtual ~IComputePass() = default;

        virtual void BindPipeline(IComputePipeline* pipeline) = 0;
        virtual void BindSamplers(int firstSlot, const TextureSamplerBinding* bindings, int count) = 0;
        virtual void BindStorageBuffers(int firstSlot, IBuffer* const* buffers, int count) = 0;
        virtual void Dispatch(int groupCountX, int groupCountY, int groupCountZ) = 0;
    };

    // Read-write resources of a compute pass are declared when the pass begins, as SDL GPU requires.
    struct ComputePassDesc
    {
        ITexture* RWTextures[MAX_STORAGE_SLOTS] = {};
        int       NumRWTextures                 = 0;
        IBuffer*  RWBuffers[MAX_STORAGE_SLOTS]  = {};
        int       NumRWBuffers                  = 0;
    };

    class ICommandBuffer
    {
    protected:
        CommandCounters Counters = {};

    public:
        virtual ~ICommandBuffer() = default;

        // API calls recorded since the command buffer was acquired.
        const CommandCounters& GetCounters() const { return Counters; }
        void                   ResetCounters()     { Counters = {}; }

        // Returns nullptr when no swapchain image is available (minimized window); skip rendering then.
        virtual ITexture* AcquireSwapchainTexture() = 0;

        virtual IRenderPass*  BeginRenderPass(const ColorTargetInfo* colorTargets, int colorTargetCount, const DepthTargetInfo* depthTarget) = 0;
        virtual void          EndRenderPass() = 0;
        virtual ICopyPass*    BeginCopyPass() = 0;
        virtual void          EndCopyPass() = 0;
        virtual IComputePass* BeginComputePass(const ComputePassDesc& desc) = 0;
        virtual void          EndComputePass() = 0;

        // Uniform data is copied at call time and applies to subsequent draws or dispatches.
        // Each slot maps to register b<slot> of the given stage.
        virtual void PushUniforms(ShaderStage stage, int slot, const void* data, int size) = 0;

        // Copies the whole source texture to the whole destination, scaling with linear filtering
        // when sizes differ. Must be called outside of passes.
        virtual void BlitTexture(ITexture* source, ITexture* destination) = 0;

        virtual void PushDebugGroup(const char* name) = 0;
        virtual void PopDebugGroup() = 0;
        virtual void InsertDebugLabel(const char* name) = 0;

        virtual void Submit() = 0;

        // Submits and returns a fence signaled when the GPU has executed this command buffer.
        virtual std::unique_ptr<IFence> SubmitWithFence() = 0;
    };

    class IDevice
    {
    public:
        virtual ~IDevice() = default;

        virtual Backend      GetBackend() const = 0;
        virtual ShaderFormat GetShaderFormat() const = 0;
        virtual std::string  GetDeviceName() const = 0;

        virtual std::unique_ptr<IBuffer>           CreateBuffer(const BufferDesc& desc) = 0;
        virtual std::unique_ptr<ITexture>          CreateTexture(const TextureDesc& desc) = 0;
        virtual std::unique_ptr<ISampler>          CreateSampler(const SamplerDesc& desc) = 0;
        virtual std::unique_ptr<IShader>           CreateShader(const ShaderDesc& desc) = 0;
        virtual std::unique_ptr<IGraphicsPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
        virtual std::unique_ptr<IComputePipeline>  CreateComputePipeline(const ComputePipelineDesc& desc) = 0;
        virtual std::unique_ptr<ITransferBuffer>   CreateTransferBuffer(TransferBufferUsage usage, int size) = 0;

        // With cycle set, a transfer buffer still used by the GPU is renamed instead of overwritten.
        virtual void* MapTransferBuffer(ITransferBuffer* buffer, bool cycle) = 0;
        virtual void  UnmapTransferBuffer(ITransferBuffer* buffer) = 0;

        virtual void GenerateMipmaps(ICommandBuffer* commandBuffer, ITexture* texture) = 0;

        virtual ICommandBuffer* AcquireCommandBuffer() = 0;

        virtual PixelFormat GetSwapchainFormat() const = 0;
        virtual void        SetPresentMode(PresentMode mode) = 0;
        virtual void        ResizeSwapchain(int width, int height) = 0;
        virtual void        WaitIdle() = 0;
        virtual bool        IsFenceSignaled(IFence* fence) = 0;

        virtual bool SupportsTextureFormat(PixelFormat format, TextureType type, unsigned int usage) const = 0;

        // SDL GPU requires swapchain acquisition on the thread that created the window.
        virtual bool RequiresWindowThreadPresent() const = 0;

        // Average GPU time of the debug groups directly inside the outermost one, plus the outermost group itself,
        // since the previous call. Empty when the backend has no timestamp queries (SDL GPU).
        virtual std::vector<GpuTiming> ConsumeGpuTimings() { return {}; }
    };

    // With preferLowPower, hybrid graphics systems use the integrated GPU instead of the discrete one.
    std::unique_ptr<IDevice> CreateDevice(Backend preferredBackend, SDL_Window* window, bool debugMode, bool preferLowPower = false);
}
