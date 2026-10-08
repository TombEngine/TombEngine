#include "framework.h"
#include "Renderer/RHI/SDLGPU/SDLGPUDevice.h"

#include <mutex>
#include <SDL3/SDL_gpu.h>

namespace TEN::Renderer::RHI::SDLGPU
{
    constexpr auto COMMAND_BUFFER_POOL_SIZE = 8;

    static SDL_GPUTextureFormat ToSDL(PixelFormat format)
    {
        switch (format)
        {
        case PixelFormat::R8Unorm:        return SDL_GPU_TEXTUREFORMAT_R8_UNORM;
        case PixelFormat::RG8Unorm:       return SDL_GPU_TEXTUREFORMAT_R8G8_UNORM;
        case PixelFormat::RGBA8Unorm:     return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        case PixelFormat::RGBA8UnormSrgb: return SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
        case PixelFormat::BGRA8Unorm:     return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        case PixelFormat::BGRA8UnormSrgb: return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
        case PixelFormat::R16Float:       return SDL_GPU_TEXTUREFORMAT_R16_FLOAT;
        case PixelFormat::RG16Float:      return SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT;
        case PixelFormat::RGBA16Float:    return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        case PixelFormat::R32Float:       return SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
        case PixelFormat::RG32Float:      return SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT;
        case PixelFormat::RGBA32Float:    return SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT;
        case PixelFormat::R11G11B10Float: return SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT;
        case PixelFormat::R32Uint:        return SDL_GPU_TEXTUREFORMAT_R32_UINT;
        case PixelFormat::BC1Unorm:       return SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM;
        case PixelFormat::BC2Unorm:       return SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM;
        case PixelFormat::BC3Unorm:       return SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM;
        case PixelFormat::BC4Unorm:       return SDL_GPU_TEXTUREFORMAT_BC4_R_UNORM;
        case PixelFormat::BC5Unorm:       return SDL_GPU_TEXTUREFORMAT_BC5_RG_UNORM;
        case PixelFormat::BC7Unorm:       return SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM;
        case PixelFormat::D16Unorm:       return SDL_GPU_TEXTUREFORMAT_D16_UNORM;
        case PixelFormat::D24UnormS8Uint: return SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
        case PixelFormat::D32Float:       return SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        default:                          return SDL_GPU_TEXTUREFORMAT_INVALID;
        }
    }

    static PixelFormat FromSDL(SDL_GPUTextureFormat format)
    {
        switch (format)
        {
        case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:      return PixelFormat::RGBA8Unorm;
        case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB: return PixelFormat::RGBA8UnormSrgb;
        case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:      return PixelFormat::BGRA8Unorm;
        case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB: return PixelFormat::BGRA8UnormSrgb;
        case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT:  return PixelFormat::RGBA16Float;
        default:                                        return PixelFormat::Unknown;
        }
    }

    static SDL_GPUVertexElementFormat ToSDL(VertexElementFormat format)
    {
        switch (format)
        {
        case VertexElementFormat::Float:      return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
        case VertexElementFormat::Float2:     return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        case VertexElementFormat::Float3:     return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        case VertexElementFormat::Float4:     return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        case VertexElementFormat::UByte4:     return SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4;
        case VertexElementFormat::UByte4Norm: return SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
        case VertexElementFormat::Byte4Norm:  return SDL_GPU_VERTEXELEMENTFORMAT_BYTE4_NORM;
        case VertexElementFormat::Short2Norm: return SDL_GPU_VERTEXELEMENTFORMAT_SHORT2_NORM;
        case VertexElementFormat::Short4Norm: return SDL_GPU_VERTEXELEMENTFORMAT_SHORT4_NORM;
        case VertexElementFormat::Half2:      return SDL_GPU_VERTEXELEMENTFORMAT_HALF2;
        case VertexElementFormat::Half4:      return SDL_GPU_VERTEXELEMENTFORMAT_HALF4;
        case VertexElementFormat::UInt:       return SDL_GPU_VERTEXELEMENTFORMAT_UINT;
        default:                              return SDL_GPU_VERTEXELEMENTFORMAT_INVALID;
        }
    }

    static SDL_GPUCompareOp ToSDL(CompareOp op)
    {
        switch (op)
        {
        case CompareOp::Never:          return SDL_GPU_COMPAREOP_NEVER;
        case CompareOp::Less:           return SDL_GPU_COMPAREOP_LESS;
        case CompareOp::Equal:          return SDL_GPU_COMPAREOP_EQUAL;
        case CompareOp::LessOrEqual:    return SDL_GPU_COMPAREOP_LESS_OR_EQUAL;
        case CompareOp::Greater:        return SDL_GPU_COMPAREOP_GREATER;
        case CompareOp::NotEqual:       return SDL_GPU_COMPAREOP_NOT_EQUAL;
        case CompareOp::GreaterOrEqual: return SDL_GPU_COMPAREOP_GREATER_OR_EQUAL;
        default:                        return SDL_GPU_COMPAREOP_ALWAYS;
        }
    }

    static SDL_GPUBlendFactor ToSDL(BlendFactor factor)
    {
        switch (factor)
        {
        case BlendFactor::Zero:             return SDL_GPU_BLENDFACTOR_ZERO;
        case BlendFactor::One:              return SDL_GPU_BLENDFACTOR_ONE;
        case BlendFactor::SrcColor:         return SDL_GPU_BLENDFACTOR_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor: return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_COLOR;
        case BlendFactor::DstColor:         return SDL_GPU_BLENDFACTOR_DST_COLOR;
        case BlendFactor::OneMinusDstColor: return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_COLOR;
        case BlendFactor::SrcAlpha:         return SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha: return SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha:         return SDL_GPU_BLENDFACTOR_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha: return SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA;
        default:                            return SDL_GPU_BLENDFACTOR_ONE;
        }
    }

    static SDL_GPUBlendOp ToSDL(BlendOp op)
    {
        switch (op)
        {
        case BlendOp::Subtract:        return SDL_GPU_BLENDOP_SUBTRACT;
        case BlendOp::ReverseSubtract: return SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
        case BlendOp::Min:             return SDL_GPU_BLENDOP_MIN;
        case BlendOp::Max:             return SDL_GPU_BLENDOP_MAX;
        default:                       return SDL_GPU_BLENDOP_ADD;
        }
    }

    static SDL_GPUSamplerAddressMode ToSDL(AddressMode mode)
    {
        switch (mode)
        {
        case AddressMode::MirroredRepeat: return SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
        case AddressMode::ClampToEdge:    return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        default:                          return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
        }
    }

    static SDL_GPULoadOp ToSDL(LoadOp op)
    {
        switch (op)
        {
        case LoadOp::Load:  return SDL_GPU_LOADOP_LOAD;
        case LoadOp::Clear: return SDL_GPU_LOADOP_CLEAR;
        default:            return SDL_GPU_LOADOP_DONT_CARE;
        }
    }

    static SDL_GPUStoreOp ToSDL(StoreOp op)
    {
        return (op == StoreOp::Store) ? SDL_GPU_STOREOP_STORE : SDL_GPU_STOREOP_DONT_CARE;
    }

    static SDL_GPUTextureType ToSDL(TextureType type)
    {
        switch (type)
        {
        case TextureType::Texture2DArray: return SDL_GPU_TEXTURETYPE_2D_ARRAY;
        case TextureType::TextureCube:    return SDL_GPU_TEXTURETYPE_CUBE;
        case TextureType::Texture3D:      return SDL_GPU_TEXTURETYPE_3D;
        default:                          return SDL_GPU_TEXTURETYPE_2D;
        }
    }

    static SDL_GPUPrimitiveType ToSDL(PrimitiveType type)
    {
        switch (type)
        {
        case PrimitiveType::TriangleStrip: return SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP;
        case PrimitiveType::LineList:      return SDL_GPU_PRIMITIVETYPE_LINELIST;
        case PrimitiveType::LineStrip:     return SDL_GPU_PRIMITIVETYPE_LINESTRIP;
        case PrimitiveType::PointList:     return SDL_GPU_PRIMITIVETYPE_POINTLIST;
        default:                           return SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
        }
    }

    static SDL_GPUSampleCount ToSDLSampleCount(int count)
    {
        switch (count)
        {
        case 2:  return SDL_GPU_SAMPLECOUNT_2;
        case 4:  return SDL_GPU_SAMPLECOUNT_4;
        case 8:  return SDL_GPU_SAMPLECOUNT_8;
        default: return SDL_GPU_SAMPLECOUNT_1;
        }
    }

    static unsigned int ToSDLBufferUsage(unsigned int usage)
    {
        unsigned int result = 0;
        if (usage & BufferUsage::Vertex)
            result |= SDL_GPU_BUFFERUSAGE_VERTEX;
        if (usage & BufferUsage::Index)
            result |= SDL_GPU_BUFFERUSAGE_INDEX;
        if (usage & BufferUsage::Indirect)
            result |= SDL_GPU_BUFFERUSAGE_INDIRECT;
        if (usage & BufferUsage::GraphicsStorageRead)
            result |= SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
        if (usage & BufferUsage::ComputeStorageRead)
            result |= SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ;
        if (usage & BufferUsage::ComputeStorageWrite)
            result |= SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE;
        return result;
    }

    static unsigned int ToSDLTextureUsage(unsigned int usage)
    {
        unsigned int result = 0;
        if (usage & TextureUsage::Sampler)
            result |= SDL_GPU_TEXTUREUSAGE_SAMPLER;
        if (usage & TextureUsage::ColorTarget)
            result |= SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        if (usage & TextureUsage::DepthStencilTarget)
            result |= SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        if (usage & TextureUsage::ComputeStorageRead)
            result |= SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ;
        if (usage & TextureUsage::ComputeStorageWrite)
            result |= SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE;
        return result;
    }

    class SDLGPUBuffer : public IBuffer
    {
    public:
        SDL_GPUDevice* Device = nullptr;
        SDL_GPUBuffer* Handle = nullptr;
        int            Size   = 0;
        unsigned int   Usage  = 0;

        ~SDLGPUBuffer() override { SDL_ReleaseGPUBuffer(Device, Handle); }
        int          GetSize() const override { return Size; }
        unsigned int GetUsage() const override { return Usage; }
    };

    class SDLGPUTexture : public ITexture
    {
    public:
        SDL_GPUDevice*  Device  = nullptr;
        SDL_GPUTexture* Handle  = nullptr;
        TextureDesc     Desc    = {};
        bool            IsOwned = true;

        ~SDLGPUTexture() override
        {
            if (IsOwned)
                SDL_ReleaseGPUTexture(Device, Handle);
        }

        const TextureDesc& GetDesc() const override { return Desc; }
    };

    class SDLGPUSampler : public ISampler
    {
    public:
        SDL_GPUDevice*  Device = nullptr;
        SDL_GPUSampler* Handle = nullptr;

        ~SDLGPUSampler() override { SDL_ReleaseGPUSampler(Device, Handle); }
    };

    // Compute shaders keep their code: SDL GPU creates compute pipelines directly from bytecode.
    class SDLGPUShader : public IShader
    {
    public:
        SDL_GPUDevice*             Device = nullptr;
        SDL_GPUShader*             Handle = nullptr;
        ShaderDesc                 Desc   = {};
        std::vector<unsigned char> Code   = {};

        ~SDLGPUShader() override
        {
            if (Handle != nullptr)
                SDL_ReleaseGPUShader(Device, Handle);
        }

        const ShaderDesc& GetDesc() const override { return Desc; }
    };

    class SDLGPUGraphicsPipeline : public IGraphicsPipeline
    {
    public:
        SDL_GPUDevice*           Device = nullptr;
        SDL_GPUGraphicsPipeline* Handle = nullptr;

        ~SDLGPUGraphicsPipeline() override { SDL_ReleaseGPUGraphicsPipeline(Device, Handle); }
    };

    class SDLGPUComputePipeline : public IComputePipeline
    {
    public:
        SDL_GPUDevice*          Device = nullptr;
        SDL_GPUComputePipeline* Handle = nullptr;

        ~SDLGPUComputePipeline() override { SDL_ReleaseGPUComputePipeline(Device, Handle); }
    };

    class SDLGPUTransferBuffer : public ITransferBuffer
    {
    public:
        SDL_GPUDevice*         Device = nullptr;
        SDL_GPUTransferBuffer* Handle = nullptr;
        int                    Size   = 0;

        ~SDLGPUTransferBuffer() override { SDL_ReleaseGPUTransferBuffer(Device, Handle); }
        int GetSize() const override { return Size; }
    };

    static SDL_GPUBuffer* GetHandle(IBuffer* buffer)
    {
        return (buffer != nullptr) ? ((SDLGPUBuffer*)buffer)->Handle : nullptr;
    }

    static SDL_GPUTexture* GetHandle(ITexture* texture)
    {
        return (texture != nullptr) ? ((SDLGPUTexture*)texture)->Handle : nullptr;
    }

    static SDL_GPUSampler* GetHandle(ISampler* sampler)
    {
        return (sampler != nullptr) ? ((SDLGPUSampler*)sampler)->Handle : nullptr;
    }

    static SDL_GPUTransferBuffer* GetHandle(ITransferBuffer* buffer)
    {
        return (buffer != nullptr) ? ((SDLGPUTransferBuffer*)buffer)->Handle : nullptr;
    }

    static void ToSDLTextureSamplerBindings(const TextureSamplerBinding* bindings, int count, SDL_GPUTextureSamplerBinding* result)
    {
        for (int i = 0; i < count; i++)
        {
            result[i].texture = GetHandle(bindings[i].Texture);
            result[i].sampler = GetHandle(bindings[i].Sampler);
        }
    }

    static void ToSDLBufferHandles(IBuffer* const* buffers, int count, SDL_GPUBuffer** result)
    {
        for (int i = 0; i < count; i++)
            result[i] = GetHandle(buffers[i]);
    }

    static SDL_GPUTextureRegion ToSDL(const TextureRegion& region)
    {
        auto result = SDL_GPUTextureRegion{};
        result.texture = GetHandle(region.Texture);
        result.mip_level = region.MipLevel;
        result.layer = region.Layer;
        result.x = region.X;
        result.y = region.Y;
        result.z = region.Z;
        result.w = region.Width;
        result.h = region.Height;
        result.d = region.Depth;
        return result;
    }

    static SDL_GPUTextureTransferInfo ToSDL(const TextureTransferInfo& info)
    {
        auto result = SDL_GPUTextureTransferInfo{};
        result.transfer_buffer = GetHandle(info.TransferBuffer);
        result.offset = info.Offset;
        result.pixels_per_row = info.PixelsPerRow;
        result.rows_per_layer = info.RowsPerLayer;
        return result;
    }

    class SDLGPURenderPass : public IRenderPass
    {
    public:
        SDL_GPURenderPass* Handle = nullptr;

        void BindPipeline(IGraphicsPipeline* pipeline) override
        {
            SDL_BindGPUGraphicsPipeline(Handle, ((SDLGPUGraphicsPipeline*)pipeline)->Handle);
        }

        void SetViewport(const Viewport& viewport) override
        {
            auto sdlViewport = SDL_GPUViewport{ viewport.X, viewport.Y, viewport.Width, viewport.Height, viewport.MinDepth, viewport.MaxDepth };
            SDL_SetGPUViewport(Handle, &sdlViewport);
        }

        void SetScissor(const ScissorRect& scissor) override
        {
            auto rect = SDL_Rect{ scissor.X, scissor.Y, scissor.Width, scissor.Height };
            SDL_SetGPUScissor(Handle, &rect);
        }

        void BindVertexBuffers(int firstSlot, const BufferBinding* bindings, int count) override
        {
            SDL_GPUBufferBinding sdlBindings[MAX_VERTEX_BUFFERS];
            for (int i = 0; i < count; i++)
                sdlBindings[i] = SDL_GPUBufferBinding{ GetHandle(bindings[i].Buffer), (Uint32)bindings[i].Offset };

            SDL_BindGPUVertexBuffers(Handle, firstSlot, sdlBindings, count);
        }

        void BindIndexBuffer(const BufferBinding& binding, IndexSize indexSize) override
        {
            auto sdlBinding = SDL_GPUBufferBinding{ GetHandle(binding.Buffer), (Uint32)binding.Offset };
            SDL_BindGPUIndexBuffer(Handle, &sdlBinding, (indexSize == IndexSize::Bits16) ? SDL_GPU_INDEXELEMENTSIZE_16BIT : SDL_GPU_INDEXELEMENTSIZE_32BIT);
        }

        void BindVertexSamplers(int firstSlot, const TextureSamplerBinding* bindings, int count) override
        {
            SDL_GPUTextureSamplerBinding sdlBindings[MAX_SAMPLER_SLOTS];
            ToSDLTextureSamplerBindings(bindings, count, sdlBindings);
            SDL_BindGPUVertexSamplers(Handle, firstSlot, sdlBindings, count);
        }

        void BindVertexStorageBuffers(int firstSlot, IBuffer* const* buffers, int count) override
        {
            SDL_GPUBuffer* handles[MAX_STORAGE_SLOTS];
            ToSDLBufferHandles(buffers, count, handles);
            SDL_BindGPUVertexStorageBuffers(Handle, firstSlot, handles, count);
        }

        void BindPixelSamplers(int firstSlot, const TextureSamplerBinding* bindings, int count) override
        {
            SDL_GPUTextureSamplerBinding sdlBindings[MAX_SAMPLER_SLOTS];
            ToSDLTextureSamplerBindings(bindings, count, sdlBindings);
            SDL_BindGPUFragmentSamplers(Handle, firstSlot, sdlBindings, count);
        }

        void BindPixelStorageBuffers(int firstSlot, IBuffer* const* buffers, int count) override
        {
            SDL_GPUBuffer* handles[MAX_STORAGE_SLOTS];
            ToSDLBufferHandles(buffers, count, handles);
            SDL_BindGPUFragmentStorageBuffers(Handle, firstSlot, handles, count);
        }

        void Draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance) override
        {
            SDL_DrawGPUPrimitives(Handle, vertexCount, instanceCount, firstVertex, firstInstance);
        }

        void DrawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance) override
        {
            SDL_DrawGPUIndexedPrimitives(Handle, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        }
    };

    class SDLGPUCopyPass : public ICopyPass
    {
    public:
        SDL_GPUCopyPass* Handle = nullptr;

        void UploadToBuffer(ITransferBuffer* source, int sourceOffset, IBuffer* destination, int destinationOffset, int size, bool cycle) override
        {
            auto location = SDL_GPUTransferBufferLocation{ GetHandle(source), (Uint32)sourceOffset };
            auto region = SDL_GPUBufferRegion{ GetHandle(destination), (Uint32)destinationOffset, (Uint32)size };
            SDL_UploadToGPUBuffer(Handle, &location, &region, cycle);
        }

        void UploadToTexture(const TextureTransferInfo& source, const TextureRegion& destination, bool cycle) override
        {
            auto sdlSource = ToSDL(source);
            auto sdlDestination = ToSDL(destination);
            SDL_UploadToGPUTexture(Handle, &sdlSource, &sdlDestination, cycle);
        }

        void CopyTexture(const TextureRegion& source, const TextureRegion& destination) override
        {
            auto sdlSource = SDL_GPUTextureLocation{ GetHandle(source.Texture), (Uint32)source.MipLevel, (Uint32)source.Layer, (Uint32)source.X, (Uint32)source.Y, (Uint32)source.Z };
            auto sdlDestination = SDL_GPUTextureLocation{ GetHandle(destination.Texture), (Uint32)destination.MipLevel, (Uint32)destination.Layer, (Uint32)destination.X, (Uint32)destination.Y, (Uint32)destination.Z };
            SDL_CopyGPUTextureToTexture(Handle, &sdlSource, &sdlDestination, source.Width, source.Height, source.Depth, false);
        }

        void DownloadFromTexture(const TextureRegion& source, const TextureTransferInfo& destination) override
        {
            auto sdlSource = ToSDL(source);
            auto sdlDestination = ToSDL(destination);
            SDL_DownloadFromGPUTexture(Handle, &sdlSource, &sdlDestination);
        }
    };

    class SDLGPUComputePass : public IComputePass
    {
    public:
        SDL_GPUComputePass* Handle = nullptr;

        void BindPipeline(IComputePipeline* pipeline) override
        {
            SDL_BindGPUComputePipeline(Handle, ((SDLGPUComputePipeline*)pipeline)->Handle);
        }

        void BindSamplers(int firstSlot, const TextureSamplerBinding* bindings, int count) override
        {
            SDL_GPUTextureSamplerBinding sdlBindings[MAX_SAMPLER_SLOTS];
            ToSDLTextureSamplerBindings(bindings, count, sdlBindings);
            SDL_BindGPUComputeSamplers(Handle, firstSlot, sdlBindings, count);
        }

        void BindStorageBuffers(int firstSlot, IBuffer* const* buffers, int count) override
        {
            SDL_GPUBuffer* handles[MAX_STORAGE_SLOTS];
            ToSDLBufferHandles(buffers, count, handles);
            SDL_BindGPUComputeStorageBuffers(Handle, firstSlot, handles, count);
        }

        void Dispatch(int groupCountX, int groupCountY, int groupCountZ) override
        {
            SDL_DispatchGPUCompute(Handle, groupCountX, groupCountY, groupCountZ);
        }
    };

    class SDLGPUCommandBuffer : public ICommandBuffer
    {
    public:
        SDL_GPUDevice*        Device           = nullptr;
        SDL_Window*           Window           = nullptr;
        SDL_GPUCommandBuffer* Handle           = nullptr;
        std::atomic<bool>     IsInUse          = false; // Read by other threads looking for a free wrapper.
        SDLGPUTexture         SwapchainTexture = {};
        SDLGPURenderPass      RenderPass       = {};
        SDLGPUCopyPass        CopyPass         = {};
        SDLGPUComputePass     ComputePass      = {};

        ITexture* AcquireSwapchainTexture() override
        {
            SDL_GPUTexture* texture = nullptr;
            Uint32 width = 0;
            Uint32 height = 0;

            if (!SDL_WaitAndAcquireGPUSwapchainTexture(Handle, Window, &texture, &width, &height))
            {
                TENLog(std::string("SDL GPU: swapchain acquisition failed: ") + SDL_GetError(), LogLevel::Error);
                return nullptr;
            }

            if (texture == nullptr)
                return nullptr;

            SwapchainTexture.Device = Device;
            SwapchainTexture.Handle = texture;
            SwapchainTexture.IsOwned = false;
            SwapchainTexture.Desc.Width = (int)width;
            SwapchainTexture.Desc.Height = (int)height;
            SwapchainTexture.Desc.Format = FromSDL(SDL_GetGPUSwapchainTextureFormat(Device, Window));
            SwapchainTexture.Desc.Usage = TextureUsage::ColorTarget;
            SwapchainTexture.Desc.Name = "Swapchain";
            return &SwapchainTexture;
        }

        IRenderPass* BeginRenderPass(const ColorTargetInfo* colorTargets, int colorTargetCount, const DepthTargetInfo* depthTarget) override
        {
            SDL_GPUColorTargetInfo sdlColorTargets[MAX_COLOR_TARGETS] = {};
            for (int i = 0; i < colorTargetCount; i++)
            {
                const auto& target = colorTargets[i];
                auto& sdlTarget = sdlColorTargets[i];
                sdlTarget.texture = GetHandle(target.Texture);
                sdlTarget.mip_level = target.MipLevel;
                sdlTarget.layer_or_depth_plane = target.Layer;
                sdlTarget.clear_color = SDL_FColor{ target.ClearColor[0], target.ClearColor[1], target.ClearColor[2], target.ClearColor[3] };
                sdlTarget.load_op = ToSDL(target.Load);
                sdlTarget.store_op = ToSDL(target.Store);
            }

            auto sdlDepthTarget = SDL_GPUDepthStencilTargetInfo{};
            if (depthTarget != nullptr)
            {
                sdlDepthTarget.texture = GetHandle(depthTarget->Texture);
                sdlDepthTarget.clear_depth = depthTarget->ClearDepth;
                sdlDepthTarget.load_op = ToSDL(depthTarget->Load);
                sdlDepthTarget.store_op = ToSDL(depthTarget->Store);
                sdlDepthTarget.stencil_load_op = ToSDL(depthTarget->Load);
                sdlDepthTarget.stencil_store_op = ToSDL(depthTarget->Store);
                sdlDepthTarget.clear_stencil = depthTarget->ClearStencil;
                sdlDepthTarget.layer = (Uint8)depthTarget->Layer;
            }

            RenderPass.Handle = SDL_BeginGPURenderPass(Handle, sdlColorTargets, colorTargetCount, (depthTarget != nullptr) ? &sdlDepthTarget : nullptr);
            return &RenderPass;
        }

        void EndRenderPass() override
        {
            SDL_EndGPURenderPass(RenderPass.Handle);
            RenderPass.Handle = nullptr;
        }

        ICopyPass* BeginCopyPass() override
        {
            CopyPass.Handle = SDL_BeginGPUCopyPass(Handle);
            return &CopyPass;
        }

        void EndCopyPass() override
        {
            SDL_EndGPUCopyPass(CopyPass.Handle);
            CopyPass.Handle = nullptr;
        }

        IComputePass* BeginComputePass(const ComputePassDesc& desc) override
        {
            SDL_GPUStorageTextureReadWriteBinding textures[MAX_STORAGE_SLOTS] = {};
            for (int i = 0; i < desc.NumRWTextures; i++)
                textures[i].texture = GetHandle(desc.RWTextures[i]);

            SDL_GPUStorageBufferReadWriteBinding buffers[MAX_STORAGE_SLOTS] = {};
            for (int i = 0; i < desc.NumRWBuffers; i++)
                buffers[i].buffer = GetHandle(desc.RWBuffers[i]);

            ComputePass.Handle = SDL_BeginGPUComputePass(Handle, textures, desc.NumRWTextures, buffers, desc.NumRWBuffers);
            return &ComputePass;
        }

        void EndComputePass() override
        {
            SDL_EndGPUComputePass(ComputePass.Handle);
            ComputePass.Handle = nullptr;
        }

        void PushUniforms(ShaderStage stage, int slot, const void* data, int size) override
        {
            switch (stage)
            {
            case ShaderStage::Vertex:
                SDL_PushGPUVertexUniformData(Handle, slot, data, size);
                break;

            case ShaderStage::Pixel:
                SDL_PushGPUFragmentUniformData(Handle, slot, data, size);
                break;

            case ShaderStage::Compute:
                SDL_PushGPUComputeUniformData(Handle, slot, data, size);
                break;
            }
        }

        void BlitTexture(ITexture* source, ITexture* destination) override
        {
            const auto& sourceDesc = source->GetDesc();
            const auto& destinationDesc = destination->GetDesc();

            auto blitInfo = SDL_GPUBlitInfo{};
            blitInfo.source.texture = GetHandle(source);
            blitInfo.source.w = sourceDesc.Width;
            blitInfo.source.h = sourceDesc.Height;
            blitInfo.destination.texture = GetHandle(destination);
            blitInfo.destination.w = destinationDesc.Width;
            blitInfo.destination.h = destinationDesc.Height;
            blitInfo.load_op = SDL_GPU_LOADOP_DONT_CARE;
            blitInfo.filter = SDL_GPU_FILTER_LINEAR;
            SDL_BlitGPUTexture(Handle, &blitInfo);
        }

        void PushDebugGroup(const char* name) override
        {
            SDL_PushGPUDebugGroup(Handle, name);
        }

        void PopDebugGroup() override
        {
            SDL_PopGPUDebugGroup(Handle);
        }

        void InsertDebugLabel(const char* name) override
        {
            SDL_InsertGPUDebugLabel(Handle, name);
        }

        void Submit() override
        {
            if (!SDL_SubmitGPUCommandBuffer(Handle))
                TENLog(std::string("SDL GPU: command buffer submission failed: ") + SDL_GetError(), LogLevel::Error);

            Handle = nullptr;
            IsInUse = false;
        }
    };

    static SDL_PropertiesID CreateNameProperties(const char* property, const std::string& name)
    {
        if (name.empty())
            return 0;

        auto props = SDL_CreateProperties();
        SDL_SetStringProperty(props, property, name.c_str());
        return props;
    }

    static void DestroyNameProperties(SDL_PropertiesID props)
    {
        if (props != 0)
            SDL_DestroyProperties(props);
    }

    class SDLGPUDevice : public IDevice
    {
    private:
        SDL_GPUDevice* _device  = nullptr;
        SDL_Window*    _window  = nullptr;
        Backend        _backend = Backend::None;
        ShaderFormat   _shaderFormat = ShaderFormat::None;
        PresentMode    _presentMode  = PresentMode::Vsync;

        std::vector<std::unique_ptr<SDLGPUCommandBuffer>> _commandBuffers = {};
        std::mutex _commandBufferMutex = {};

    public:
        SDLGPUDevice(SDL_GPUDevice* device, SDL_Window* window, Backend backend, ShaderFormat shaderFormat) :
            _device(device),
            _window(window),
            _backend(backend),
            _shaderFormat(shaderFormat)
        {
            for (int i = 0; i < COMMAND_BUFFER_POOL_SIZE; i++)
            {
                auto commandBuffer = std::make_unique<SDLGPUCommandBuffer>();
                commandBuffer->Device = device;
                commandBuffer->Window = window;
                _commandBuffers.push_back(std::move(commandBuffer));
            }
        }

        ~SDLGPUDevice() override
        {
            SDL_WaitForGPUIdle(_device);
            _commandBuffers.clear();
            SDL_ReleaseWindowFromGPUDevice(_device, _window);
            SDL_DestroyGPUDevice(_device);
        }

        Backend GetBackend() const override
        {
            return _backend;
        }

        ShaderFormat GetShaderFormat() const override
        {
            return _shaderFormat;
        }

        std::string GetDeviceName() const override
        {
            auto props = SDL_GetGPUDeviceProperties(_device);
            auto name = std::string(SDL_GetStringProperty(props, SDL_PROP_GPU_DEVICE_NAME_STRING, "Unknown"));
            return name + " (" + SDL_GetGPUDeviceDriver(_device) + ")";
        }

        std::unique_ptr<IBuffer> CreateBuffer(const BufferDesc& desc) override
        {
            // Names are passed at creation: SDL_SetGPUBufferName is not thread safe.
            auto createInfo = SDL_GPUBufferCreateInfo{};
            createInfo.usage = ToSDLBufferUsage(desc.Usage);
            createInfo.size = desc.Size;
            createInfo.props = CreateNameProperties(SDL_PROP_GPU_BUFFER_CREATE_NAME_STRING, desc.Name);

            auto* handle = SDL_CreateGPUBuffer(_device, &createInfo);
            DestroyNameProperties(createInfo.props);
            if (handle == nullptr)
            {
                TENLog("SDL GPU: cannot create buffer " + desc.Name + ": " + SDL_GetError(), LogLevel::Error);
                return nullptr;
            }

            auto buffer = std::make_unique<SDLGPUBuffer>();
            buffer->Device = _device;
            buffer->Handle = handle;
            buffer->Size = desc.Size;
            buffer->Usage = desc.Usage;
            return buffer;
        }

        std::unique_ptr<ITexture> CreateTexture(const TextureDesc& desc) override
        {
            auto createInfo = SDL_GPUTextureCreateInfo{};
            createInfo.type = ToSDL(desc.Type);
            createInfo.format = ToSDL(desc.Format);
            createInfo.usage = ToSDLTextureUsage(desc.Usage);
            createInfo.width = desc.Width;
            createInfo.height = desc.Height;
            createInfo.layer_count_or_depth = desc.DepthOrLayers;
            createInfo.num_levels = desc.MipLevels;
            createInfo.sample_count = ToSDLSampleCount(desc.SampleCount);
            createInfo.props = CreateNameProperties(SDL_PROP_GPU_TEXTURE_CREATE_NAME_STRING, desc.Name);

            auto* handle = SDL_CreateGPUTexture(_device, &createInfo);
            DestroyNameProperties(createInfo.props);
            if (handle == nullptr)
            {
                TENLog("SDL GPU: cannot create texture " + desc.Name + ": " + SDL_GetError(), LogLevel::Error);
                return nullptr;
            }

            auto texture = std::make_unique<SDLGPUTexture>();
            texture->Device = _device;
            texture->Handle = handle;
            texture->Desc = desc;
            return texture;
        }

        std::unique_ptr<ISampler> CreateSampler(const SamplerDesc& desc) override
        {
            auto createInfo = SDL_GPUSamplerCreateInfo{};
            createInfo.min_filter = (desc.MinFilter == Filter::Linear) ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
            createInfo.mag_filter = (desc.MagFilter == Filter::Linear) ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;
            createInfo.mipmap_mode = (desc.MipFilter == Filter::Linear) ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
            createInfo.address_mode_u = ToSDL(desc.AddressU);
            createInfo.address_mode_v = ToSDL(desc.AddressV);
            createInfo.address_mode_w = ToSDL(desc.AddressW);
            createInfo.mip_lod_bias = desc.MipLodBias;
            createInfo.max_anisotropy = (float)desc.MaxAnisotropy;
            createInfo.enable_anisotropy = desc.MaxAnisotropy > 1;
            createInfo.compare_op = ToSDL(desc.Compare);
            createInfo.enable_compare = desc.EnableCompare;
            createInfo.min_lod = desc.MinLod;
            createInfo.max_lod = desc.MaxLod;

            auto* handle = SDL_CreateGPUSampler(_device, &createInfo);
            if (handle == nullptr)
            {
                TENLog("SDL GPU: cannot create sampler " + desc.Name + ": " + SDL_GetError(), LogLevel::Error);
                return nullptr;
            }

            auto sampler = std::make_unique<SDLGPUSampler>();
            sampler->Device = _device;
            sampler->Handle = handle;
            return sampler;
        }

        std::unique_ptr<IShader> CreateShader(const ShaderDesc& desc) override
        {
            auto shader = std::make_unique<SDLGPUShader>();
            shader->Device = _device;
            shader->Desc = desc;
            shader->Code.assign(desc.Code, desc.Code + desc.CodeSize);
            shader->Desc.Code = shader->Code.data();

            if (desc.Stage == ShaderStage::Compute)
                return shader;

            auto createInfo = SDL_GPUShaderCreateInfo{};
            createInfo.code_size = desc.CodeSize;
            createInfo.code = desc.Code;
            createInfo.entrypoint = desc.EntryPoint.c_str();
            createInfo.format = (_shaderFormat == ShaderFormat::Dxil) ? SDL_GPU_SHADERFORMAT_DXIL : SDL_GPU_SHADERFORMAT_SPIRV;
            createInfo.stage = (desc.Stage == ShaderStage::Vertex) ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
            createInfo.num_samplers = desc.NumSamplers;
            createInfo.num_storage_textures = desc.NumStorageTextures;
            createInfo.num_storage_buffers = desc.NumStorageBuffers;
            createInfo.num_uniform_buffers = desc.NumUniformBuffers;

            shader->Handle = SDL_CreateGPUShader(_device, &createInfo);
            if (shader->Handle == nullptr)
            {
                TENLog("SDL GPU: cannot create shader " + desc.Name + ": " + SDL_GetError(), LogLevel::Error);
                return nullptr;
            }

            return shader;
        }

        std::unique_ptr<IGraphicsPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) override
        {
            SDL_GPUVertexBufferDescription buffers[MAX_VERTEX_BUFFERS] = {};
            for (int i = 0; i < desc.Layout.NumBuffers; i++)
            {
                const auto& buffer = desc.Layout.Buffers[i];
                buffers[i].slot = buffer.Slot;
                buffers[i].pitch = buffer.Stride;
                buffers[i].input_rate = (buffer.Rate == VertexInputRate::Instance) ? SDL_GPU_VERTEXINPUTRATE_INSTANCE : SDL_GPU_VERTEXINPUTRATE_VERTEX;
            }

            SDL_GPUVertexAttribute attributes[MAX_VERTEX_ATTRIBUTES] = {};
            for (int i = 0; i < desc.Layout.NumAttributes; i++)
            {
                const auto& attribute = desc.Layout.Attributes[i];
                attributes[i].location = attribute.Location;
                attributes[i].buffer_slot = attribute.BufferSlot;
                attributes[i].format = ToSDL(attribute.Format);
                attributes[i].offset = attribute.Offset;
            }

            SDL_GPUColorTargetDescription colorTargets[MAX_COLOR_TARGETS] = {};
            for (int i = 0; i < desc.NumColorTargets; i++)
            {
                const auto& target = desc.ColorTargets[i];
                colorTargets[i].format = ToSDL(target.Format);
                colorTargets[i].blend_state.enable_blend = target.Blend.EnableBlend;
                colorTargets[i].blend_state.src_color_blendfactor = ToSDL(target.Blend.SrcColor);
                colorTargets[i].blend_state.dst_color_blendfactor = ToSDL(target.Blend.DstColor);
                colorTargets[i].blend_state.color_blend_op = ToSDL(target.Blend.ColorOp);
                colorTargets[i].blend_state.src_alpha_blendfactor = ToSDL(target.Blend.SrcAlpha);
                colorTargets[i].blend_state.dst_alpha_blendfactor = ToSDL(target.Blend.DstAlpha);
                colorTargets[i].blend_state.alpha_blend_op = ToSDL(target.Blend.AlphaOp);
                colorTargets[i].blend_state.enable_color_write_mask = true;
                colorTargets[i].blend_state.color_write_mask = (SDL_GPUColorComponentFlags)target.Blend.WriteMask;
            }

            auto createInfo = SDL_GPUGraphicsPipelineCreateInfo{};
            createInfo.vertex_shader = ((SDLGPUShader*)desc.VertexShader)->Handle;
            createInfo.fragment_shader = ((SDLGPUShader*)desc.PixelShader)->Handle;
            createInfo.vertex_input_state.vertex_buffer_descriptions = buffers;
            createInfo.vertex_input_state.num_vertex_buffers = desc.Layout.NumBuffers;
            createInfo.vertex_input_state.vertex_attributes = attributes;
            createInfo.vertex_input_state.num_vertex_attributes = desc.Layout.NumAttributes;
            createInfo.primitive_type = ToSDL(desc.Primitive);

            auto& rasterizer = createInfo.rasterizer_state;
            rasterizer.fill_mode = (desc.Rasterizer.Fill == FillMode::Wireframe) ? SDL_GPU_FILLMODE_LINE : SDL_GPU_FILLMODE_FILL;
            rasterizer.cull_mode = (desc.Rasterizer.Cull == CullMode::None) ? SDL_GPU_CULLMODE_NONE : ((desc.Rasterizer.Cull == CullMode::Front) ? SDL_GPU_CULLMODE_FRONT : SDL_GPU_CULLMODE_BACK);
            rasterizer.front_face = (desc.Rasterizer.Front == FrontFace::Clockwise) ? SDL_GPU_FRONTFACE_CLOCKWISE : SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
            rasterizer.depth_bias_constant_factor = desc.Rasterizer.DepthBiasConstant;
            rasterizer.depth_bias_slope_factor = desc.Rasterizer.DepthBiasSlope;
            rasterizer.depth_bias_clamp = desc.Rasterizer.DepthBiasClamp;
            rasterizer.enable_depth_bias = desc.Rasterizer.EnableDepthBias;
            rasterizer.enable_depth_clip = desc.Rasterizer.EnableDepthClip;

            createInfo.multisample_state.sample_count = ToSDLSampleCount(desc.SampleCount);

            auto& depthStencil = createInfo.depth_stencil_state;
            depthStencil.enable_depth_test = desc.DepthStencil.EnableDepthTest;
            depthStencil.enable_depth_write = desc.DepthStencil.EnableDepthWrite;
            depthStencil.compare_op = ToSDL(desc.DepthStencil.DepthCompare);

            createInfo.target_info.color_target_descriptions = colorTargets;
            createInfo.target_info.num_color_targets = desc.NumColorTargets;
            createInfo.target_info.has_depth_stencil_target = desc.DepthFormat != PixelFormat::Unknown;
            createInfo.target_info.depth_stencil_format = ToSDL(desc.DepthFormat);

            auto* handle = SDL_CreateGPUGraphicsPipeline(_device, &createInfo);
            if (handle == nullptr)
            {
                TENLog("SDL GPU: cannot create graphics pipeline " + desc.Name + ": " + SDL_GetError(), LogLevel::Error);
                return nullptr;
            }

            auto pipeline = std::make_unique<SDLGPUGraphicsPipeline>();
            pipeline->Device = _device;
            pipeline->Handle = handle;
            return pipeline;
        }

        std::unique_ptr<IComputePipeline> CreateComputePipeline(const ComputePipelineDesc& desc) override
        {
            const auto& shaderDesc = desc.Shader->GetDesc();

            auto createInfo = SDL_GPUComputePipelineCreateInfo{};
            createInfo.code_size = shaderDesc.CodeSize;
            createInfo.code = shaderDesc.Code;
            createInfo.entrypoint = shaderDesc.EntryPoint.c_str();
            createInfo.format = (_shaderFormat == ShaderFormat::Dxil) ? SDL_GPU_SHADERFORMAT_DXIL : SDL_GPU_SHADERFORMAT_SPIRV;
            createInfo.num_samplers = shaderDesc.NumSamplers;
            createInfo.num_readonly_storage_textures = shaderDesc.NumStorageTextures;
            createInfo.num_readonly_storage_buffers = shaderDesc.NumStorageBuffers;
            createInfo.num_readwrite_storage_textures = shaderDesc.NumRWStorageTextures;
            createInfo.num_readwrite_storage_buffers = shaderDesc.NumRWStorageBuffers;
            createInfo.num_uniform_buffers = shaderDesc.NumUniformBuffers;
            createInfo.threadcount_x = shaderDesc.ThreadCountX;
            createInfo.threadcount_y = shaderDesc.ThreadCountY;
            createInfo.threadcount_z = shaderDesc.ThreadCountZ;

            auto* handle = SDL_CreateGPUComputePipeline(_device, &createInfo);
            if (handle == nullptr)
            {
                TENLog("SDL GPU: cannot create compute pipeline " + desc.Name + ": " + SDL_GetError(), LogLevel::Error);
                return nullptr;
            }

            auto pipeline = std::make_unique<SDLGPUComputePipeline>();
            pipeline->Device = _device;
            pipeline->Handle = handle;
            return pipeline;
        }

        std::unique_ptr<ITransferBuffer> CreateTransferBuffer(TransferBufferUsage usage, int size) override
        {
            auto createInfo = SDL_GPUTransferBufferCreateInfo{};
            createInfo.usage = (usage == TransferBufferUsage::Upload) ? SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD : SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD;
            createInfo.size = size;

            auto* handle = SDL_CreateGPUTransferBuffer(_device, &createInfo);
            if (handle == nullptr)
            {
                TENLog(std::string("SDL GPU: cannot create transfer buffer: ") + SDL_GetError(), LogLevel::Error);
                return nullptr;
            }

            auto buffer = std::make_unique<SDLGPUTransferBuffer>();
            buffer->Device = _device;
            buffer->Handle = handle;
            buffer->Size = size;
            return buffer;
        }

        void* MapTransferBuffer(ITransferBuffer* buffer, bool cycle) override
        {
            return SDL_MapGPUTransferBuffer(_device, GetHandle(buffer), cycle);
        }

        void UnmapTransferBuffer(ITransferBuffer* buffer) override
        {
            SDL_UnmapGPUTransferBuffer(_device, GetHandle(buffer));
        }

        void GenerateMipmaps(ICommandBuffer* commandBuffer, ITexture* texture) override
        {
            SDL_GenerateMipmapsForGPUTexture(((SDLGPUCommandBuffer*)commandBuffer)->Handle, GetHandle(texture));
        }

        // Command buffers can be recorded on different threads, so acquisition picks a wrapper not in use.
        ICommandBuffer* AcquireCommandBuffer() override
        {
            auto lock = std::lock_guard<std::mutex>(_commandBufferMutex);

            SDLGPUCommandBuffer* commandBuffer = nullptr;
            for (auto& candidate : _commandBuffers)
            {
                if (!candidate->IsInUse)
                {
                    commandBuffer = candidate.get();
                    break;
                }
            }

            if (commandBuffer == nullptr)
            {
                TENLog("SDL GPU: too many command buffers recorded at once.", LogLevel::Error);
                return nullptr;
            }

            commandBuffer->Handle = SDL_AcquireGPUCommandBuffer(_device);
            if (commandBuffer->Handle == nullptr)
            {
                TENLog(std::string("SDL GPU: cannot acquire command buffer: ") + SDL_GetError(), LogLevel::Error);
                return nullptr;
            }

            commandBuffer->IsInUse = true;

            return commandBuffer;
        }

        PixelFormat GetSwapchainFormat() const override
        {
            return FromSDL(SDL_GetGPUSwapchainTextureFormat(_device, _window));
        }

        void SetPresentMode(PresentMode mode) override
        {
            auto sdlMode = SDL_GPU_PRESENTMODE_VSYNC;
            if (mode == PresentMode::Immediate && SDL_WindowSupportsGPUPresentMode(_device, _window, SDL_GPU_PRESENTMODE_IMMEDIATE))
                sdlMode = SDL_GPU_PRESENTMODE_IMMEDIATE;
            else if (mode == PresentMode::Mailbox && SDL_WindowSupportsGPUPresentMode(_device, _window, SDL_GPU_PRESENTMODE_MAILBOX))
                sdlMode = SDL_GPU_PRESENTMODE_MAILBOX;

            if (!SDL_SetGPUSwapchainParameters(_device, _window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR, sdlMode))
                TENLog(std::string("SDL GPU: cannot set present mode: ") + SDL_GetError(), LogLevel::Warning);

            _presentMode = mode;
        }

        void ResizeSwapchain(int width, int height) override
        {
            // SDL GPU recreates the swapchain on the next acquisition after a window resize.
        }

        void WaitIdle() override
        {
            SDL_WaitForGPUIdle(_device);
        }

        bool SupportsTextureFormat(PixelFormat format, TextureType type, unsigned int usage) const override
        {
            return SDL_GPUTextureSupportsFormat(_device, ToSDL(format), ToSDL(type), ToSDLTextureUsage(usage));
        }

        bool RequiresWindowThreadPresent() const override
        {
            return true;
        }
    };

    std::unique_ptr<IDevice> CreateSDLGPUDevice(SDL_Window* window, const char* driverName, bool debugMode)
    {
        bool isDirect3D12 = (std::string(driverName) == "direct3d12");
        auto shaderFormat = isDirect3D12 ? ShaderFormat::Dxil : ShaderFormat::Spirv;
        auto sdlShaderFormat = isDirect3D12 ? SDL_GPU_SHADERFORMAT_DXIL : SDL_GPU_SHADERFORMAT_SPIRV;

        if (!SDL_GPUSupportsShaderFormats(sdlShaderFormat, driverName))
        {
            TENLog(std::string("SDL GPU: driver ") + driverName + " is not available.", LogLevel::Warning);
            return nullptr;
        }

        auto* device = SDL_CreateGPUDevice(sdlShaderFormat, debugMode, driverName);
        if (device == nullptr)
        {
            TENLog(std::string("SDL GPU: cannot create device on ") + driverName + ": " + SDL_GetError(), LogLevel::Warning);
            return nullptr;
        }

        if (!SDL_ClaimWindowForGPUDevice(device, window))
        {
            TENLog(std::string("SDL GPU: cannot claim window: ") + SDL_GetError(), LogLevel::Error);
            SDL_DestroyGPUDevice(device);
            return nullptr;
        }

        auto backend = isDirect3D12 ? Backend::Direct3D12 : Backend::Vulkan;
        auto result = std::make_unique<SDLGPUDevice>(device, window, backend, shaderFormat);
        TENLog("SDL GPU device created: " + result->GetDeviceName(), LogLevel::Info);
        return result;
    }
}
