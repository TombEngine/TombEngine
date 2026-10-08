#include "framework.h"
#include "Renderer/RHI/DirectX11/DX11Device.h"

#ifdef SDL_PLATFORM_WIN32

#include <d3d11_1.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include "Specific/trutils.h"

using Microsoft::WRL::ComPtr;

namespace TEN::Renderer::RHI::DirectX11
{
    constexpr auto UNIFORM_BUFFER_SIZE   = 16384;
    constexpr auto SWAPCHAIN_BUFFER_COUNT = 2;
    constexpr auto MAX_SRV_SLOTS         = 32;

    static DXGI_FORMAT ToDXGI(PixelFormat format)
    {
        switch (format)
        {
        case PixelFormat::R8Unorm:        return DXGI_FORMAT_R8_UNORM;
        case PixelFormat::RG8Unorm:       return DXGI_FORMAT_R8G8_UNORM;
        case PixelFormat::RGBA8Unorm:     return DXGI_FORMAT_R8G8B8A8_UNORM;
        case PixelFormat::RGBA8UnormSrgb: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        case PixelFormat::BGRA8Unorm:     return DXGI_FORMAT_B8G8R8A8_UNORM;
        case PixelFormat::BGRA8UnormSrgb: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        case PixelFormat::R16Float:       return DXGI_FORMAT_R16_FLOAT;
        case PixelFormat::RG16Float:      return DXGI_FORMAT_R16G16_FLOAT;
        case PixelFormat::RGBA16Float:    return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case PixelFormat::R32Float:       return DXGI_FORMAT_R32_FLOAT;
        case PixelFormat::RG32Float:      return DXGI_FORMAT_R32G32_FLOAT;
        case PixelFormat::RGBA32Float:    return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case PixelFormat::R11G11B10Float: return DXGI_FORMAT_R11G11B10_FLOAT;
        case PixelFormat::R32Uint:        return DXGI_FORMAT_R32_UINT;
        case PixelFormat::BC1Unorm:       return DXGI_FORMAT_BC1_UNORM;
        case PixelFormat::BC2Unorm:       return DXGI_FORMAT_BC2_UNORM;
        case PixelFormat::BC3Unorm:       return DXGI_FORMAT_BC3_UNORM;
        case PixelFormat::BC4Unorm:       return DXGI_FORMAT_BC4_UNORM;
        case PixelFormat::BC5Unorm:       return DXGI_FORMAT_BC5_UNORM;
        case PixelFormat::BC7Unorm:       return DXGI_FORMAT_BC7_UNORM;
        case PixelFormat::D16Unorm:       return DXGI_FORMAT_D16_UNORM;
        case PixelFormat::D24UnormS8Uint: return DXGI_FORMAT_D24_UNORM_S8_UINT;
        case PixelFormat::D32Float:       return DXGI_FORMAT_D32_FLOAT;
        default:                          return DXGI_FORMAT_UNKNOWN;
        }
    }

    // Sampled depth textures need a typeless resource with separate DSV and SRV formats.
    static DXGI_FORMAT ToDXGIResourceFormat(PixelFormat format, bool isSampled)
    {
        if (!isSampled)
            return ToDXGI(format);

        switch (format)
        {
        case PixelFormat::D16Unorm:       return DXGI_FORMAT_R16_TYPELESS;
        case PixelFormat::D24UnormS8Uint: return DXGI_FORMAT_R24G8_TYPELESS;
        case PixelFormat::D32Float:       return DXGI_FORMAT_R32_TYPELESS;
        default:                          return ToDXGI(format);
        }
    }

    static DXGI_FORMAT ToDXGIShaderResourceFormat(PixelFormat format)
    {
        switch (format)
        {
        case PixelFormat::D16Unorm:       return DXGI_FORMAT_R16_UNORM;
        case PixelFormat::D24UnormS8Uint: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
        case PixelFormat::D32Float:       return DXGI_FORMAT_R32_FLOAT;
        default:                          return ToDXGI(format);
        }
    }

    static DXGI_FORMAT ToDXGI(VertexElementFormat format)
    {
        switch (format)
        {
        case VertexElementFormat::Float:      return DXGI_FORMAT_R32_FLOAT;
        case VertexElementFormat::Float2:     return DXGI_FORMAT_R32G32_FLOAT;
        case VertexElementFormat::Float3:     return DXGI_FORMAT_R32G32B32_FLOAT;
        case VertexElementFormat::Float4:     return DXGI_FORMAT_R32G32B32A32_FLOAT;
        case VertexElementFormat::UByte4:     return DXGI_FORMAT_R8G8B8A8_UINT;
        case VertexElementFormat::UByte4Norm: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case VertexElementFormat::Byte4Norm:  return DXGI_FORMAT_R8G8B8A8_SNORM;
        case VertexElementFormat::Short2Norm: return DXGI_FORMAT_R16G16_SNORM;
        case VertexElementFormat::Short4Norm: return DXGI_FORMAT_R16G16B16A16_SNORM;
        case VertexElementFormat::Half2:      return DXGI_FORMAT_R16G16_FLOAT;
        case VertexElementFormat::Half4:      return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case VertexElementFormat::UInt:       return DXGI_FORMAT_R32_UINT;
        default:                              return DXGI_FORMAT_UNKNOWN;
        }
    }

    static D3D11_COMPARISON_FUNC ToD3D11(CompareOp op)
    {
        switch (op)
        {
        case CompareOp::Never:          return D3D11_COMPARISON_NEVER;
        case CompareOp::Less:           return D3D11_COMPARISON_LESS;
        case CompareOp::Equal:          return D3D11_COMPARISON_EQUAL;
        case CompareOp::LessOrEqual:    return D3D11_COMPARISON_LESS_EQUAL;
        case CompareOp::Greater:        return D3D11_COMPARISON_GREATER;
        case CompareOp::NotEqual:       return D3D11_COMPARISON_NOT_EQUAL;
        case CompareOp::GreaterOrEqual: return D3D11_COMPARISON_GREATER_EQUAL;
        default:                        return D3D11_COMPARISON_ALWAYS;
        }
    }

    static D3D11_BLEND ToD3D11(BlendFactor factor)
    {
        switch (factor)
        {
        case BlendFactor::Zero:             return D3D11_BLEND_ZERO;
        case BlendFactor::One:              return D3D11_BLEND_ONE;
        case BlendFactor::SrcColor:         return D3D11_BLEND_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor: return D3D11_BLEND_INV_SRC_COLOR;
        case BlendFactor::DstColor:         return D3D11_BLEND_DEST_COLOR;
        case BlendFactor::OneMinusDstColor: return D3D11_BLEND_INV_DEST_COLOR;
        case BlendFactor::SrcAlpha:         return D3D11_BLEND_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha: return D3D11_BLEND_INV_SRC_ALPHA;
        case BlendFactor::DstAlpha:         return D3D11_BLEND_DEST_ALPHA;
        case BlendFactor::OneMinusDstAlpha: return D3D11_BLEND_INV_DEST_ALPHA;
        default:                            return D3D11_BLEND_ONE;
        }
    }

    // Alpha blend factors cannot reference color channels in D3D11.
    static D3D11_BLEND ToD3D11Alpha(BlendFactor factor)
    {
        switch (factor)
        {
        case BlendFactor::SrcColor:         return D3D11_BLEND_SRC_ALPHA;
        case BlendFactor::OneMinusSrcColor: return D3D11_BLEND_INV_SRC_ALPHA;
        case BlendFactor::DstColor:         return D3D11_BLEND_DEST_ALPHA;
        case BlendFactor::OneMinusDstColor: return D3D11_BLEND_INV_DEST_ALPHA;
        default:                            return ToD3D11(factor);
        }
    }

    static D3D11_BLEND_OP ToD3D11(BlendOp op)
    {
        switch (op)
        {
        case BlendOp::Subtract:        return D3D11_BLEND_OP_SUBTRACT;
        case BlendOp::ReverseSubtract: return D3D11_BLEND_OP_REV_SUBTRACT;
        case BlendOp::Min:             return D3D11_BLEND_OP_MIN;
        case BlendOp::Max:             return D3D11_BLEND_OP_MAX;
        default:                       return D3D11_BLEND_OP_ADD;
        }
    }

    static D3D11_TEXTURE_ADDRESS_MODE ToD3D11(AddressMode mode)
    {
        switch (mode)
        {
        case AddressMode::MirroredRepeat: return D3D11_TEXTURE_ADDRESS_MIRROR;
        case AddressMode::ClampToEdge:    return D3D11_TEXTURE_ADDRESS_CLAMP;
        default:                          return D3D11_TEXTURE_ADDRESS_WRAP;
        }
    }

    static D3D11_PRIMITIVE_TOPOLOGY ToD3D11(PrimitiveType type)
    {
        switch (type)
        {
        case PrimitiveType::TriangleStrip: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
        case PrimitiveType::LineList:      return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
        case PrimitiveType::LineStrip:     return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
        case PrimitiveType::PointList:     return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
        default:                           return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        }
    }

    static D3D11_FILTER ToD3D11Filter(const SamplerDesc& desc)
    {
        if (desc.MaxAnisotropy > 1)
            return desc.EnableCompare ? D3D11_FILTER_COMPARISON_ANISOTROPIC : D3D11_FILTER_ANISOTROPIC;

        int filter = 0;
        if (desc.MipFilter == Filter::Linear)
            filter |= 0x1;

        if (desc.MagFilter == Filter::Linear)
            filter |= 0x4;

        if (desc.MinFilter == Filter::Linear)
            filter |= 0x10;

        if (desc.EnableCompare)
            filter |= 0x80;

        return (D3D11_FILTER)filter;
    }

    static void SetDebugName(ID3D11DeviceChild* object, const std::string& name)
    {
        if (object != nullptr && !name.empty())
            object->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)name.size(), name.c_str());
    }

    class DX11Buffer : public IBuffer
    {
    public:
        ComPtr<ID3D11Buffer>              Handle = nullptr;
        ComPtr<ID3D11ShaderResourceView>  Srv    = nullptr;
        ComPtr<ID3D11UnorderedAccessView> Uav    = nullptr;
        int                               Size   = 0;
        unsigned int                      Usage  = 0;

        int          GetSize() const override { return Size; }
        unsigned int GetUsage() const override { return Usage; }
    };

    class DX11Texture : public ITexture
    {
    public:
        ComPtr<ID3D11Resource>            Handle = nullptr;
        ComPtr<ID3D11ShaderResourceView>  Srv    = nullptr;
        ComPtr<ID3D11UnorderedAccessView> Uav    = nullptr;
        TextureDesc                       Desc   = {};

        // Views for each (mip, layer) used as a target, created on first use.
        std::unordered_map<int, ComPtr<ID3D11RenderTargetView>> Rtvs = {};
        std::unordered_map<int, ComPtr<ID3D11DepthStencilView>> Dsvs = {};

        const TextureDesc& GetDesc() const override { return Desc; }
    };

    class DX11Sampler : public ISampler
    {
    public:
        ComPtr<ID3D11SamplerState> Handle = nullptr;
    };

    class DX11Shader : public IShader
    {
    public:
        ComPtr<ID3D11VertexShader>  VertexShader  = nullptr;
        ComPtr<ID3D11PixelShader>   PixelShader   = nullptr;
        ComPtr<ID3D11ComputeShader> ComputeShader = nullptr;
        std::vector<unsigned char>  Bytecode      = {};
        ShaderDesc                  Desc          = {};

        const ShaderDesc& GetDesc() const override { return Desc; }
    };

    class DX11GraphicsPipeline : public IGraphicsPipeline
    {
    public:
        ComPtr<ID3D11VertexShader>      VertexShader = nullptr;
        ComPtr<ID3D11PixelShader>       PixelShader  = nullptr;
        ComPtr<ID3D11InputLayout>       InputLayout  = nullptr;
        ComPtr<ID3D11BlendState>        BlendState   = nullptr;
        ComPtr<ID3D11RasterizerState>   Rasterizer   = nullptr;
        ComPtr<ID3D11DepthStencilState> DepthStencil = nullptr;
        D3D11_PRIMITIVE_TOPOLOGY        Topology     = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        int                             VertexStrides[MAX_VERTEX_BUFFERS] = {};

        // Storage buffers follow sampled and storage textures in the t register range.
        int VertexStorageBufferBase = 0;
        int PixelStorageBufferBase  = 0;
    };

    class DX11ComputePipeline : public IComputePipeline
    {
    public:
        ComPtr<ID3D11ComputeShader> Shader            = nullptr;
        int                         StorageBufferBase = 0;
    };

    // DX11 copies upload data at call time, so a transfer buffer is plain CPU memory.
    class DX11TransferBuffer : public ITransferBuffer
    {
    public:
        std::vector<unsigned char> Data = {};

        int GetSize() const override { return (int)Data.size(); }
    };

    static ID3D11ShaderResourceView* GetSrv(ITexture* texture)
    {
        return (texture != nullptr) ? ((DX11Texture*)texture)->Srv.Get() : nullptr;
    }

    static ID3D11ShaderResourceView* GetSrv(IBuffer* buffer)
    {
        return (buffer != nullptr) ? ((DX11Buffer*)buffer)->Srv.Get() : nullptr;
    }

    static ID3D11SamplerState* GetHandle(ISampler* sampler)
    {
        return (sampler != nullptr) ? ((DX11Sampler*)sampler)->Handle.Get() : nullptr;
    }

    class DX11CommandBuffer;

    class DX11RenderPass : public IRenderPass
    {
    public:
        ID3D11DeviceContext*  Context  = nullptr;
        DX11GraphicsPipeline* Pipeline = nullptr;

        void BindPipeline(IGraphicsPipeline* pipeline) override
        {
            auto* dxPipeline = (DX11GraphicsPipeline*)pipeline;
            if (dxPipeline == Pipeline)
                return;

            Pipeline = dxPipeline;
            Context->IASetInputLayout(dxPipeline->InputLayout.Get());
            Context->IASetPrimitiveTopology(dxPipeline->Topology);
            Context->VSSetShader(dxPipeline->VertexShader.Get(), nullptr, 0);
            Context->PSSetShader(dxPipeline->PixelShader.Get(), nullptr, 0);
            Context->OMSetBlendState(dxPipeline->BlendState.Get(), nullptr, 0xFFFFFFFF);
            Context->OMSetDepthStencilState(dxPipeline->DepthStencil.Get(), 0);
            Context->RSSetState(dxPipeline->Rasterizer.Get());
        }

        void SetViewport(const Viewport& viewport) override
        {
            auto d3dViewport = D3D11_VIEWPORT{ viewport.X, viewport.Y, viewport.Width, viewport.Height, viewport.MinDepth, viewport.MaxDepth };
            Context->RSSetViewports(1, &d3dViewport);
        }

        void SetScissor(const ScissorRect& scissor) override
        {
            auto rect = D3D11_RECT{ scissor.X, scissor.Y, scissor.X + scissor.Width, scissor.Y + scissor.Height };
            Context->RSSetScissorRects(1, &rect);
        }

        void BindVertexBuffers(int firstSlot, const BufferBinding* bindings, int count) override
        {
            ID3D11Buffer* buffers[MAX_VERTEX_BUFFERS] = {};
            UINT strides[MAX_VERTEX_BUFFERS] = {};
            UINT offsets[MAX_VERTEX_BUFFERS] = {};

            for (int i = 0; i < count; i++)
            {
                buffers[i] = ((DX11Buffer*)bindings[i].Buffer)->Handle.Get();
                strides[i] = (Pipeline != nullptr) ? Pipeline->VertexStrides[firstSlot + i] : 0;
                offsets[i] = bindings[i].Offset;
            }

            Context->IASetVertexBuffers(firstSlot, count, buffers, strides, offsets);
        }

        void BindIndexBuffer(const BufferBinding& binding, IndexSize indexSize) override
        {
            auto format = (indexSize == IndexSize::Bits16) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
            Context->IASetIndexBuffer(((DX11Buffer*)binding.Buffer)->Handle.Get(), format, binding.Offset);
        }

        void BindVertexSamplers(int firstSlot, const TextureSamplerBinding* bindings, int count) override
        {
            ID3D11ShaderResourceView* views[MAX_SAMPLER_SLOTS];
            ID3D11SamplerState* samplers[MAX_SAMPLER_SLOTS];
            for (int i = 0; i < count; i++)
            {
                views[i] = GetSrv(bindings[i].Texture);
                samplers[i] = GetHandle(bindings[i].Sampler);
            }

            Context->VSSetShaderResources(firstSlot, count, views);
            Context->VSSetSamplers(firstSlot, count, samplers);
        }

        void BindVertexStorageBuffers(int firstSlot, IBuffer* const* buffers, int count) override
        {
            ID3D11ShaderResourceView* views[MAX_STORAGE_SLOTS];
            for (int i = 0; i < count; i++)
                views[i] = GetSrv(buffers[i]);

            int base = (Pipeline != nullptr) ? Pipeline->VertexStorageBufferBase : 0;
            Context->VSSetShaderResources(base + firstSlot, count, views);
        }

        void BindPixelSamplers(int firstSlot, const TextureSamplerBinding* bindings, int count) override
        {
            ID3D11ShaderResourceView* views[MAX_SAMPLER_SLOTS];
            ID3D11SamplerState* samplers[MAX_SAMPLER_SLOTS];
            for (int i = 0; i < count; i++)
            {
                views[i] = GetSrv(bindings[i].Texture);
                samplers[i] = GetHandle(bindings[i].Sampler);
            }

            Context->PSSetShaderResources(firstSlot, count, views);
            Context->PSSetSamplers(firstSlot, count, samplers);
        }

        void BindPixelStorageBuffers(int firstSlot, IBuffer* const* buffers, int count) override
        {
            ID3D11ShaderResourceView* views[MAX_STORAGE_SLOTS];
            for (int i = 0; i < count; i++)
                views[i] = GetSrv(buffers[i]);

            int base = (Pipeline != nullptr) ? Pipeline->PixelStorageBufferBase : 0;
            Context->PSSetShaderResources(base + firstSlot, count, views);
        }

        void Draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance) override
        {
            Context->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
        }

        void DrawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance) override
        {
            Context->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
        }
    };

    class DX11CopyPass : public ICopyPass
    {
    public:
        ID3D11Device*        Device  = nullptr;
        ID3D11DeviceContext* Context = nullptr;

        void UploadToBuffer(ITransferBuffer* source, int sourceOffset, IBuffer* destination, int destinationOffset, int size, bool cycle) override
        {
            const auto& data = ((DX11TransferBuffer*)source)->Data;
            auto box = D3D11_BOX{ (UINT)destinationOffset, 0, 0, (UINT)(destinationOffset + size), 1, 1 };
            Context->UpdateSubresource(((DX11Buffer*)destination)->Handle.Get(), 0, &box, data.data() + sourceOffset, 0, 0);
        }

        void UploadToTexture(const TextureTransferInfo& source, const TextureRegion& destination, bool cycle) override
        {
            auto* texture = (DX11Texture*)destination.Texture;
            const auto& desc = texture->Desc;
            const auto& data = ((DX11TransferBuffer*)source.TransferBuffer)->Data;

            int pixelsPerRow = (source.PixelsPerRow > 0) ? source.PixelsPerRow : destination.Width;
            int rowsPerLayer = (source.RowsPerLayer > 0) ? source.RowsPerLayer : destination.Height;
            int blockDimension = GetFormatBlockDimension(desc.Format);
            int rowPitch = ((pixelsPerRow + blockDimension - 1) / blockDimension) * GetFormatBlockSize(desc.Format);
            int depthPitch = rowPitch * ((rowsPerLayer + blockDimension - 1) / blockDimension);

            bool is3D = (desc.Type == TextureType::Texture3D);
            int arraySlice = is3D ? 0 : destination.Layer;
            int subresource = D3D11CalcSubresource(destination.MipLevel, arraySlice, desc.MipLevels);
            // Block compressed mips smaller than a block still cover a whole block.
            int alignedWidth = ((destination.Width + blockDimension - 1) / blockDimension) * blockDimension;
            int alignedHeight = ((destination.Height + blockDimension - 1) / blockDimension) * blockDimension;
            auto box = D3D11_BOX{ (UINT)destination.X, (UINT)destination.Y, (UINT)destination.Z,
                (UINT)(destination.X + alignedWidth), (UINT)(destination.Y + alignedHeight), (UINT)(destination.Z + destination.Depth) };

            Context->UpdateSubresource(texture->Handle.Get(), subresource, &box, data.data() + source.Offset, rowPitch, depthPitch);
        }

        void CopyTexture(const TextureRegion& source, const TextureRegion& destination) override
        {
            auto* sourceTexture = (DX11Texture*)source.Texture;
            auto* destinationTexture = (DX11Texture*)destination.Texture;

            int sourceSubresource = D3D11CalcSubresource(source.MipLevel, source.Layer, sourceTexture->Desc.MipLevels);
            int destinationSubresource = D3D11CalcSubresource(destination.MipLevel, destination.Layer, destinationTexture->Desc.MipLevels);
            auto box = D3D11_BOX{ (UINT)source.X, (UINT)source.Y, (UINT)source.Z,
                (UINT)(source.X + source.Width), (UINT)(source.Y + source.Height), (UINT)(source.Z + source.Depth) };

            Context->CopySubresourceRegion(destinationTexture->Handle.Get(), destinationSubresource, destination.X, destination.Y, destination.Z,
                sourceTexture->Handle.Get(), sourceSubresource, &box);
        }

        // Synchronous readback through a staging copy; data is available as soon as this returns.
        void DownloadFromTexture(const TextureRegion& source, const TextureTransferInfo& destination) override
        {
            auto* texture = (DX11Texture*)source.Texture;
            const auto& desc = texture->Desc;
            auto& data = ((DX11TransferBuffer*)destination.TransferBuffer)->Data;

            auto stagingDesc = D3D11_TEXTURE2D_DESC{};
            stagingDesc.Width = source.Width;
            stagingDesc.Height = source.Height;
            stagingDesc.MipLevels = 1;
            stagingDesc.ArraySize = 1;
            stagingDesc.Format = ToDXGIShaderResourceFormat(desc.Format);
            stagingDesc.SampleDesc.Count = 1;
            stagingDesc.Usage = D3D11_USAGE_STAGING;
            stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

            ComPtr<ID3D11Texture2D> staging;
            if (FAILED(Device->CreateTexture2D(&stagingDesc, nullptr, staging.GetAddressOf())))
            {
                TENLog("DX11: cannot create staging texture for readback.", LogLevel::Error);
                return;
            }

            int subresource = D3D11CalcSubresource(source.MipLevel, source.Layer, desc.MipLevels);
            auto box = D3D11_BOX{ (UINT)source.X, (UINT)source.Y, 0, (UINT)(source.X + source.Width), (UINT)(source.Y + source.Height), 1 };
            Context->CopySubresourceRegion(staging.Get(), 0, 0, 0, 0, texture->Handle.Get(), subresource, &box);

            auto mapped = D3D11_MAPPED_SUBRESOURCE{};
            if (FAILED(Context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
                return;

            int rowSize = GetTextureDataSize(desc.Format, source.Width, 1, 1);
            for (int y = 0; y < source.Height; y++)
            {
                int offset = destination.Offset + (y * rowSize);
                if (offset + rowSize > (int)data.size())
                    break;

                memcpy(data.data() + offset, (unsigned char*)mapped.pData + (y * mapped.RowPitch), rowSize);
            }

            Context->Unmap(staging.Get(), 0);
        }
    };

    class DX11ComputePass : public IComputePass
    {
    public:
        ID3D11DeviceContext* Context  = nullptr;
        DX11ComputePipeline* Pipeline = nullptr;

        void BindPipeline(IComputePipeline* pipeline) override
        {
            Pipeline = (DX11ComputePipeline*)pipeline;
            Context->CSSetShader(Pipeline->Shader.Get(), nullptr, 0);
        }

        void BindSamplers(int firstSlot, const TextureSamplerBinding* bindings, int count) override
        {
            ID3D11ShaderResourceView* views[MAX_SAMPLER_SLOTS];
            ID3D11SamplerState* samplers[MAX_SAMPLER_SLOTS];
            for (int i = 0; i < count; i++)
            {
                views[i] = GetSrv(bindings[i].Texture);
                samplers[i] = GetHandle(bindings[i].Sampler);
            }

            Context->CSSetShaderResources(firstSlot, count, views);
            Context->CSSetSamplers(firstSlot, count, samplers);
        }

        void BindStorageBuffers(int firstSlot, IBuffer* const* buffers, int count) override
        {
            ID3D11ShaderResourceView* views[MAX_STORAGE_SLOTS];
            for (int i = 0; i < count; i++)
                views[i] = GetSrv(buffers[i]);

            int base = (Pipeline != nullptr) ? Pipeline->StorageBufferBase : 0;
            Context->CSSetShaderResources(base + firstSlot, count, views);
        }

        void Dispatch(int groupCountX, int groupCountY, int groupCountZ) override
        {
            Context->Dispatch(groupCountX, groupCountY, groupCountZ);
        }
    };

    class DX11Device;

    class DX11CommandBuffer : public ICommandBuffer
    {
    public:
        DX11Device*                          Owner      = nullptr;
        ID3D11Device*                        Device     = nullptr;
        ID3D11DeviceContext*                 Context    = nullptr;
        ComPtr<ID3DUserDefinedAnnotation>    Annotation = nullptr;
        ComPtr<ID3D11Buffer>                 UniformBuffers[3][MAX_UNIFORM_SLOTS] = {};
        DX11RenderPass                       RenderPass  = {};
        DX11CopyPass                         CopyPass    = {};
        DX11ComputePass                      ComputePass = {};
        bool                                 HasSwapchainTexture = false;
        int                                  NumComputeUavs = 0;

        ITexture* AcquireSwapchainTexture() override;
        void      Submit() override;

        IRenderPass* BeginRenderPass(const ColorTargetInfo* colorTargets, int colorTargetCount, const DepthTargetInfo* depthTarget) override
        {
            // Unbind every shader resource first, so a texture written by this pass is not still bound as input.
            ID3D11ShaderResourceView* nullViews[MAX_SRV_SLOTS] = {};
            Context->VSSetShaderResources(0, MAX_SRV_SLOTS, nullViews);
            Context->PSSetShaderResources(0, MAX_SRV_SLOTS, nullViews);

            ID3D11RenderTargetView* rtvs[MAX_COLOR_TARGETS] = {};
            int width = 0;
            int height = 0;

            for (int i = 0; i < colorTargetCount; i++)
            {
                const auto& target = colorTargets[i];
                rtvs[i] = GetRenderTargetView((DX11Texture*)target.Texture, target.MipLevel, target.Layer);

                if (target.Load == LoadOp::Clear)
                    Context->ClearRenderTargetView(rtvs[i], target.ClearColor);

                const auto& desc = target.Texture->GetDesc();
                width = std::max(1, desc.Width >> target.MipLevel);
                height = std::max(1, desc.Height >> target.MipLevel);
            }

            ID3D11DepthStencilView* dsv = nullptr;
            if (depthTarget != nullptr)
            {
                dsv = GetDepthStencilView((DX11Texture*)depthTarget->Texture, depthTarget->Layer);
                if (depthTarget->Load == LoadOp::Clear)
                    Context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depthTarget->ClearDepth, depthTarget->ClearStencil);

                if (colorTargetCount == 0)
                {
                    width = depthTarget->Texture->GetDesc().Width;
                    height = depthTarget->Texture->GetDesc().Height;
                }
            }

            Context->OMSetRenderTargets(colorTargetCount, rtvs, dsv);

            // Match SDL GPU, which resets viewport and scissor to the full target at pass start.
            auto viewport = D3D11_VIEWPORT{ 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
            auto scissor = D3D11_RECT{ 0, 0, width, height };
            Context->RSSetViewports(1, &viewport);
            Context->RSSetScissorRects(1, &scissor);

            RenderPass.Context = Context;
            RenderPass.Pipeline = nullptr;
            return &RenderPass;
        }

        void EndRenderPass() override
        {
            Context->OMSetRenderTargets(0, nullptr, nullptr);
            RenderPass.Pipeline = nullptr;
        }

        ICopyPass* BeginCopyPass() override
        {
            CopyPass.Device = Device;
            CopyPass.Context = Context;
            return &CopyPass;
        }

        void EndCopyPass() override {}

        IComputePass* BeginComputePass(const ComputePassDesc& desc) override
        {
            ID3D11UnorderedAccessView* uavs[MAX_STORAGE_SLOTS * 2] = {};
            int count = 0;

            for (int i = 0; i < desc.NumRWTextures; i++)
                uavs[count++] = ((DX11Texture*)desc.RWTextures[i])->Uav.Get();

            for (int i = 0; i < desc.NumRWBuffers; i++)
                uavs[count++] = ((DX11Buffer*)desc.RWBuffers[i])->Uav.Get();

            Context->CSSetUnorderedAccessViews(0, count, uavs, nullptr);
            NumComputeUavs = count;

            ComputePass.Context = Context;
            ComputePass.Pipeline = nullptr;
            return &ComputePass;
        }

        void EndComputePass() override
        {
            ID3D11UnorderedAccessView* nullUavs[MAX_STORAGE_SLOTS * 2] = {};
            Context->CSSetUnorderedAccessViews(0, NumComputeUavs, nullUavs, nullptr);

            ID3D11ShaderResourceView* nullViews[MAX_SRV_SLOTS] = {};
            Context->CSSetShaderResources(0, MAX_SRV_SLOTS, nullViews);
            NumComputeUavs = 0;
        }

        void PushUniforms(ShaderStage stage, int slot, const void* data, int size) override
        {
            if (size > UNIFORM_BUFFER_SIZE)
            {
                TENLog("DX11: uniform push of " + std::to_string(size) + " bytes exceeds the uniform buffer size.", LogLevel::Error);
                return;
            }

            auto& buffer = UniformBuffers[(int)stage][slot];
            auto mapped = D3D11_MAPPED_SUBRESOURCE{};
            if (FAILED(Context->Map(buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
                return;

            memcpy(mapped.pData, data, size);
            Context->Unmap(buffer.Get(), 0);

            auto* handle = buffer.Get();
            switch (stage)
            {
            case ShaderStage::Vertex:
                Context->VSSetConstantBuffers(slot, 1, &handle);
                break;

            case ShaderStage::Pixel:
                Context->PSSetConstantBuffers(slot, 1, &handle);
                break;

            case ShaderStage::Compute:
                Context->CSSetConstantBuffers(slot, 1, &handle);
                break;
            }
        }

        // Same-size copy only; the new renderer keeps its offscreen target at swapchain size.
        void BlitTexture(ITexture* source, ITexture* destination) override
        {
            const auto& sourceDesc = source->GetDesc();
            const auto& destinationDesc = destination->GetDesc();
            if (sourceDesc.Width != destinationDesc.Width || sourceDesc.Height != destinationDesc.Height)
                return;

            Context->CopySubresourceRegion(((DX11Texture*)destination)->Handle.Get(), 0, 0, 0, 0, ((DX11Texture*)source)->Handle.Get(), 0, nullptr);
        }

        void PushDebugGroup(const char* name) override
        {
            if (Annotation != nullptr)
                Annotation->BeginEvent(TEN::Utils::ToWString(name).c_str());
        }

        void PopDebugGroup() override
        {
            if (Annotation != nullptr)
                Annotation->EndEvent();
        }

        void InsertDebugLabel(const char* name) override
        {
            if (Annotation != nullptr)
                Annotation->SetMarker(TEN::Utils::ToWString(name).c_str());
        }

    private:
        static int GetViewKey(int mipLevel, int layer)
        {
            return (mipLevel << 16) | layer;
        }

        ID3D11RenderTargetView* GetRenderTargetView(DX11Texture* texture, int mipLevel, int layer)
        {
            int key = GetViewKey(mipLevel, layer);
            auto it = texture->Rtvs.find(key);
            if (it != texture->Rtvs.end())
                return it->second.Get();

            const auto& desc = texture->Desc;
            auto viewDesc = D3D11_RENDER_TARGET_VIEW_DESC{};
            viewDesc.Format = ToDXGI(desc.Format);

            if (desc.Type == TextureType::Texture2D)
            {
                viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                viewDesc.Texture2D.MipSlice = mipLevel;
            }
            else if (desc.Type == TextureType::Texture3D)
            {
                viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
                viewDesc.Texture3D.MipSlice = mipLevel;
                viewDesc.Texture3D.FirstWSlice = layer;
                viewDesc.Texture3D.WSize = 1;
            }
            else
            {
                viewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                viewDesc.Texture2DArray.MipSlice = mipLevel;
                viewDesc.Texture2DArray.FirstArraySlice = layer;
                viewDesc.Texture2DArray.ArraySize = 1;
            }

            ComPtr<ID3D11RenderTargetView> view;
            Device->CreateRenderTargetView(texture->Handle.Get(), &viewDesc, view.GetAddressOf());
            texture->Rtvs[key] = view;
            return view.Get();
        }

        ID3D11DepthStencilView* GetDepthStencilView(DX11Texture* texture, int layer)
        {
            int key = GetViewKey(0, layer);
            auto it = texture->Dsvs.find(key);
            if (it != texture->Dsvs.end())
                return it->second.Get();

            const auto& desc = texture->Desc;
            auto viewDesc = D3D11_DEPTH_STENCIL_VIEW_DESC{};
            viewDesc.Format = ToDXGI(desc.Format);

            if (desc.Type == TextureType::Texture2D)
            {
                viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
            }
            else
            {
                viewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
                viewDesc.Texture2DArray.FirstArraySlice = layer;
                viewDesc.Texture2DArray.ArraySize = 1;
            }

            ComPtr<ID3D11DepthStencilView> view;
            Device->CreateDepthStencilView(texture->Handle.Get(), &viewDesc, view.GetAddressOf());
            texture->Dsvs[key] = view;
            return view.Get();
        }
    };

    class DX11Device : public IDevice
    {
    private:
        ComPtr<ID3D11Device>        _device    = nullptr;
        ComPtr<ID3D11DeviceContext> _context   = nullptr;
        ComPtr<IDXGISwapChain1>     _swapChain = nullptr;
        SDL_Window*                 _window    = nullptr;
        int                         _syncInterval = 1;
        std::string                 _adapterName  = {};

        DX11Texture       _swapchainTexture = {};
        DX11CommandBuffer _commandBuffer    = {};

        // Debug layer messages, forwarded to the log once per message ID.
        ComPtr<ID3D11InfoQueue>  _infoQueue      = nullptr;
        std::unordered_set<int>  _loggedMessages = {};

    public:
        DX11Device(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> context, ComPtr<IDXGISwapChain1> swapChain, SDL_Window* window, const std::string& adapterName) :
            _device(device),
            _context(context),
            _swapChain(swapChain),
            _window(window),
            _adapterName(adapterName)
        {
            _commandBuffer.Owner = this;
            _commandBuffer.Device = device.Get();
            _commandBuffer.Context = context.Get();
            context.As(&_commandBuffer.Annotation);
            device.As(&_infoQueue);

            auto uniformDesc = D3D11_BUFFER_DESC{};
            uniformDesc.ByteWidth = UNIFORM_BUFFER_SIZE;
            uniformDesc.Usage = D3D11_USAGE_DYNAMIC;
            uniformDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            uniformDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            for (int stage = 0; stage < 3; stage++)
            {
                for (int slot = 0; slot < MAX_UNIFORM_SLOTS; slot++)
                    device->CreateBuffer(&uniformDesc, nullptr, _commandBuffer.UniformBuffers[stage][slot].GetAddressOf());
            }

            UpdateSwapchainTexture();
        }

        ~DX11Device() override
        {
            _context->ClearState();
            _context->Flush();
        }

        void UpdateSwapchainTexture()
        {
            ComPtr<ID3D11Texture2D> backBuffer;
            _swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.GetAddressOf()));

            auto backBufferDesc = D3D11_TEXTURE2D_DESC{};
            backBuffer->GetDesc(&backBufferDesc);

            _swapchainTexture.Handle = backBuffer;
            _swapchainTexture.Rtvs.clear();
            _swapchainTexture.Desc.Width = backBufferDesc.Width;
            _swapchainTexture.Desc.Height = backBufferDesc.Height;
            _swapchainTexture.Desc.Format = PixelFormat::BGRA8Unorm;
            _swapchainTexture.Desc.Usage = TextureUsage::ColorTarget;
            _swapchainTexture.Desc.Name = "Swapchain";
        }

        ITexture* GetSwapchainTexture()
        {
            return &_swapchainTexture;
        }

        void Present()
        {
            _swapChain->Present(_syncInterval, 0);
        }

        void FlushDebugMessages()
        {
            if (_infoQueue == nullptr)
                return;

            auto count = _infoQueue->GetNumStoredMessages();
            for (UINT64 i = 0; i < count; i++)
            {
                SIZE_T size = 0;
                _infoQueue->GetMessage(i, nullptr, &size);

                auto buffer = std::vector<unsigned char>(size);
                auto* message = (D3D11_MESSAGE*)buffer.data();
                if (FAILED(_infoQueue->GetMessage(i, message, &size)) || message->Severity > D3D11_MESSAGE_SEVERITY_WARNING)
                    continue;

                if (!_loggedMessages.insert((int)message->ID).second)
                    continue;

                auto level = (message->Severity == D3D11_MESSAGE_SEVERITY_WARNING) ? LogLevel::Warning : LogLevel::Error;
                TENLog("DX11 debug layer: " + std::string(message->pDescription), level);
            }

            _infoQueue->ClearStoredMessages();
        }

        Backend GetBackend() const override
        {
            return Backend::DirectX11;
        }

        ShaderFormat GetShaderFormat() const override
        {
            return ShaderFormat::Dxbc;
        }

        std::string GetDeviceName() const override
        {
            return _adapterName + " (direct3d11)";
        }

        std::unique_ptr<IBuffer> CreateBuffer(const BufferDesc& desc) override
        {
            auto bufferDesc = D3D11_BUFFER_DESC{};
            bufferDesc.ByteWidth = desc.Size;
            bufferDesc.Usage = D3D11_USAGE_DEFAULT;

            if (desc.Usage & BufferUsage::Vertex)
                bufferDesc.BindFlags |= D3D11_BIND_VERTEX_BUFFER;

            if (desc.Usage & BufferUsage::Index)
                bufferDesc.BindFlags |= D3D11_BIND_INDEX_BUFFER;

            bool isStorage = (desc.Usage & (BufferUsage::GraphicsStorageRead | BufferUsage::ComputeStorageRead)) != 0;
            bool isWritable = (desc.Usage & BufferUsage::ComputeStorageWrite) != 0;

            if (isStorage)
                bufferDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

            if (isWritable)
                bufferDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;

            if ((isStorage || isWritable) && desc.Stride > 0)
            {
                bufferDesc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
                bufferDesc.StructureByteStride = desc.Stride;
            }

            if (desc.Usage & BufferUsage::Indirect)
                bufferDesc.MiscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;

            auto buffer = std::make_unique<DX11Buffer>();
            if (FAILED(_device->CreateBuffer(&bufferDesc, nullptr, buffer->Handle.GetAddressOf())))
            {
                TENLog("DX11: cannot create buffer " + desc.Name, LogLevel::Error);
                return nullptr;
            }

            if (isStorage)
                _device->CreateShaderResourceView(buffer->Handle.Get(), nullptr, buffer->Srv.GetAddressOf());

            if (isWritable)
                _device->CreateUnorderedAccessView(buffer->Handle.Get(), nullptr, buffer->Uav.GetAddressOf());

            SetDebugName(buffer->Handle.Get(), desc.Name);
            buffer->Size = desc.Size;
            buffer->Usage = desc.Usage;
            return buffer;
        }

        std::unique_ptr<ITexture> CreateTexture(const TextureDesc& desc) override
        {
            bool isDepth = IsDepthFormat(desc.Format);
            bool isSampled = (desc.Usage & TextureUsage::Sampler) != 0;
            bool isTarget = (desc.Usage & TextureUsage::ColorTarget) != 0;

            unsigned int bindFlags = 0;
            if (isSampled)
                bindFlags |= D3D11_BIND_SHADER_RESOURCE;

            if (isTarget)
                bindFlags |= D3D11_BIND_RENDER_TARGET;

            if (desc.Usage & TextureUsage::DepthStencilTarget)
                bindFlags |= D3D11_BIND_DEPTH_STENCIL;

            if (desc.Usage & TextureUsage::ComputeStorageWrite)
                bindFlags |= D3D11_BIND_UNORDERED_ACCESS;

            // Mip generation in D3D11 needs a render target texture with the dedicated flag.
            unsigned int miscFlags = (isTarget && isSampled && desc.MipLevels > 1) ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;
            auto format = ToDXGIResourceFormat(desc.Format, isDepth && isSampled);

            auto texture = std::make_unique<DX11Texture>();
            texture->Desc = desc;

            auto srvDesc = D3D11_SHADER_RESOURCE_VIEW_DESC{};
            srvDesc.Format = ToDXGIShaderResourceFormat(desc.Format);

            HRESULT result = E_FAIL;
            if (desc.Type == TextureType::Texture3D)
            {
                auto textureDesc = D3D11_TEXTURE3D_DESC{};
                textureDesc.Width = desc.Width;
                textureDesc.Height = desc.Height;
                textureDesc.Depth = desc.DepthOrLayers;
                textureDesc.MipLevels = desc.MipLevels;
                textureDesc.Format = format;
                textureDesc.Usage = D3D11_USAGE_DEFAULT;
                textureDesc.BindFlags = bindFlags;
                textureDesc.MiscFlags = miscFlags;

                ComPtr<ID3D11Texture3D> texture3D;
                result = _device->CreateTexture3D(&textureDesc, nullptr, texture3D.GetAddressOf());
                texture->Handle = texture3D;

                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
                srvDesc.Texture3D.MipLevels = desc.MipLevels;
            }
            else
            {
                bool isCube = (desc.Type == TextureType::TextureCube);

                auto textureDesc = D3D11_TEXTURE2D_DESC{};
                textureDesc.Width = desc.Width;
                textureDesc.Height = desc.Height;
                textureDesc.MipLevels = desc.MipLevels;
                textureDesc.ArraySize = isCube ? 6 : desc.DepthOrLayers;
                textureDesc.Format = format;
                textureDesc.SampleDesc.Count = desc.SampleCount;
                textureDesc.Usage = D3D11_USAGE_DEFAULT;
                textureDesc.BindFlags = bindFlags;
                textureDesc.MiscFlags = miscFlags | (isCube ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0);

                ComPtr<ID3D11Texture2D> texture2D;
                result = _device->CreateTexture2D(&textureDesc, nullptr, texture2D.GetAddressOf());
                texture->Handle = texture2D;

                if (isCube)
                {
                    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
                    srvDesc.TextureCube.MipLevels = desc.MipLevels;
                }
                else if (desc.Type == TextureType::Texture2DArray)
                {
                    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                    srvDesc.Texture2DArray.MipLevels = desc.MipLevels;
                    srvDesc.Texture2DArray.ArraySize = desc.DepthOrLayers;
                }
                else
                {
                    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                    srvDesc.Texture2D.MipLevels = desc.MipLevels;
                }
            }

            if (FAILED(result))
            {
                TENLog("DX11: cannot create texture " + desc.Name, LogLevel::Error);
                return nullptr;
            }

            if (isSampled)
                _device->CreateShaderResourceView(texture->Handle.Get(), &srvDesc, texture->Srv.GetAddressOf());

            if (desc.Usage & TextureUsage::ComputeStorageWrite)
                _device->CreateUnorderedAccessView(texture->Handle.Get(), nullptr, texture->Uav.GetAddressOf());

            SetDebugName(texture->Handle.Get(), desc.Name);
            return texture;
        }

        std::unique_ptr<ISampler> CreateSampler(const SamplerDesc& desc) override
        {
            auto samplerDesc = D3D11_SAMPLER_DESC{};
            samplerDesc.Filter = ToD3D11Filter(desc);
            samplerDesc.AddressU = ToD3D11(desc.AddressU);
            samplerDesc.AddressV = ToD3D11(desc.AddressV);
            samplerDesc.AddressW = ToD3D11(desc.AddressW);
            samplerDesc.MipLODBias = desc.MipLodBias;
            samplerDesc.MaxAnisotropy = std::max(1, desc.MaxAnisotropy);
            samplerDesc.ComparisonFunc = desc.EnableCompare ? ToD3D11(desc.Compare) : D3D11_COMPARISON_NEVER;
            samplerDesc.MinLOD = desc.MinLod;
            samplerDesc.MaxLOD = desc.MaxLod;

            auto sampler = std::make_unique<DX11Sampler>();
            if (FAILED(_device->CreateSamplerState(&samplerDesc, sampler->Handle.GetAddressOf())))
            {
                TENLog("DX11: cannot create sampler " + desc.Name, LogLevel::Error);
                return nullptr;
            }

            SetDebugName(sampler->Handle.Get(), desc.Name);
            return sampler;
        }

        std::unique_ptr<IShader> CreateShader(const ShaderDesc& desc) override
        {
            auto shader = std::make_unique<DX11Shader>();
            shader->Desc = desc;
            shader->Bytecode.assign(desc.Code, desc.Code + desc.CodeSize);
            shader->Desc.Code = shader->Bytecode.data();

            HRESULT result = E_FAIL;
            switch (desc.Stage)
            {
            case ShaderStage::Vertex:
                result = _device->CreateVertexShader(desc.Code, desc.CodeSize, nullptr, shader->VertexShader.GetAddressOf());
                SetDebugName(shader->VertexShader.Get(), desc.Name);
                break;

            case ShaderStage::Pixel:
                result = _device->CreatePixelShader(desc.Code, desc.CodeSize, nullptr, shader->PixelShader.GetAddressOf());
                SetDebugName(shader->PixelShader.Get(), desc.Name);
                break;

            case ShaderStage::Compute:
                result = _device->CreateComputeShader(desc.Code, desc.CodeSize, nullptr, shader->ComputeShader.GetAddressOf());
                SetDebugName(shader->ComputeShader.Get(), desc.Name);
                break;
            }

            if (FAILED(result))
            {
                TENLog("DX11: cannot create shader " + desc.Name, LogLevel::Error);
                return nullptr;
            }

            return shader;
        }

        std::unique_ptr<IGraphicsPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) override
        {
            auto* vertexShader = (DX11Shader*)desc.VertexShader;
            auto* pixelShader = (DX11Shader*)desc.PixelShader;

            auto pipeline = std::make_unique<DX11GraphicsPipeline>();
            pipeline->VertexShader = vertexShader->VertexShader;
            pipeline->PixelShader = pixelShader->PixelShader;
            pipeline->Topology = ToD3D11(desc.Primitive);
            pipeline->VertexStorageBufferBase = vertexShader->Desc.NumSamplers + vertexShader->Desc.NumStorageTextures;
            pipeline->PixelStorageBufferBase = pixelShader->Desc.NumSamplers + pixelShader->Desc.NumStorageTextures;

            for (int i = 0; i < desc.Layout.NumBuffers; i++)
                pipeline->VertexStrides[desc.Layout.Buffers[i].Slot] = desc.Layout.Buffers[i].Stride;

            if (desc.Layout.NumAttributes > 0)
            {
                D3D11_INPUT_ELEMENT_DESC elements[MAX_VERTEX_ATTRIBUTES] = {};
                for (int i = 0; i < desc.Layout.NumAttributes; i++)
                {
                    const auto& attribute = desc.Layout.Attributes[i];
                    VertexInputRate rate = VertexInputRate::Vertex;
                    for (int j = 0; j < desc.Layout.NumBuffers; j++)
                    {
                        if (desc.Layout.Buffers[j].Slot == attribute.BufferSlot)
                            rate = desc.Layout.Buffers[j].Rate;
                    }

                    elements[i].SemanticName = "TEXCOORD";
                    elements[i].SemanticIndex = attribute.Location;
                    elements[i].Format = ToDXGI(attribute.Format);
                    elements[i].InputSlot = attribute.BufferSlot;
                    elements[i].AlignedByteOffset = attribute.Offset;
                    elements[i].InputSlotClass = (rate == VertexInputRate::Instance) ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
                    elements[i].InstanceDataStepRate = (rate == VertexInputRate::Instance) ? 1 : 0;
                }

                if (FAILED(_device->CreateInputLayout(elements, desc.Layout.NumAttributes, vertexShader->Bytecode.data(), vertexShader->Bytecode.size(), pipeline->InputLayout.GetAddressOf())))
                {
                    TENLog("DX11: cannot create input layout for pipeline " + desc.Name, LogLevel::Error);
                    return nullptr;
                }
            }

            auto blendDesc = D3D11_BLEND_DESC{};
            blendDesc.IndependentBlendEnable = desc.NumColorTargets > 1;
            for (int i = 0; i < desc.NumColorTargets; i++)
            {
                const auto& blend = desc.ColorTargets[i].Blend;
                auto& target = blendDesc.RenderTarget[i];
                target.BlendEnable = blend.EnableBlend;
                target.SrcBlend = ToD3D11(blend.SrcColor);
                target.DestBlend = ToD3D11(blend.DstColor);
                target.BlendOp = ToD3D11(blend.ColorOp);
                target.SrcBlendAlpha = ToD3D11Alpha(blend.SrcAlpha);
                target.DestBlendAlpha = ToD3D11Alpha(blend.DstAlpha);
                target.BlendOpAlpha = ToD3D11(blend.AlphaOp);
                target.RenderTargetWriteMask = (UINT8)blend.WriteMask;
            }

            _device->CreateBlendState(&blendDesc, pipeline->BlendState.GetAddressOf());

            auto rasterizerDesc = D3D11_RASTERIZER_DESC{};
            rasterizerDesc.FillMode = (desc.Rasterizer.Fill == FillMode::Wireframe) ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
            rasterizerDesc.CullMode = (desc.Rasterizer.Cull == CullMode::None) ? D3D11_CULL_NONE : ((desc.Rasterizer.Cull == CullMode::Front) ? D3D11_CULL_FRONT : D3D11_CULL_BACK);
            rasterizerDesc.FrontCounterClockwise = (desc.Rasterizer.Front == FrontFace::CounterClockwise);
            rasterizerDesc.DepthBias = desc.Rasterizer.EnableDepthBias ? (INT)desc.Rasterizer.DepthBiasConstant : 0;
            rasterizerDesc.DepthBiasClamp = desc.Rasterizer.EnableDepthBias ? desc.Rasterizer.DepthBiasClamp : 0.0f;
            rasterizerDesc.SlopeScaledDepthBias = desc.Rasterizer.EnableDepthBias ? desc.Rasterizer.DepthBiasSlope : 0.0f;
            rasterizerDesc.DepthClipEnable = desc.Rasterizer.EnableDepthClip;
            rasterizerDesc.ScissorEnable = true;
            rasterizerDesc.MultisampleEnable = desc.SampleCount > 1;

            _device->CreateRasterizerState(&rasterizerDesc, pipeline->Rasterizer.GetAddressOf());

            auto depthDesc = D3D11_DEPTH_STENCIL_DESC{};
            depthDesc.DepthEnable = desc.DepthStencil.EnableDepthTest && desc.DepthFormat != PixelFormat::Unknown;
            depthDesc.DepthWriteMask = desc.DepthStencil.EnableDepthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
            depthDesc.DepthFunc = ToD3D11(desc.DepthStencil.DepthCompare);

            _device->CreateDepthStencilState(&depthDesc, pipeline->DepthStencil.GetAddressOf());
            return pipeline;
        }

        std::unique_ptr<IComputePipeline> CreateComputePipeline(const ComputePipelineDesc& desc) override
        {
            auto* shader = (DX11Shader*)desc.Shader;

            auto pipeline = std::make_unique<DX11ComputePipeline>();
            pipeline->Shader = shader->ComputeShader;
            pipeline->StorageBufferBase = shader->Desc.NumSamplers + shader->Desc.NumStorageTextures;
            return pipeline;
        }

        std::unique_ptr<ITransferBuffer> CreateTransferBuffer(TransferBufferUsage usage, int size) override
        {
            auto buffer = std::make_unique<DX11TransferBuffer>();
            buffer->Data.resize(size);
            return buffer;
        }

        void* MapTransferBuffer(ITransferBuffer* buffer, bool cycle) override
        {
            return ((DX11TransferBuffer*)buffer)->Data.data();
        }

        void UnmapTransferBuffer(ITransferBuffer* buffer) override {}

        void GenerateMipmaps(ICommandBuffer* commandBuffer, ITexture* texture) override
        {
            auto* srv = ((DX11Texture*)texture)->Srv.Get();
            if (srv != nullptr)
                _context->GenerateMips(srv);
        }

        ICommandBuffer* AcquireCommandBuffer() override
        {
            return &_commandBuffer;
        }

        PixelFormat GetSwapchainFormat() const override
        {
            return PixelFormat::BGRA8Unorm;
        }

        void SetPresentMode(PresentMode mode) override
        {
            _syncInterval = (mode == PresentMode::Vsync) ? 1 : 0;
        }

        void ResizeSwapchain(int width, int height) override
        {
            _context->OMSetRenderTargets(0, nullptr, nullptr);
            _swapchainTexture.Handle.Reset();
            _swapchainTexture.Rtvs.clear();

            if (FAILED(_swapChain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0)))
                TENLog("DX11: swapchain resize failed.", LogLevel::Error);

            UpdateSwapchainTexture();
        }

        void WaitIdle() override
        {
            _context->Flush();
        }

        bool SupportsTextureFormat(PixelFormat format, TextureType type, unsigned int usage) const override
        {
            UINT support = 0;
            if (FAILED(_device->CheckFormatSupport(ToDXGI(format), &support)))
                return false;

            if ((usage & TextureUsage::Sampler) && !(support & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE))
                return false;

            if ((usage & TextureUsage::ColorTarget) && !(support & D3D11_FORMAT_SUPPORT_RENDER_TARGET))
                return false;

            if ((usage & TextureUsage::DepthStencilTarget) && !(support & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL))
                return false;

            return true;
        }

        bool RequiresWindowThreadPresent() const override
        {
            return false;
        }
    };

    ITexture* DX11CommandBuffer::AcquireSwapchainTexture()
    {
        HasSwapchainTexture = true;
        return Owner->GetSwapchainTexture();
    }

    void DX11CommandBuffer::Submit()
    {
        Owner->FlushDebugMessages();

        if (HasSwapchainTexture)
            Owner->Present();

        HasSwapchainTexture = false;
    }

    // Same policy as SDL GPU: prefer the high performance adapter on hybrid graphics systems,
    // otherwise let the runtime pick the default one.
    static ComPtr<IDXGIAdapter1> GetHighPerformanceAdapter()
    {
        ComPtr<IDXGIFactory6> factory;
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf()))))
            return nullptr;

        ComPtr<IDXGIAdapter1> adapter;
        if (FAILED(factory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(adapter.GetAddressOf()))))
            return nullptr;

        return adapter;
    }

    std::unique_ptr<IDevice> CreateDX11Device(SDL_Window* window, bool debugMode)
    {
        auto props = SDL_GetWindowProperties(window);
        auto hwnd = (HWND)SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        if (hwnd == nullptr)
            return nullptr;

        UINT flags = debugMode ? D3D11_CREATE_DEVICE_DEBUG : 0;
        D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };

        auto preferredAdapter = GetHighPerformanceAdapter();
        auto* adapterToUse = preferredAdapter.Get();
        auto driverType = (adapterToUse != nullptr) ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE;

        ComPtr<ID3D11Device> device;
        ComPtr<ID3D11DeviceContext> context;
        auto result = D3D11CreateDevice(adapterToUse, driverType, nullptr, flags, levels, 2, D3D11_SDK_VERSION, device.GetAddressOf(), nullptr, context.GetAddressOf());

        // The debug layer is optional; retry without it when the SDK layers are not installed.
        if (FAILED(result) && debugMode)
            result = D3D11CreateDevice(adapterToUse, driverType, nullptr, 0, levels, 2, D3D11_SDK_VERSION, device.GetAddressOf(), nullptr, context.GetAddressOf());

        if (FAILED(result))
        {
            TENLog("DX11: cannot create device.", LogLevel::Error);
            return nullptr;
        }

        ComPtr<IDXGIDevice> dxgiDevice;
        ComPtr<IDXGIAdapter> adapter;
        ComPtr<IDXGIFactory2> factory;
        device.As(&dxgiDevice);
        dxgiDevice->GetAdapter(adapter.GetAddressOf());
        adapter->GetParent(IID_PPV_ARGS(factory.GetAddressOf()));

        auto adapterDesc = DXGI_ADAPTER_DESC{};
        adapter->GetDesc(&adapterDesc);
        auto adapterName = TEN::Utils::ToString(adapterDesc.Description);

        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);

        auto swapChainDesc = DXGI_SWAP_CHAIN_DESC1{};
        swapChainDesc.Width = width;
        swapChainDesc.Height = height;
        swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = SWAPCHAIN_BUFFER_COUNT;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

        ComPtr<IDXGISwapChain1> swapChain;
        if (FAILED(factory->CreateSwapChainForHwnd(device.Get(), hwnd, &swapChainDesc, nullptr, nullptr, swapChain.GetAddressOf())))
        {
            TENLog("DX11: cannot create swapchain.", LogLevel::Error);
            return nullptr;
        }

        factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

        auto result2 = std::make_unique<DX11Device>(device, context, swapChain, window, adapterName);
        TENLog("DX11 RHI device created: " + result2->GetDeviceName(), LogLevel::Info);
        return result2;
    }
}

#else

namespace TEN::Renderer::RHI::DirectX11
{
    std::unique_ptr<IDevice> CreateDX11Device(SDL_Window* window, bool debugMode)
    {
        return nullptr;
    }
}

#endif
