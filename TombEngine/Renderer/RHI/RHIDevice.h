#pragma once

#include "Renderer/RHI/RHITypes.h"

struct SDL_Window;

// Rendering hardware interface: a thin layer modeled on SDL GPU, implemented by the DX11 (Direct3D 11), SDL GPU
// (Vulkan and Direct3D 12) and OpenGL 4.5 backends. Work is recorded into a command buffer as render, copy and
// compute passes; resources are created by the device and owned by the caller through std::unique_ptr.
// Rules shared by every backend follow SDL GPU: no copy or compute pass inside a render pass, bindings and
// uniforms per stage and slot, and data reaching GPU resources only through transfer buffers and copy passes.

namespace TEN::Renderer::RHI
{
    // Base of every object created by the device; destroying it releases the backend object. Resources should be
    // destroyed before the device: the SDL GPU backend releases them through the device handle.
    class IResource
    {
    public:
        virtual ~IResource() = default;
    };

    class IBuffer : public IResource
    {
    public:
        virtual int          GetSize() const = 0;  // Bytes.
        virtual unsigned int GetUsage() const = 0; // BufferUsage flags.
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

    // Signaled when the GPU has executed a submitted command buffer; see ICommandBuffer::SubmitWithFence.
    class IFence : public IResource {};

    // CPU visible staging memory for uploads and downloads. Written or read through IDevice::MapTransferBuffer and
    // consumed by a copy pass. DX11 and OpenGL keep it in system memory and copy at call time.
    class ITransferBuffer : public IResource
    {
    public:
        virtual int GetSize() const = 0; // Bytes.
    };

    // Commands recorded between BeginRenderPass and EndRenderPass. Resources cannot be uploaded
    // and render targets cannot change while a render pass is open.
    // Bindings do not outlive the pass: every pass binds what it uses again. The returned pass object belongs to
    // the command buffer and is valid only until EndRenderPass.
    class IRenderPass
    {
    public:
        virtual ~IRenderPass() = default;

        virtual void BindPipeline(IGraphicsPipeline* pipeline) = 0;
        // Viewport and scissor are in pixels from the upper left corner of the target. Both are reset to the
        // full target when the pass begins.
        virtual void SetViewport(const Viewport& viewport) = 0;
        virtual void SetScissor(const ScissorRect& scissor) = 0;

        // Vertex buffer slots match VertexBufferLayout::Slot; strides come from the bound pipeline, so buffers may be
        // bound before or after it. Offsets are in bytes.
        virtual void BindVertexBuffers(int firstSlot, const BufferBinding* bindings, int count) = 0;
        virtual void BindIndexBuffer(const BufferBinding& binding, IndexSize indexSize) = 0;

        // Texture slot N maps to texture tN and sampler sN of the stage (TEN_VS_TEXTURE and TEN_PS_TEXTURE).
        // Storage buffer index N follows the sampled and storage textures of the shader; backends add that base.
        virtual void BindVertexSamplers(int firstSlot, const TextureSamplerBinding* bindings, int count) = 0;
        virtual void BindVertexStorageBuffers(int firstSlot, IBuffer* const* buffers, int count) = 0;
        virtual void BindPixelSamplers(int firstSlot, const TextureSamplerBinding* bindings, int count) = 0;
        virtual void BindPixelStorageBuffers(int firstSlot, IBuffer* const* buffers, int count) = 0;

        // Counts and offsets are in vertices, indices and instances. vertexOffset is added to every index.
        virtual void Draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance) = 0;
        virtual void DrawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance) = 0;
    };

    // Transfers between transfer buffers and GPU resources, recorded between BeginCopyPass and EndCopyPass.
    // With cycle set, a destination still in use by submitted GPU work is replaced by a new internal resource
    // instead of being overwritten (SDL GPU); its contents not written by this upload become undefined.
    // DX11 and OpenGL ignore cycle, as they copy at call time.
    class ICopyPass
    {
    public:
        virtual ~ICopyPass() = default;

        // Offsets and size in bytes.
        virtual void UploadToBuffer(ITransferBuffer* source, int sourceOffset, IBuffer* destination, int destinationOffset, int size, bool cycle) = 0;
        virtual void UploadToTexture(const TextureTransferInfo& source, const TextureRegion& destination, bool cycle) = 0;

        // Copies source.Width x Height x Depth texels; only the position of the destination region is used.
        virtual void CopyTexture(const TextureRegion& source, const TextureRegion& destination) = 0;

        // The data is in the transfer buffer once the command buffer has completed: wait for its fence (or WaitIdle)
        // before mapping. DX11 reads the data back when the transfer buffer is mapped, OpenGL during this call.
        virtual void DownloadFromTexture(const TextureRegion& source, const TextureTransferInfo& destination) = 0;
    };

    // Compute work recorded between BeginComputePass and EndComputePass. Read-write resources are bound by the
    // ComputePassDesc given to BeginComputePass.
    class IComputePass
    {
    public:
        virtual ~IComputePass() = default;

        virtual void BindPipeline(IComputePipeline* pipeline) = 0;
        virtual void BindSamplers(int firstSlot, const TextureSamplerBinding* bindings, int count) = 0;
        virtual void BindStorageBuffers(int firstSlot, IBuffer* const* buffers, int count) = 0; // Read-only storage buffers.
        virtual void Dispatch(int groupCountX, int groupCountY, int groupCountZ) = 0;            // Thread group counts.
    };

    // Read-write resources of a compute pass are declared when the pass begins, as SDL GPU requires.
    // Index N of each array is binding N of TEN_CS_RW_TEXTURE and TEN_CS_RW_STORAGE_BUFFER (u registers on DX11,
    // where the buffers follow the textures).
    struct ComputePassDesc
    {
        ITexture* RWTextures[MAX_STORAGE_SLOTS] = {};
        int       NumRWTextures                 = 0;
        IBuffer*  RWBuffers[MAX_STORAGE_SLOTS]  = {};
        int       NumRWBuffers                  = 0;
    };

    // A recording of GPU work, acquired from IDevice::AcquireCommandBuffer and recorded by one thread at a time.
    // The pointer stays valid until Submit or SubmitWithFence; the device reuses the object afterwards.
    // DX11 and OpenGL execute commands on the immediate context as they are recorded, so Submit mainly presents.
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
        // The texture is presented when this command buffer is submitted and is valid only until then.
        // SDL GPU requires the call on the window thread (see IDevice::RequiresWindowThreadPresent).
        virtual ITexture* AcquireSwapchainTexture() = 0;

        // Only one pass can be open at a time. Pass objects are owned by the command buffer and valid until
        // the matching End call. depthTarget may be nullptr.
        virtual IRenderPass*  BeginRenderPass(const ColorTargetInfo* colorTargets, int colorTargetCount, const DepthTargetInfo* depthTarget) = 0;
        virtual void          EndRenderPass() = 0;
        virtual ICopyPass*    BeginCopyPass() = 0;
        virtual void          EndCopyPass() = 0;
        virtual IComputePass* BeginComputePass(const ComputePassDesc& desc) = 0;
        virtual void          EndComputePass() = 0;

        // Uniform data is copied at call time and applies to subsequent draws or dispatches.
        // Each slot is the uniform block declared with TEN_VS_UNIFORMS, TEN_PS_UNIFORMS or TEN_CS_UNIFORMS at
        // that slot of the given stage; ShaderInterop.hlsli maps it to the binding of each backend.
        // At most MAX_UNIFORM_SLOTS slots per stage; size is in bytes, at most 16 KB on DX11.
        virtual void PushUniforms(ShaderStage stage, int slot, const void* data, int size) = 0;

        // Copies the whole source texture to the whole destination, scaling with linear filtering
        // when sizes differ. Must be called outside of passes. DX11 copies same-size textures only.
        virtual void BlitTexture(ITexture* source, ITexture* destination) = 0;

        // Markers for graphics debuggers. The outermost group and the groups directly inside it are also the
        // scopes measured by GPU timing (see IDevice::SetGpuTimingEnabled).
        virtual void PushDebugGroup(const char* name) = 0;
        virtual void PopDebugGroup() = 0;
        virtual void InsertDebugLabel(const char* name) = 0;

        // Sends the recorded work to the GPU and presents the swapchain texture if one was acquired.
        virtual void Submit() = 0;

        // Submits and returns a fence signaled when the GPU has executed this command buffer.
        // May return nullptr on failure; IsFenceSignaled and WaitForFence treat a null fence as signaled.
        virtual std::unique_ptr<IFence> SubmitWithFence() = 0;
    };

    // Creates resources and command buffers and owns the swapchain of the window.
    // Resource creation may happen on any thread (OpenGL takes its context to the calling thread, see ReleaseThread).
    // Creation functions return nullptr and log the error on failure.
    class IDevice
    {
    public:
        virtual ~IDevice() = default;

        virtual GraphicsApi      GetBackend() const = 0;
        virtual ShaderFormat GetShaderFormat() const = 0; // Bytecode format LoadShader picks for this device.
        virtual std::string  GetDeviceName() const = 0;   // Adapter name followed by the driver in parentheses.

        virtual std::unique_ptr<IBuffer>           CreateBuffer(const BufferDesc& desc) = 0;
        virtual std::unique_ptr<ITexture>          CreateTexture(const TextureDesc& desc) = 0;
        virtual std::unique_ptr<ISampler>          CreateSampler(const SamplerDesc& desc) = 0;
        // The code is copied where the backend needs it later, so desc.Code may be freed after the call.
        virtual std::unique_ptr<IShader>           CreateShader(const ShaderDesc& desc) = 0;
        virtual std::unique_ptr<IGraphicsPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
        virtual std::unique_ptr<IComputePipeline>  CreateComputePipeline(const ComputePipelineDesc& desc) = 0;
        virtual std::unique_ptr<ITransferBuffer>   CreateTransferBuffer(TransferBufferUsage usage, int size) = 0; // Size in bytes.

        // With cycle set, a transfer buffer still used by the GPU is renamed instead of overwritten.
        // Without it, the caller must make sure the GPU is done with the buffer (fence or WaitIdle).
        // The pointer is valid until UnmapTransferBuffer; DX11 and OpenGL ignore cycle and return system memory.
        virtual void* MapTransferBuffer(ITransferBuffer* buffer, bool cycle) = 0;
        virtual void  UnmapTransferBuffer(ITransferBuffer* buffer) = 0;

        // Fills the mips below level 0. Recorded outside of passes; the texture needs the ColorTarget and Sampler usage.
        virtual void GenerateMipmaps(ICommandBuffer* commandBuffer, ITexture* texture) = 0;

        // Returns nullptr on failure. SDL GPU allows several command buffers recorded at once on different threads;
        // DX11 and OpenGL always return the same object.
        virtual ICommandBuffer* AcquireCommandBuffer() = 0;

        virtual PixelFormat GetSwapchainFormat() const = 0;
        // SDL GPU falls back to Vsync for unsupported modes; DX11 and OpenGL present every other mode without vertical sync.
        virtual void        SetPresentMode(PresentMode mode) = 0;
        virtual bool        SupportsPresentMode(PresentMode mode) const { return (mode == PresentMode::Vsync); }
        // Called after the window size changes; SDL GPU resizes on the next swapchain acquisition by itself.
        virtual void        ResizeSwapchain(int width, int height) = 0;
        // Waits until submitted work has completed (DX11 only flushes its context).
        virtual void        WaitIdle() = 0;
        virtual bool        IsFenceSignaled(IFence* fence) = 0;
        virtual void        WaitForFence(IFence* fence) = 0;

        // usage is a combination of TextureUsage flags.
        virtual bool SupportsTextureFormat(PixelFormat format, TextureType type, unsigned int usage) const = 0;

        // SDL GPU requires swapchain acquisition on the thread that created the window.
        // When true, the renderer draws the frame into its own target and copies it to the swapchain from the window thread.
        virtual bool RequiresWindowThreadPresent() const = 0;

        // An OpenGL context is current on one thread at a time: a thread that stops using the device releases it,
        // and the next call from another thread takes it.
        virtual void ReleaseThread() {}

        // Average GPU time of the debug groups directly inside the outermost one, plus the outermost group itself,
        // since the previous call. SDL GPU has no timestamp queries and measures fences only while timing is enabled,
        // splitting command buffers at those groups.
        virtual void                   SetGpuTimingEnabled(bool enabled) {}
        virtual std::vector<GpuTiming> ConsumeGpuTimings() { return {}; }
    };

    // Tries the preferred backend, then Vulkan, Direct3D 12, DX11 and OpenGL; GetBackend tells which one was created.
    // debugMode enables the validation layers of the API where they are installed.
    // With preferLowPower, hybrid graphics systems use the integrated GPU instead of the discrete one.
    std::unique_ptr<IDevice> CreateDevice(GraphicsApi preferredBackend, SDL_Window* window, bool debugMode, bool preferLowPower = false);

    // SDL window flags the backend needs when the window is created.
    unsigned int GetWindowFlags(GraphicsApi backend);
}
