#pragma once

namespace TEN::Renderer::RHI
{
    class IBuffer;
    class ITexture;
    class ISampler;
    class IShader;
    class IGraphicsPipeline;
    class IComputePipeline;
    class ITransferBuffer;

    constexpr auto MAX_COLOR_TARGETS      = 4;
    constexpr auto MAX_VERTEX_BUFFERS     = 4;
    constexpr auto MAX_VERTEX_ATTRIBUTES  = 16;
    constexpr auto MAX_UNIFORM_SLOTS      = 4;
    constexpr auto MAX_SAMPLER_SLOTS      = 16;
    constexpr auto MAX_STORAGE_SLOTS      = 8;

    enum class GraphicsApi
    {
        None,
        DirectX11,
        Vulkan,
        Direct3D12,
        OpenGL
    };

    enum class ShaderFormat
    {
        None,
        Dxbc,
        Spirv,
        Dxil,
        Glsl
    };

    enum class ShaderStage
    {
        Vertex,
        Pixel,
        Compute
    };

    enum class PixelFormat
    {
        Unknown,

        R8Unorm,
        RG8Unorm,
        RGBA8Unorm,
        RGBA8UnormSrgb,
        BGRA8Unorm,
        BGRA8UnormSrgb,
        R16Float,
        RG16Float,
        RGBA16Float,
        R32Float,
        RG32Float,
        RGBA32Float,
        R11G11B10Float,
        R32Uint,

        BC1Unorm,
        BC2Unorm,
        BC3Unorm,
        BC4Unorm,
        BC5Unorm,
        BC7Unorm,

        D16Unorm,
        D24UnormS8Uint,
        D32Float
    };

    enum class VertexElementFormat
    {
        Float,
        Float2,
        Float3,
        Float4,
        UByte4,
        UByte4Norm,
        Byte4Norm,
        Short2Norm,
        UShort2Norm,
        Short4Norm,
        Half2,
        Half4,
        UInt
    };

    enum class VertexInputRate
    {
        Vertex,
        Instance
    };

    enum class PrimitiveType
    {
        TriangleList,
        TriangleStrip,
        LineList,
        LineStrip,
        PointList
    };

    enum class IndexSize
    {
        Bits16,
        Bits32
    };

    enum class FillMode
    {
        Solid,
        Wireframe
    };

    enum class CullMode
    {
        None,
        Front,
        Back
    };

    enum class FrontFace
    {
        CounterClockwise,
        Clockwise
    };

    enum class CompareOp
    {
        Never,
        Less,
        Equal,
        LessOrEqual,
        Greater,
        NotEqual,
        GreaterOrEqual,
        Always
    };

    enum class BlendFactor
    {
        Zero,
        One,
        SrcColor,
        OneMinusSrcColor,
        DstColor,
        OneMinusDstColor,
        SrcAlpha,
        OneMinusSrcAlpha,
        DstAlpha,
        OneMinusDstAlpha
    };

    enum class BlendOp
    {
        Add,
        Subtract,
        ReverseSubtract,
        Min,
        Max
    };

    enum class Filter
    {
        Nearest,
        Linear
    };

    enum class AddressMode
    {
        Repeat,
        MirroredRepeat,
        ClampToEdge
    };

    enum class LoadOp
    {
        Load,
        Clear,
        DontCare
    };

    enum class StoreOp
    {
        Store,
        DontCare
    };

    enum class TextureType
    {
        Texture2D,
        Texture2DArray,
        TextureCube,
        Texture3D
    };

    enum class TransferBufferUsage
    {
        Upload,
        Download
    };

    enum class PresentMode
    {
        Vsync,
        Immediate,
        Mailbox
    };

    namespace BufferUsage
    {
        constexpr auto Vertex              = 1 << 0;
        constexpr auto Index               = 1 << 1;
        constexpr auto Indirect            = 1 << 2;
        constexpr auto GraphicsStorageRead = 1 << 3;
        constexpr auto ComputeStorageRead  = 1 << 4;
        constexpr auto ComputeStorageWrite = 1 << 5;
    }

    namespace TextureUsage
    {
        constexpr auto Sampler             = 1 << 0;
        constexpr auto ColorTarget         = 1 << 1;
        constexpr auto DepthStencilTarget  = 1 << 2;
        constexpr auto ComputeStorageRead  = 1 << 3;
        constexpr auto ComputeStorageWrite = 1 << 4;
    }

    namespace ColorWriteMask
    {
        constexpr auto Red   = 1 << 0;
        constexpr auto Green = 1 << 1;
        constexpr auto Blue  = 1 << 2;
        constexpr auto Alpha = 1 << 3;
        constexpr auto All   = Red | Green | Blue | Alpha;
    }

    struct BufferDesc
    {
        int          Size   = 0;
        int          Stride = 0; // Element size of storage buffers, required by DX11 structured buffers.
        unsigned int Usage  = 0;
        std::string  Name   = {};
    };

    struct TextureDesc
    {
        TextureType  Type          = TextureType::Texture2D;
        PixelFormat       Format        = PixelFormat::RGBA8Unorm;
        int          Width         = 1;
        int          Height        = 1;
        int          DepthOrLayers = 1;
        int          MipLevels     = 1;
        int          SampleCount   = 1;
        unsigned int Usage         = TextureUsage::Sampler;
        std::string  Name          = {};
    };

    struct SamplerDesc
    {
        Filter      MinFilter     = Filter::Linear;
        Filter      MagFilter     = Filter::Linear;
        Filter      MipFilter     = Filter::Linear;
        AddressMode AddressU      = AddressMode::Repeat;
        AddressMode AddressV      = AddressMode::Repeat;
        AddressMode AddressW      = AddressMode::Repeat;
        int         MaxAnisotropy = 1;
        bool        EnableCompare = false;
        CompareOp   Compare       = CompareOp::LessOrEqual;
        float       MipLodBias    = 0.0f;
        float       MinLod        = 0.0f;
        float       MaxLod        = 1000.0f;
        std::string Name          = {};
    };

    // Resource counts must match the shader exactly; the shader compiler step writes them
    // into the shader manifest, so callers normally fill this from ShaderLibrary.
    struct ShaderDesc
    {
        ShaderStage                Stage               = ShaderStage::Vertex;
        const unsigned char*       Code                = nullptr;
        int                        CodeSize            = 0;
        std::string                EntryPoint          = "main";
        int                        NumSamplers         = 0;
        int                        NumStorageTextures  = 0;
        int                        NumStorageBuffers   = 0;
        int                        NumUniformBuffers   = 0;
        int                        NumRWStorageTextures = 0; // Compute only.
        int                        NumRWStorageBuffers  = 0; // Compute only.
        int                        ThreadCountX        = 1;  // Compute only.
        int                        ThreadCountY        = 1;
        int                        ThreadCountZ        = 1;
        std::string                Name                = {};
    };

    struct VertexBufferLayout
    {
        int             Slot   = 0;
        int             Stride = 0;
        VertexInputRate Rate   = VertexInputRate::Vertex;
    };

    // DX11 matches attributes by semantic, SDL GPU by location. Shaders use TEXCOORD<Location>
    // for every attribute so both backends resolve the same layout.
    struct VertexAttribute
    {
        int                 Location   = 0;
        int                 BufferSlot = 0;
        VertexElementFormat Format     = VertexElementFormat::Float3;
        int                 Offset     = 0;
    };

    struct VertexLayout
    {
        VertexBufferLayout Buffers[MAX_VERTEX_BUFFERS]       = {};
        int                NumBuffers                        = 0;
        VertexAttribute    Attributes[MAX_VERTEX_ATTRIBUTES] = {};
        int                NumAttributes                     = 0;
    };

    struct RasterizerState
    {
        FillMode  Fill                 = FillMode::Solid;
        CullMode  Cull                 = CullMode::Back;
        FrontFace Front                = FrontFace::Clockwise;
        float     DepthBiasConstant    = 0.0f;
        float     DepthBiasSlope       = 0.0f;
        float     DepthBiasClamp       = 0.0f;
        bool      EnableDepthBias      = false;
        bool      EnableDepthClip      = true;
    };

    struct DepthStencilState
    {
        bool      EnableDepthTest  = true;
        bool      EnableDepthWrite = true;
        CompareOp DepthCompare     = CompareOp::LessOrEqual;
    };

    struct BlendState
    {
        bool         EnableBlend    = false;
        BlendFactor  SrcColor       = BlendFactor::One;
        BlendFactor  DstColor       = BlendFactor::Zero;
        BlendOp      ColorOp        = BlendOp::Add;
        BlendFactor  SrcAlpha       = BlendFactor::One;
        BlendFactor  DstAlpha       = BlendFactor::Zero;
        BlendOp      AlphaOp        = BlendOp::Add;
        unsigned int WriteMask      = ColorWriteMask::All;
    };

    struct ColorTargetDesc
    {
        PixelFormat     Format = PixelFormat::RGBA8Unorm;
        BlendState Blend  = {};
    };

    struct GraphicsPipelineDesc
    {
        IShader*          VertexShader                    = nullptr;
        IShader*          PixelShader                     = nullptr;
        VertexLayout      Layout                          = {};
        PrimitiveType     Primitive                       = PrimitiveType::TriangleList;
        RasterizerState   Rasterizer                      = {};
        DepthStencilState DepthStencil                    = {};
        ColorTargetDesc   ColorTargets[MAX_COLOR_TARGETS] = {};
        int               NumColorTargets                 = 0;
        PixelFormat            DepthFormat                     = PixelFormat::Unknown;
        int               SampleCount                     = 1;
        std::string       Name                            = {};
    };

    struct ComputePipelineDesc
    {
        IShader*    Shader = nullptr;
        std::string Name   = {};
    };

    struct ColorTargetInfo
    {
        ITexture* Texture    = nullptr;
        int       MipLevel   = 0;
        int       Layer      = 0;
        LoadOp    Load       = LoadOp::Clear;
        StoreOp   Store      = StoreOp::Store;
        float     ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    };

    struct DepthTargetInfo
    {
        ITexture*     Texture      = nullptr;
        int           Layer        = 0;
        LoadOp        Load         = LoadOp::Clear;
        StoreOp       Store        = StoreOp::Store;
        float         ClearDepth   = 1.0f;
        unsigned char ClearStencil = 0;
    };

    // API calls recorded into a command buffer, for the debug statistics.
    struct CommandCounters
    {
        int       RenderPasses       = 0;
        int       CopyPasses         = 0;
        int       ComputePasses      = 0;
        int       PipelineBinds      = 0;
        int       VertexBufferBinds  = 0;
        int       IndexBufferBinds   = 0;
        int       TextureBinds       = 0;
        int       StorageBufferBinds = 0;
        int       UniformPushes      = 0;
        long long UniformBytes       = 0;
        int       Draws              = 0;
        int       IndexedDraws       = 0;
        long long Instances          = 0;
        long long Triangles          = 0;
        long long Lines              = 0;
        int       Dispatches         = 0;
        int       Blits              = 0;
        int       Uploads            = 0;
        long long UploadBytes        = 0;

        void AddDraw(PrimitiveType primitive, int count, int instanceCount, bool isIndexed)
        {
            Draws++;
            IndexedDraws += isIndexed ? 1 : 0;
            Instances += instanceCount;

            switch (primitive)
            {
            case PrimitiveType::TriangleList:
                Triangles += (long long)(count / 3) * instanceCount;
                break;

            case PrimitiveType::TriangleStrip:
                Triangles += (long long)std::max(count - 2, 0) * instanceCount;
                break;

            case PrimitiveType::LineList:
                Lines += (long long)(count / 2) * instanceCount;
                break;

            case PrimitiveType::LineStrip:
                Lines += (long long)std::max(count - 1, 0) * instanceCount;
                break;

            default:
                break;
            }
        }
    };

    struct GpuTiming
    {
        std::string Name         = {};
        float       Milliseconds = 0.0f;
        int         Samples      = 0;
    };

    struct BufferBinding
    {
        IBuffer* Buffer = nullptr;
        int      Offset = 0;
    };

    struct TextureSamplerBinding
    {
        ITexture* Texture = nullptr;
        ISampler* Sampler = nullptr;
    };

    struct Viewport
    {
        float X        = 0.0f;
        float Y        = 0.0f;
        float Width    = 0.0f;
        float Height   = 0.0f;
        float MinDepth = 0.0f;
        float MaxDepth = 1.0f;
    };

    struct ScissorRect
    {
        int X      = 0;
        int Y      = 0;
        int Width  = 0;
        int Height = 0;
    };

    struct TextureRegion
    {
        ITexture* Texture  = nullptr;
        int       MipLevel = 0;
        int       Layer    = 0;
        int       X        = 0;
        int       Y        = 0;
        int       Z        = 0;
        int       Width    = 0;
        int       Height   = 0;
        int       Depth    = 1;
    };

    // Layout of texel data inside a transfer buffer. Zero pitches mean tightly packed rows.
    struct TextureTransferInfo
    {
        ITransferBuffer* TransferBuffer = nullptr;
        int              Offset         = 0;
        int              PixelsPerRow   = 0;
        int              RowsPerLayer   = 0;
    };

    int  GetFormatBlockSize(PixelFormat format);
    int  GetFormatBlockDimension(PixelFormat format);
    bool IsDepthFormat(PixelFormat format);
    int  GetTextureDataSize(PixelFormat format, int width, int height, int depth);
}
