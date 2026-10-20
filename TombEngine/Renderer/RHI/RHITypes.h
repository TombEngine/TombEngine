#pragma once

// Plain descriptions, enums and binding structures of the RHI, shared by the renderer and every backend.
// Values mirror SDL GPU; each backend translates them to its API. Sizes and offsets are in bytes and texel
// coordinates in texels unless stated otherwise.

namespace TEN::Renderer::RHI
{
    class IBuffer;
    class ITexture;
    class ISampler;
    class IShader;
    class IGraphicsPipeline;
    class IComputePipeline;
    class ITransferBuffer;

    // Limits per pipeline or per shader stage.
    constexpr auto MAX_COLOR_TARGETS      = 4;
    constexpr auto MAX_VERTEX_BUFFERS     = 4;
    constexpr auto MAX_VERTEX_ATTRIBUTES  = 16;
    constexpr auto MAX_UNIFORM_SLOTS      = 4;  // Uniform blocks per stage, as in SDL GPU.
    constexpr auto MAX_SAMPLER_SLOTS      = 16; // Texture and sampler pairs per stage.
    constexpr auto MAX_STORAGE_SLOTS      = 8;  // Storage buffers per stage, and read-write resources per compute pass.

    // Vulkan and Direct3D12 run through SDL GPU.
    enum class GraphicsApi
    {
        None,
        DirectX11,
        Vulkan,
        Direct3D12,
        OpenGL
    };

    // Shader binary formats written by CompileShaders.ps1: DXBC (FXC) for DX11, SPIR-V for Vulkan,
    // DXIL for Direct3D 12 and GLSL (SPIRV-Cross) for OpenGL.
    enum class ShaderFormat
    {
        None,
        Dxbc,
        Spirv,
        Dxil,
        Glsl
    };

    // Pixel is the fragment stage of SDL GPU and OpenGL.
    enum class ShaderStage
    {
        Vertex,
        Pixel,
        Compute
    };

    // BC formats store 4x4 texel blocks (see GetFormatBlockSize and GetFormatBlockDimension). D formats are depth
    // formats; the S8 variants also have stencil.
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
        D32Float,
        D32FloatS8Uint
    };

    // Norm formats reach the shader as floats in [0, 1] or [-1, 1]; UByte4 and UInt as integers.
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

    // Applied to pixels that pass both the stencil and the depth test; failing pixels keep their stencil value.
    enum class StencilOp
    {
        Keep,
        Zero,
        Replace
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

    // Arrays and cube maps select their layer or face through the Layer of targets and regions; 3D textures
    // use Z and Depth.
    enum class TextureType
    {
        Texture2D,
        Texture2DArray,
        TextureCube,
        Texture3D
    };

    // Upload buffers are written by the CPU and read by copy passes; download buffers the opposite.
    enum class TransferBufferUsage
    {
        Upload,
        Download
    };

    // Vsync waits for vertical blank. Immediate presents at once and may tear. Mailbox replaces the queued image
    // without tearing; only SDL GPU supports it (see IDevice::SupportsPresentMode).
    enum class PresentMode
    {
        Vsync,
        Immediate,
        Mailbox
    };

    // Flags of BufferDesc::Usage. GraphicsStorageRead allows binding as a read-only storage buffer in vertex and
    // pixel shaders; the compute flags allow read-only and read-write binding in compute passes.
    namespace BufferUsage
    {
        constexpr auto Vertex              = 1 << 0;
        constexpr auto Index               = 1 << 1;
        constexpr auto Indirect            = 1 << 2;
        constexpr auto GraphicsStorageRead = 1 << 3;
        constexpr auto ComputeStorageRead  = 1 << 4;
        constexpr auto ComputeStorageWrite = 1 << 5;
    }

    // Flags of TextureDesc::Usage.
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

    // GPU buffers are not CPU visible: fill them through a transfer buffer and a copy pass (see UploadRing).
    struct BufferDesc
    {
        int          Size   = 0;
        int          Stride = 0; // Element size of storage buffers, required by DX11 structured buffers.
        unsigned int Usage  = 0; // BufferUsage flags.
        std::string  Name   = {}; // Debug name shown by graphics debuggers.
    };

    struct TextureDesc
    {
        TextureType  Type          = TextureType::Texture2D;
        PixelFormat       Format        = PixelFormat::RGBA8Unorm;
        int          Width         = 1;
        int          Height        = 1;
        int          DepthOrLayers = 1; // Depth of 3D textures, layer count of arrays; 6 for cube maps.
        int          MipLevels     = 1;
        int          SampleCount   = 1; // Multisampling: 1, 2, 4 or 8.
        unsigned int Usage         = TextureUsage::Sampler; // TextureUsage flags.
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
        int         MaxAnisotropy = 1;     // Above 1, anisotropic filtering replaces the filters on DX11 and OpenGL.
        bool        EnableCompare = false; // Comparison sampler for shadow maps (TEN_PS_COMPARISON_TEXTURE).
        CompareOp   Compare       = CompareOp::LessOrEqual;
        float       MipLodBias    = 0.0f;
        float       MinLod        = 0.0f;
        float       MaxLod        = 1000.0f; // Default covers every mip.
        std::string Name          = {};
    };

    // Resource counts must match the shader exactly; the shader compiler step writes them
    // into the shader manifest, so callers normally fill this from ShaderLibrary.
    struct ShaderDesc
    {
        ShaderStage                Stage               = ShaderStage::Vertex;
        const unsigned char*       Code                = nullptr; // Not owned; GLSL source text on OpenGL.
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
        int             Slot   = 0; // Slot given to IRenderPass::BindVertexBuffers.
        int             Stride = 0; // Bytes between consecutive vertices or instances.
        VertexInputRate Rate   = VertexInputRate::Vertex;
    };

    // DX11 matches attributes by semantic, SDL GPU by location. Shaders use TEXCOORD<Location>
    // for every attribute so both backends resolve the same layout.
    struct VertexAttribute
    {
        int                 Location   = 0; // TEN_LOCATION(N) and semantic TEXCOORD<N> in the shader.
        int                 BufferSlot = 0;
        VertexElementFormat Format     = VertexElementFormat::Float3;
        int                 Offset     = 0; // Bytes from the start of the vertex.
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
        FrontFace Front                = FrontFace::Clockwise; // Winding in Direct3D conventions, shared by every backend.
        float     DepthBiasConstant    = 0.0f; // In units of the smallest depth difference; truncated to an integer on DX11.
        float     DepthBiasSlope       = 0.0f;
        float     DepthBiasClamp       = 0.0f; // Ignored on OpenGL.
        bool      EnableDepthBias      = false;
        bool      EnableDepthClip      = true; // When off, depth is clamped to the viewport range instead of clipped.
    };

    struct DepthStencilState
    {
        bool      EnableDepthTest  = true;
        bool      EnableDepthWrite = true;
        CompareOp DepthCompare     = CompareOp::LessOrEqual;

        // Stencil test and write of the pixels that pass, with the same operations for both faces.
        bool          EnableStencilTest = false;
        CompareOp     StencilCompare    = CompareOp::Always;
        StencilOp     StencilPassOp     = StencilOp::Keep;
        unsigned char StencilReference  = 0;
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
        unsigned int WriteMask      = ColorWriteMask::All; // ColorWriteMask flags.
    };

    struct ColorTargetDesc
    {
        PixelFormat     Format = PixelFormat::RGBA8Unorm;
        BlendState Blend  = {};
    };

    // Formats and sample count must match the targets of the render passes the pipeline is used in.
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
        PixelFormat            DepthFormat                     = PixelFormat::Unknown; // Unknown for passes without a depth target.
        int               SampleCount                     = 1;
        std::string       Name                            = {};
    };

    struct ComputePipelineDesc
    {
        IShader*    Shader = nullptr;
        std::string Name   = {};
    };

    // Target of a render pass. Load and Store tell whether previous contents are kept and whether the result is
    // written back; DontCare lets tiled GPUs skip the memory traffic.
    struct ColorTargetInfo
    {
        ITexture* Texture    = nullptr;
        int       MipLevel   = 0;
        int       Layer      = 0; // Array layer, cube face or 3D depth slice.
        LoadOp    Load       = LoadOp::Clear;
        StoreOp   Store      = StoreOp::Store;
        float     ClearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // Used with LoadOp::Clear.
    };

    // Load and Store also apply to stencil. Mip level 0 only.
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

    // Result of IDevice::ConsumeGpuTimings for one debug group.
    struct GpuTiming
    {
        std::string Name         = {};   // Debug group name.
        float       Milliseconds = 0.0f; // Average over the samples.
        int         Samples      = 0;    // Measurements since the previous call.
    };

    struct BufferBinding
    {
        IBuffer* Buffer = nullptr;
        int      Offset = 0; // Bytes.
    };

    struct TextureSamplerBinding
    {
        ITexture* Texture = nullptr;
        ISampler* Sampler = nullptr;
    };

    // In pixels from the upper left corner of the target; depth range from 0 to 1.
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

    // Box of texels in one mip level and layer. Z and Depth are used by 3D textures only.
    struct TextureRegion
    {
        ITexture* Texture  = nullptr;
        int       MipLevel = 0;
        int       Layer    = 0; // Array layer or cube face.
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
        int              Offset         = 0; // Bytes.
        int              PixelsPerRow   = 0; // Row pitch in texels.
        int              RowsPerLayer   = 0; // Rows between layers or depth slices.
    };

    int  GetFormatBlockSize(PixelFormat format);      // Bytes per texel, or per 4x4 block of BC formats.
    int  GetFormatBlockDimension(PixelFormat format); // 4 for BC formats, otherwise 1.
    bool IsDepthFormat(PixelFormat format);
    bool HasStencil(PixelFormat format);
    int  GetTextureDataSize(PixelFormat format, int width, int height, int depth); // Bytes of one tightly packed mip.
}
