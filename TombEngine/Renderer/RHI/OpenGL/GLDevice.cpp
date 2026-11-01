#include "framework.h"
#include "Renderer/RHI/OpenGL/GLDevice.h"

#define GL_GLEXT_PROTOTYPES
#include <SDL3/SDL_opengl.h>

#include "Renderer/RHI/MemoryTracker.h"

// OpenGL 4.5 core backend. Shaders are GLSL generated from the SPIR-V by SPIRV-Cross, loaded as separate programs,
// and the clip control of Direct3D (upper left origin, depth from 0 to 1) keeps the conventions of the other backends:
// row 0 of every texture is the top of the image, so only the presentation to the window is flipped.
// Commands execute on the context as they are recorded, by one command buffer reused every frame. The context is
// current on one thread at a time: device calls and resource destructors make it current, and ReleaseThread hands it
// over (AcquireCommandBuffer takes it for the thread recording the frame). Uniforms go to
// a persistently mapped ring, fences are sync objects and GPU timings come from timestamp queries.

namespace TEN::Renderer::RHI::OpenGL
{
    constexpr auto REQUIRED_MAJOR_VERSION = 4;
    constexpr auto REQUIRED_MINOR_VERSION = 5;

    // Texture units and buffer bindings are shared by all stages in OpenGL. The GLSL keeps the per-stage bindings of
    // SDL GPU, so the bindings of each program are offset by the base of its stage when the program is created.
    // Bindings of storage resources do not outlive a pass, so compute passes reuse the vertex ranges.
    constexpr auto PIXEL_TEXTURE_UNIT_BASE   = MAX_SAMPLER_SLOTS;
    constexpr auto PIXEL_UNIFORM_BASE        = MAX_UNIFORM_SLOTS;
    constexpr auto COMPUTE_UNIFORM_BASE      = MAX_UNIFORM_SLOTS * 2;
    constexpr auto PIXEL_STORAGE_BASE        = MAX_SAMPLER_SLOTS + MAX_STORAGE_SLOTS;
    constexpr auto COMPUTE_RW_STORAGE_BASE   = MAX_SAMPLER_SLOTS + MAX_STORAGE_SLOTS;
    constexpr auto STORAGE_BINDINGS_REQUIRED = PIXEL_STORAGE_BASE * 2;

    constexpr auto UNIFORM_RING_SEGMENT_SIZE  = 4 * 1024 * 1024;
    constexpr auto UNIFORM_RING_SEGMENT_COUNT = 4;
    constexpr auto FENCE_WAIT_TIMEOUT         = 1000000000ULL; // Nanoseconds.

#define TEN_GL_FUNCTIONS(X) \
    X(BindBufferBase) X(BindBufferRange) X(BindFramebuffer) X(BindImageTexture) X(BindProgramPipeline) X(BindSampler) \
    X(BindTextureUnit) X(BindVertexArray) X(BlendEquationSeparatei) X(BlendFuncSeparatei) X(BlitNamedFramebuffer) \
    X(CheckNamedFramebufferStatus) X(ClearNamedFramebufferfi) X(ClearNamedFramebufferfv) X(ClearNamedFramebufferuiv) \
    X(ClientWaitSync) X(ClipControl) X(ColorMaski) X(CompressedTextureSubImage2D) X(CompressedTextureSubImage3D) \
    X(CopyImageSubData) X(CreateBuffers) X(CreateFramebuffers) X(CreateProgramPipelines) X(CreateQueries) \
    X(CreateSamplers) X(CreateShaderProgramv) X(CreateTextures) X(CreateVertexArrays) X(CullFace) \
    X(DebugMessageCallback) X(DebugMessageInsert) X(DeleteBuffers) X(DeleteFramebuffers) X(DeleteProgram) \
    X(DeleteProgramPipelines) X(DeleteQueries) X(DeleteSamplers) X(DeleteSync) X(DeleteTextures) X(DeleteVertexArrays) \
    X(DepthFunc) X(DepthMask) X(DepthRangeIndexed) X(Disable) X(Disablei) X(DispatchCompute) \
    X(DrawArraysInstancedBaseInstance) X(DrawElementsInstancedBaseVertexBaseInstance) X(Enable) X(Enablei) \
    X(EnableVertexArrayAttrib) X(FenceSync) X(Finish) X(Flush) X(FrontFace) X(GenerateTextureMipmap) \
    X(GetIntegerv) X(GetInternalformativ) X(GetProgramInfoLog) X(GetProgramInterfaceiv) X(GetProgramiv) \
    X(GetProgramResourceiv) X(GetProgramResourceName) X(GetQueryObjectiv) X(GetQueryObjectui64v) X(GetString) \
    X(GetTextureSubImage) X(GetUniformiv) X(MapNamedBufferRange) X(MemoryBarrier) X(NamedBufferStorage) \
    X(NamedBufferSubData) X(NamedFramebufferDrawBuffer) X(NamedFramebufferDrawBuffers) X(NamedFramebufferReadBuffer) \
    X(NamedFramebufferTexture) X(NamedFramebufferTextureLayer) X(ObjectLabel) X(PixelStorei) X(PolygonMode) \
    X(PolygonOffset) X(PopDebugGroup) X(ProgramUniform1i) X(PushDebugGroup) X(QueryCounter) X(SamplerParameterf) \
    X(SamplerParameteri) X(ScissorIndexed) X(ShaderStorageBlockBinding) X(StencilFunc) X(StencilOp) X(TextureStorage2D) \
    X(TextureStorage2DMultisample) X(TextureStorage3D) X(TextureSubImage2D) X(TextureSubImage3D) X(UniformBlockBinding) \
    X(UnmapNamedBuffer) X(UseProgramStages) X(VertexArrayAttribBinding) X(VertexArrayAttribFormat) \
    X(VertexArrayAttribIFormat) X(VertexArrayBindingDivisor) X(VertexArrayElementBuffer) X(VertexArrayVertexBuffer) \
    X(ViewportIndexedf)

    // Entry points of the context, loaded through SDL; the prototypes only give their types.
    struct GLFunctions
    {
#define TEN_GL_DECLARE(name) decltype(&::gl##name) name = nullptr;
        TEN_GL_FUNCTIONS(TEN_GL_DECLARE)
#undef TEN_GL_DECLARE
    };

    static GLFunctions g_GL = {};

    // Loads every entry point of TEN_GL_FUNCTIONS from the current context; false if any is missing.
    static bool LoadFunctions()
    {
        bool isComplete = true;

#define TEN_GL_LOAD(name)                                                                   \
        g_GL.name = (decltype(g_GL.name))SDL_GL_GetProcAddress("gl" #name);                 \
        if (g_GL.name == nullptr)                                                           \
        {                                                                                   \
            TENLog("OpenGL: missing entry point gl" #name ".", LogLevel::Warning);         \
            isComplete = false;                                                             \
        }

        TEN_GL_FUNCTIONS(TEN_GL_LOAD)
#undef TEN_GL_LOAD

        return isComplete;
    }

    // Internal format for storage, with the pixel format and type of client data; the last two are unused when compressed.
    struct GLFormat
    {
        GLenum Internal     = 0;
        GLenum Format       = 0;
        GLenum Type         = 0;
        bool   IsCompressed = false;
    };

    static GLFormat ToGL(PixelFormat format)
    {
        switch (format)
        {
        case PixelFormat::R8Unorm:        return { GL_R8, GL_RED, GL_UNSIGNED_BYTE };
        case PixelFormat::RG8Unorm:       return { GL_RG8, GL_RG, GL_UNSIGNED_BYTE };
        case PixelFormat::RGBA8Unorm:     return { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE };
        case PixelFormat::RGBA8UnormSrgb: return { GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE };
        case PixelFormat::BGRA8Unorm:     return { GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE };
        case PixelFormat::BGRA8UnormSrgb: return { GL_SRGB8_ALPHA8, GL_BGRA, GL_UNSIGNED_BYTE };
        case PixelFormat::R16Float:       return { GL_R16F, GL_RED, GL_HALF_FLOAT };
        case PixelFormat::RG16Float:      return { GL_RG16F, GL_RG, GL_HALF_FLOAT };
        case PixelFormat::RGBA16Float:    return { GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT };
        case PixelFormat::R32Float:       return { GL_R32F, GL_RED, GL_FLOAT };
        case PixelFormat::RG32Float:      return { GL_RG32F, GL_RG, GL_FLOAT };
        case PixelFormat::RGBA32Float:    return { GL_RGBA32F, GL_RGBA, GL_FLOAT };
        case PixelFormat::R11G11B10Float: return { GL_R11F_G11F_B10F, GL_RGB, GL_UNSIGNED_INT_10F_11F_11F_REV };
        case PixelFormat::R32Uint:        return { GL_R32UI, GL_RED_INTEGER, GL_UNSIGNED_INT };
        case PixelFormat::BC1Unorm:       return { GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, 0, 0, true };
        case PixelFormat::BC2Unorm:       return { GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, 0, 0, true };
        case PixelFormat::BC3Unorm:       return { GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, 0, 0, true };
        case PixelFormat::BC4Unorm:       return { GL_COMPRESSED_RED_RGTC1, 0, 0, true };
        case PixelFormat::BC5Unorm:       return { GL_COMPRESSED_RG_RGTC2, 0, 0, true };
        case PixelFormat::BC7Unorm:       return { GL_COMPRESSED_RGBA_BPTC_UNORM, 0, 0, true };
        case PixelFormat::D16Unorm:       return { GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT };
        case PixelFormat::D24UnormS8Uint: return { GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8 };
        case PixelFormat::D32Float:       return { GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT };
        case PixelFormat::D32FloatS8Uint: return { GL_DEPTH32F_STENCIL8, GL_DEPTH_STENCIL, GL_FLOAT_32_UNSIGNED_INT_24_8_REV };
        default:                          return {};
        }
    }

    static GLenum ToGLTarget(TextureType type, int sampleCount)
    {
        switch (type)
        {
        case TextureType::Texture2DArray: return GL_TEXTURE_2D_ARRAY;
        case TextureType::TextureCube:    return GL_TEXTURE_CUBE_MAP;
        case TextureType::Texture3D:      return GL_TEXTURE_3D;
        default:                          return (sampleCount > 1) ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
        }
    }

    struct GLVertexFormat
    {
        int    Size         = 0;
        GLenum Type         = GL_FLOAT;
        bool   IsNormalized = false;
        bool   IsInteger    = false;
    };

    static GLVertexFormat ToGL(VertexElementFormat format)
    {
        switch (format)
        {
        case VertexElementFormat::Float:       return { 1, GL_FLOAT };
        case VertexElementFormat::Float2:      return { 2, GL_FLOAT };
        case VertexElementFormat::Float3:      return { 3, GL_FLOAT };
        case VertexElementFormat::Float4:      return { 4, GL_FLOAT };
        case VertexElementFormat::UByte4:      return { 4, GL_UNSIGNED_BYTE, false, true };
        case VertexElementFormat::UByte4Norm:  return { 4, GL_UNSIGNED_BYTE, true };
        case VertexElementFormat::Byte4Norm:   return { 4, GL_BYTE, true };
        case VertexElementFormat::Short2Norm:  return { 2, GL_SHORT, true };
        case VertexElementFormat::UShort2Norm: return { 2, GL_UNSIGNED_SHORT, true };
        case VertexElementFormat::Short4Norm:  return { 4, GL_SHORT, true };
        case VertexElementFormat::Half2:       return { 2, GL_HALF_FLOAT };
        case VertexElementFormat::Half4:       return { 4, GL_HALF_FLOAT };
        case VertexElementFormat::UInt:        return { 1, GL_UNSIGNED_INT, false, true };
        default:                               return { 4, GL_FLOAT };
        }
    }

    static GLenum ToGL(CompareOp op)
    {
        switch (op)
        {
        case CompareOp::Never:          return GL_NEVER;
        case CompareOp::Less:           return GL_LESS;
        case CompareOp::Equal:          return GL_EQUAL;
        case CompareOp::LessOrEqual:    return GL_LEQUAL;
        case CompareOp::Greater:        return GL_GREATER;
        case CompareOp::NotEqual:       return GL_NOTEQUAL;
        case CompareOp::GreaterOrEqual: return GL_GEQUAL;
        default:                        return GL_ALWAYS;
        }
    }

    static GLenum ToGL(StencilOp op)
    {
        switch (op)
        {
        case StencilOp::Zero:    return GL_ZERO;
        case StencilOp::Replace: return GL_REPLACE;
        default:                 return GL_KEEP;
        }
    }

    static GLenum ToGL(BlendFactor factor)
    {
        switch (factor)
        {
        case BlendFactor::Zero:             return GL_ZERO;
        case BlendFactor::SrcColor:         return GL_SRC_COLOR;
        case BlendFactor::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
        case BlendFactor::DstColor:         return GL_DST_COLOR;
        case BlendFactor::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
        case BlendFactor::SrcAlpha:         return GL_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::DstAlpha:         return GL_DST_ALPHA;
        case BlendFactor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
        default:                            return GL_ONE;
        }
    }

    static GLenum ToGL(BlendOp op)
    {
        switch (op)
        {
        case BlendOp::Subtract:        return GL_FUNC_SUBTRACT;
        case BlendOp::ReverseSubtract: return GL_FUNC_REVERSE_SUBTRACT;
        case BlendOp::Min:             return GL_MIN;
        case BlendOp::Max:             return GL_MAX;
        default:                       return GL_FUNC_ADD;
        }
    }

    static GLenum ToGL(AddressMode mode)
    {
        switch (mode)
        {
        case AddressMode::MirroredRepeat: return GL_MIRRORED_REPEAT;
        case AddressMode::ClampToEdge:    return GL_CLAMP_TO_EDGE;
        default:                          return GL_REPEAT;
        }
    }

    static GLenum ToGL(PrimitiveType type)
    {
        switch (type)
        {
        case PrimitiveType::TriangleStrip: return GL_TRIANGLE_STRIP;
        case PrimitiveType::LineList:      return GL_LINES;
        case PrimitiveType::LineStrip:     return GL_LINE_STRIP;
        case PrimitiveType::PointList:     return GL_POINTS;
        default:                           return GL_TRIANGLES;
        }
    }

    // OpenGL combines minification and mip filtering in one value; anisotropic filtering needs trilinear filtering.
    static GLenum ToGLMinFilter(const SamplerDesc& desc)
    {
        if (desc.MaxAnisotropy > 1)
            return GL_LINEAR_MIPMAP_LINEAR;

        if (desc.MinFilter == Filter::Linear)
            return (desc.MipFilter == Filter::Linear) ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST;

        return (desc.MipFilter == Filter::Linear) ? GL_NEAREST_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST;
    }

    // Uniform types that use a texture unit, for OffsetTextureUnits.
    static bool IsSamplerType(GLenum type)
    {
        switch (type)
        {
        case GL_SAMPLER_2D:
        case GL_SAMPLER_3D:
        case GL_SAMPLER_CUBE:
        case GL_SAMPLER_2D_SHADOW:
        case GL_SAMPLER_2D_ARRAY:
        case GL_SAMPLER_2D_ARRAY_SHADOW:
        case GL_SAMPLER_CUBE_SHADOW:
        case GL_SAMPLER_2D_MULTISAMPLE:
        case GL_INT_SAMPLER_2D:
        case GL_INT_SAMPLER_2D_ARRAY:
        case GL_INT_SAMPLER_3D:
        case GL_UNSIGNED_INT_SAMPLER_2D:
        case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
        case GL_UNSIGNED_INT_SAMPLER_3D:
            return true;

        default:
            return false;
        }
    }

    static void SetDebugName(GLenum identifier, GLuint name, const std::string& label)
    {
        if (name != 0 && !label.empty())
            g_GL.ObjectLabel(identifier, name, -1, label.c_str());
    }

    // Debug output callback of debug contexts; logs each message ID once, without notifications.
    static void APIENTRY OnDebugMessage(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
    {
        static auto loggedMessages = std::unordered_set<GLuint>{};

        if (severity == GL_DEBUG_SEVERITY_NOTIFICATION || !loggedMessages.insert(id).second)
            return;

        auto level = (severity == GL_DEBUG_SEVERITY_HIGH) ? LogLevel::Error : LogLevel::Warning;
        TENLog(std::string("OpenGL debug output: ") + message, level);
    }

    // Framebuffer objects for each combination of attachments used as a target, created on first use.
    struct FramebufferKey
    {
        // Texture and packed mip and layer of each color target, then of the depth target, then the color target count.
        std::array<unsigned int, (MAX_COLOR_TARGETS + 1) * 2 + 1> Values = {};

        bool operator==(const FramebufferKey& other) const { return Values == other.Values; }

        bool Contains(GLuint texture) const
        {
            for (int i = 0; i < (MAX_COLOR_TARGETS + 1) * 2; i += 2)
            {
                if (Values[i] == texture)
                    return true;
            }

            return false;
        }
    };

    struct FramebufferKeyHash
    {
        size_t operator()(const FramebufferKey& key) const
        {
            size_t hash = 0;
            for (auto value : key.Values)
                hash = (hash * 31) ^ value;

            return hash;
        }
    };

    // Shared by the device and its resources: resources released after the device find the context gone.
    class GLContext
    {
    public:
        SDL_Window*   Window  = nullptr;
        SDL_GLContext Handle  = nullptr;
        bool          IsAlive = false;

        std::unordered_map<FramebufferKey, GLuint, FramebufferKeyHash> Framebuffers = {};

        // Takes the context to the calling thread if another thread does not hold it; false once the device is gone.
        bool MakeCurrent()
        {
            if (!IsAlive)
                return false;

            if (SDL_GL_GetCurrentContext() == Handle)
                return true;

            if (SDL_GL_MakeCurrent(Window, Handle))
                return true;

            TENLog(std::string("OpenGL: cannot make the context current: ") + SDL_GetError(), LogLevel::Error);
            return false;
        }

        // Deletes the cached framebuffers that attach a texture about to be deleted.
        void ReleaseFramebuffers(GLuint texture)
        {
            for (auto it = Framebuffers.begin(); it != Framebuffers.end();)
            {
                if (it->first.Contains(texture))
                {
                    g_GL.DeleteFramebuffers(1, &it->second);
                    it = Framebuffers.erase(it);
                }
                else
                {
                    it++;
                }
            }
        }
    };

    class GLBuffer : public IBuffer
    {
    public:
        std::shared_ptr<GLContext> Context    = nullptr;
        GLuint                     Id         = 0;
        int                        Size       = 0;
        unsigned int               Usage      = 0;
        MemoryAllocation           Allocation = {};

        ~GLBuffer() override
        {
            if (Id != 0 && Context->MakeCurrent())
                g_GL.DeleteBuffers(1, &Id);
        }

        int          GetSize() const override { return Size; }
        unsigned int GetUsage() const override { return Usage; }
    };

    class GLTexture : public ITexture
    {
    public:
        std::shared_ptr<GLContext> Context    = nullptr;
        GLuint                     Id         = 0; // Zero for the window framebuffer.
        GLenum                     Target     = GL_TEXTURE_2D;
        GLFormat                   Format     = {};
        TextureDesc                Desc       = {};
        MemoryAllocation           Allocation = {};

        ~GLTexture() override
        {
            if (Id == 0 || !Context->MakeCurrent())
                return;

            Context->ReleaseFramebuffers(Id);
            g_GL.DeleteTextures(1, &Id);
        }

        const TextureDesc& GetDesc() const override { return Desc; }
    };

    class GLSampler : public ISampler
    {
    public:
        std::shared_ptr<GLContext> Context = nullptr;
        GLuint                     Id      = 0;

        ~GLSampler() override
        {
            if (Id != 0 && Context->MakeCurrent())
                g_GL.DeleteSamplers(1, &Id);
        }
    };

    class GLShader : public IShader
    {
    public:
        std::shared_ptr<GLContext> Context = nullptr;
        GLuint                     Program = 0;
        ShaderDesc                 Desc    = {};

        ~GLShader() override
        {
            if (Program != 0 && Context->MakeCurrent())
                g_GL.DeleteProgram(Program);
        }

        const ShaderDesc& GetDesc() const override { return Desc; }
    };

    struct GLBlendTarget
    {
        bool         IsEnabled = false;
        GLenum       Src       = GL_ONE;
        GLenum       Dst       = GL_ZERO;
        GLenum       SrcAlpha  = GL_ONE;
        GLenum       DstAlpha  = GL_ZERO;
        GLenum       Op        = GL_FUNC_ADD;
        GLenum       OpAlpha   = GL_FUNC_ADD;
        unsigned int WriteMask = ColorWriteMask::All;
    };

    // Fixed function state of a pipeline, applied when the pipeline is bound. The front face is the one of
    // texture targets; the window framebuffer is flipped, which inverts it.
    struct GLRenderState
    {
        GLBlendTarget Blend[MAX_COLOR_TARGETS] = {};
        bool          IsCullEnabled            = false;
        GLenum        CullFace                 = GL_BACK;
        GLenum        FrontFace                = GL_CCW;
        GLenum        PolygonMode              = GL_FILL;
        bool          IsDepthBiasEnabled       = false;
        float         DepthBiasConstant        = 0.0f;
        float         DepthBiasSlope           = 0.0f;
        bool          IsDepthClampEnabled      = false;
        bool          IsDepthTestEnabled       = false;
        bool          IsDepthWriteEnabled      = false;
        GLenum        DepthFunc                = GL_LEQUAL;
        bool          IsStencilTestEnabled     = false;
        GLenum        StencilFunc              = GL_ALWAYS;
        GLenum        StencilPassOp            = GL_KEEP;
        int           StencilReference         = 0;
    };

    // A program pipeline of the separate vertex and pixel programs, with a vertex array holding the attribute layout.
    class GLGraphicsPipeline : public IGraphicsPipeline
    {
    public:
        std::shared_ptr<GLContext> Context         = nullptr;
        GLuint                     ProgramPipeline = 0;
        GLuint                     VertexArray     = 0;
        GLenum                     Mode            = GL_TRIANGLES;
        PrimitiveType              Primitive       = PrimitiveType::TriangleList;
        GLRenderState              State           = {};
        int                        VertexStrides[MAX_VERTEX_BUFFERS] = {};

        // Storage buffers follow sampled and storage textures in the binding range of each stage.
        int VertexStorageBufferBase = 0;
        int PixelStorageBufferBase  = 0;

        ~GLGraphicsPipeline() override
        {
            if (!Context->MakeCurrent())
                return;

            g_GL.DeleteProgramPipelines(1, &ProgramPipeline);
            g_GL.DeleteVertexArrays(1, &VertexArray);
        }
    };

    class GLComputePipeline : public IComputePipeline
    {
    public:
        std::shared_ptr<GLContext> Context           = nullptr;
        GLuint                     ProgramPipeline   = 0;
        int                        StorageBufferBase = 0;

        ~GLComputePipeline() override
        {
            if (ProgramPipeline != 0 && Context->MakeCurrent())
                g_GL.DeleteProgramPipelines(1, &ProgramPipeline);
        }
    };

    // OpenGL copies upload data at call time and reads downloads back synchronously, so a transfer buffer is plain
    // CPU memory.
    class GLTransferBuffer : public ITransferBuffer
    {
    public:
        std::vector<unsigned char> Data       = {};
        MemoryAllocation           Allocation = {};

        int GetSize() const override { return (int)Data.size(); }
    };

    class GLFence : public IFence
    {
    public:
        std::shared_ptr<GLContext> Context = nullptr;
        GLsync                     Sync    = nullptr;

        ~GLFence() override
        {
            if (Sync != nullptr && Context->MakeCurrent())
                g_GL.DeleteSync(Sync);
        }
    };

    static GLuint GetId(ITexture* texture)
    {
        return (texture != nullptr) ? ((GLTexture*)texture)->Id : 0;
    }

    static GLuint GetId(ISampler* sampler)
    {
        return (sampler != nullptr) ? ((GLSampler*)sampler)->Id : 0;
    }

    static GLuint GetId(IBuffer* buffer)
    {
        return (buffer != nullptr) ? ((GLBuffer*)buffer)->Id : 0;
    }

    // Waits for the fence and deletes it.
    static void WaitForSync(GLsync& sync)
    {
        if (sync == nullptr)
            return;

        while (g_GL.ClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT, FENCE_WAIT_TIMEOUT) == GL_TIMEOUT_EXPIRED)
            TENLog("OpenGL: GPU fence wait timed out.", LogLevel::Warning);

        g_GL.DeleteSync(sync);
        sync = nullptr;
    }

    // Uniform pushes go to a persistently mapped ring split into segments. A fence marks the end of each segment,
    // and writing wraps into a segment only after the GPU has read its previous contents.
    class GLUniformRing
    {
    private:
        GLuint         _buffer    = 0;
        unsigned char* _data      = nullptr;
        int            _offset    = 0;
        int            _segment   = 0;
        int            _alignment = 256;
        GLsync         _fences[UNIFORM_RING_SEGMENT_COUNT] = {};

    public:
        bool Initialize()
        {
            GLint alignment = 0;
            g_GL.GetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);
            _alignment = std::max(alignment, 16);

            constexpr auto FLAGS = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
            constexpr auto SIZE = UNIFORM_RING_SEGMENT_SIZE * UNIFORM_RING_SEGMENT_COUNT;

            g_GL.CreateBuffers(1, &_buffer);
            g_GL.NamedBufferStorage(_buffer, SIZE, nullptr, FLAGS);
            _data = (unsigned char*)g_GL.MapNamedBufferRange(_buffer, 0, SIZE, FLAGS);
            SetDebugName(GL_BUFFER, _buffer, "Uniform ring");
            return (_data != nullptr);
        }

        void Release()
        {
            for (auto& fence : _fences)
            {
                if (fence != nullptr)
                    g_GL.DeleteSync(fence);

                fence = nullptr;
            }

            if (_buffer != 0)
            {
                g_GL.UnmapNamedBuffer(_buffer);
                g_GL.DeleteBuffers(1, &_buffer);
            }

            _buffer = 0;
            _data = nullptr;
        }

        GLuint GetBuffer() const
        {
            return _buffer;
        }

        // Returns the offset of the data, which has room for the size rounded up to the alignment.
        int Write(const void* data, int size)
        {
            int reservedSize = GetReservedSize(size);
            int offset = Align(_offset);
            if ((offset + reservedSize) > ((_segment + 1) * UNIFORM_RING_SEGMENT_SIZE))
            {
                _fences[_segment] = g_GL.FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
                _segment = (_segment + 1) % UNIFORM_RING_SEGMENT_COUNT;
                WaitForSync(_fences[_segment]);
                offset = _segment * UNIFORM_RING_SEGMENT_SIZE;
            }

            memcpy(_data + offset, data, size);
            _offset = offset + reservedSize;
            return offset;
        }

        int GetReservedSize(int size) const
        {
            return Align(size);
        }

    private:
        int Align(int value) const
        {
            return (value + _alignment - 1) / _alignment * _alignment;
        }
    };

    // Timestamp queries around the outermost debug group (the frame) and the groups directly inside it.
    // Results are read a few frames later without stalling; frames still pending when their slot is reused are dropped.
    class GLGpuProfiler
    {
    private:
        static constexpr int FRAME_LATENCY = 16; // Two per frame: the scene and the frame finish groups.
        static constexpr int SCOPES_MAX    = 32;

        struct Scope
        {
            std::string Name  = {};
            GLuint      Begin = 0;
            GLuint      End   = 0;
        };

        struct Frame
        {
            Scope Scopes[SCOPES_MAX] = {};
            int   ScopeCount         = 0;
            bool  IsPending          = false;
        };

        Frame                  _frames[FRAME_LATENCY] = {};
        int                    _frameIndex    = 0;
        int                    _openScopes[2] = { -1, -1 };
        std::vector<GpuTiming> _totals        = {};

        void Accumulate(const std::string& name, float milliseconds)
        {
            for (auto& total : _totals)
            {
                if (total.Name == name)
                {
                    total.Milliseconds += milliseconds;
                    total.Samples++;
                    return;
                }
            }

            _totals.push_back(GpuTiming{ name, milliseconds, 1 });
        }

        void Resolve(Frame& frame)
        {
            frame.IsPending = false;

            for (int i = 0; i < frame.ScopeCount; i++)
            {
                GLint isAvailable = GL_FALSE;
                g_GL.GetQueryObjectiv(frame.Scopes[i].End, GL_QUERY_RESULT_AVAILABLE, &isAvailable);
                if (!isAvailable)
                    return;

                GLuint64 begin = 0;
                GLuint64 end = 0;
                g_GL.GetQueryObjectui64v(frame.Scopes[i].Begin, GL_QUERY_RESULT, &begin);
                g_GL.GetQueryObjectui64v(frame.Scopes[i].End, GL_QUERY_RESULT, &end);

                // A scope whose end query was not issued in this frame still holds an older timestamp.
                if (end < begin)
                    continue;

                Accumulate(frame.Scopes[i].Name, (float)((double)(end - begin) / 1000000.0));
            }
        }

        void BeginScope(Frame& frame, int level, const char* name)
        {
            if (frame.ScopeCount >= SCOPES_MAX)
                return;

            auto& scope = frame.Scopes[frame.ScopeCount];
            if (scope.Begin == 0)
            {
                g_GL.CreateQueries(GL_TIMESTAMP, 1, &scope.Begin);
                g_GL.CreateQueries(GL_TIMESTAMP, 1, &scope.End);
            }

            scope.Name = name;
            g_GL.QueryCounter(scope.Begin, GL_TIMESTAMP);
            _openScopes[level] = frame.ScopeCount++;
        }

        void EndScope(Frame& frame, int level)
        {
            if (_openScopes[level] >= 0)
                g_GL.QueryCounter(frame.Scopes[_openScopes[level]].End, GL_TIMESTAMP);

            _openScopes[level] = -1;
        }

    public:
        void Release()
        {
            for (auto& frame : _frames)
            {
                for (auto& scope : frame.Scopes)
                {
                    if (scope.Begin != 0)
                    {
                        g_GL.DeleteQueries(1, &scope.Begin);
                        g_GL.DeleteQueries(1, &scope.End);
                    }

                    scope.Begin = scope.End = 0;
                }
            }
        }

        void OnPushGroup(int depth, const char* name)
        {
            if (depth > 2)
                return;

            auto& frame = _frames[_frameIndex];
            if (depth == 1)
            {
                if (frame.IsPending)
                    Resolve(frame);

                frame.ScopeCount = 0;
            }

            BeginScope(frame, depth - 1, name);
        }

        void OnPopGroup(int depth)
        {
            if (depth > 2)
                return;

            auto& frame = _frames[_frameIndex];
            EndScope(frame, depth - 1);

            if (depth == 1)
            {
                frame.IsPending = true;
                _frameIndex = (_frameIndex + 1) % FRAME_LATENCY;
            }
        }

        std::vector<GpuTiming> Consume()
        {
            auto result = std::move(_totals);
            _totals.clear();
            for (auto& timing : result)
                timing.Milliseconds /= (float)std::max(timing.Samples, 1);

            return result;
        }
    };

    // Context state last set by the command buffer, to skip redundant calls.
    struct GLStateCache
    {
        GLRenderState State        = {};
        bool          IsStateValid = false;
        GLenum        ClipOrigin   = GL_UPPER_LEFT;
        bool          IsSrgbWrite  = true;
    };

    class GLRenderPass : public IRenderPass
    {
    public:
        GLStateCache*       Cache    = nullptr;
        GLGraphicsPipeline* Pipeline = nullptr;
        CommandCounters*    Counters = nullptr;

        bool IsWindowTarget = false; // The window framebuffer is drawn upside down: its origin is the lower left.
        int  TargetHeight   = 0;

        // Vertex buffer bindings belong to the vertex array of each pipeline, so they are kept and bound again
        // with the strides of each new pipeline, as on DX11.
        GLuint     VertexBuffers[MAX_VERTEX_BUFFERS]       = {};
        GLintptr   VertexBufferOffsets[MAX_VERTEX_BUFFERS] = {};
        int        VertexBufferCount                       = 0;
        GLuint     IndexBuffer                             = 0;
        int        IndexOffset                             = 0;
        GLenum     IndexType                               = GL_UNSIGNED_SHORT;
        int        IndexSizeBytes                          = 2;

        void Reset()
        {
            Pipeline = nullptr;
            VertexBufferCount = 0;
            IndexBuffer = 0;
        }

        void BindPipeline(IGraphicsPipeline* pipeline) override
        {
            auto* glPipeline = (GLGraphicsPipeline*)pipeline;
            if (glPipeline == Pipeline)
                return;

            Counters->PipelineBinds++;
            Pipeline = glPipeline;
            g_GL.BindProgramPipeline(glPipeline->ProgramPipeline);
            g_GL.BindVertexArray(glPipeline->VertexArray);
            ApplyState(glPipeline->State);
            ApplyVertexBuffers(0, VertexBufferCount);
            g_GL.VertexArrayElementBuffer(glPipeline->VertexArray, IndexBuffer);
        }

        void SetViewport(const Viewport& viewport) override
        {
            float y = IsWindowTarget ? ((float)TargetHeight - viewport.Y - viewport.Height) : viewport.Y;
            g_GL.ViewportIndexedf(0, viewport.X, y, viewport.Width, viewport.Height);
            g_GL.DepthRangeIndexed(0, viewport.MinDepth, viewport.MaxDepth);
        }

        void SetScissor(const ScissorRect& scissor) override
        {
            int y = IsWindowTarget ? (TargetHeight - scissor.Y - scissor.Height) : scissor.Y;
            g_GL.ScissorIndexed(0, scissor.X, y, scissor.Width, scissor.Height);
        }

        void BindVertexBuffers(int firstSlot, const BufferBinding* bindings, int count) override
        {
            for (int i = 0; i < count; i++)
            {
                VertexBuffers[firstSlot + i] = GetId(bindings[i].Buffer);
                VertexBufferOffsets[firstSlot + i] = bindings[i].Offset;
            }

            VertexBufferCount = std::max(VertexBufferCount, firstSlot + count);
            Counters->VertexBufferBinds += count;
            ApplyVertexBuffers(firstSlot, count);
        }

        void BindIndexBuffer(const BufferBinding& binding, IndexSize indexSize) override
        {
            IndexBuffer = GetId(binding.Buffer);
            IndexOffset = binding.Offset;
            IndexType = (indexSize == IndexSize::Bits16) ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT;
            IndexSizeBytes = (indexSize == IndexSize::Bits16) ? 2 : 4;
            Counters->IndexBufferBinds++;

            if (Pipeline != nullptr)
                g_GL.VertexArrayElementBuffer(Pipeline->VertexArray, IndexBuffer);
        }

        void BindVertexSamplers(int firstSlot, const TextureSamplerBinding* bindings, int count) override
        {
            BindSamplers(firstSlot, bindings, count);
        }

        void BindVertexStorageBuffers(int firstSlot, IBuffer* const* buffers, int count) override
        {
            int base = (Pipeline != nullptr) ? Pipeline->VertexStorageBufferBase : 0;
            BindStorageBuffers(base + firstSlot, buffers, count);
        }

        // Pixel texture units and storage bindings follow the vertex ones (see OffsetBindings).
        void BindPixelSamplers(int firstSlot, const TextureSamplerBinding* bindings, int count) override
        {
            BindSamplers(PIXEL_TEXTURE_UNIT_BASE + firstSlot, bindings, count);
        }

        void BindPixelStorageBuffers(int firstSlot, IBuffer* const* buffers, int count) override
        {
            int base = (Pipeline != nullptr) ? Pipeline->PixelStorageBufferBase : 0;
            BindStorageBuffers(PIXEL_STORAGE_BASE + base + firstSlot, buffers, count);
        }

        void Draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance) override
        {
            auto mode = (Pipeline != nullptr) ? Pipeline->Mode : GL_TRIANGLES;
            g_GL.DrawArraysInstancedBaseInstance(mode, firstVertex, vertexCount, instanceCount, firstInstance);
            Counters->AddDraw((Pipeline != nullptr) ? Pipeline->Primitive : PrimitiveType::TriangleList, vertexCount, instanceCount, false);
        }

        void DrawIndexed(int indexCount, int instanceCount, int firstIndex, int vertexOffset, int firstInstance) override
        {
            auto mode = (Pipeline != nullptr) ? Pipeline->Mode : GL_TRIANGLES;
            // With an element buffer bound, the index pointer is a byte offset into it.
            auto* indices = (const void*)(GLintptr)(IndexOffset + (firstIndex * IndexSizeBytes));
            g_GL.DrawElementsInstancedBaseVertexBaseInstance(mode, indexCount, IndexType, indices, instanceCount, vertexOffset, firstInstance);
            Counters->AddDraw((Pipeline != nullptr) ? Pipeline->Primitive : PrimitiveType::TriangleList, indexCount, instanceCount, true);
        }

        static void BindSamplers(int firstUnit, const TextureSamplerBinding* bindings, int count)
        {
            for (int i = 0; i < count; i++)
            {
                g_GL.BindTextureUnit(firstUnit + i, GetId(bindings[i].Texture));
                g_GL.BindSampler(firstUnit + i, GetId(bindings[i].Sampler));
            }
        }

        void BindStorageBuffers(int firstBinding, IBuffer* const* buffers, int count)
        {
            for (int i = 0; i < count; i++)
                g_GL.BindBufferBase(GL_SHADER_STORAGE_BUFFER, firstBinding + i, GetId(buffers[i]));

            Counters->StorageBufferBinds += count;
        }

    private:
        void ApplyVertexBuffers(int firstSlot, int count)
        {
            if (Pipeline == nullptr)
                return;

            for (int i = firstSlot; i < (firstSlot + count); i++)
                g_GL.VertexArrayVertexBuffer(Pipeline->VertexArray, i, VertexBuffers[i], VertexBufferOffsets[i], Pipeline->VertexStrides[i]);
        }

        // Sets the blend state of one draw buffer where it differs from the cache, or always when forced.
        void ApplyBlendState(const GLBlendTarget& target, GLBlendTarget& current, int index, bool isForced)
        {
            if (isForced || target.IsEnabled != current.IsEnabled)
            {
                if (target.IsEnabled)
                    g_GL.Enablei(GL_BLEND, index);
                else
                    g_GL.Disablei(GL_BLEND, index);
            }

            if (isForced || target.Src != current.Src || target.Dst != current.Dst || target.SrcAlpha != current.SrcAlpha || target.DstAlpha != current.DstAlpha)
                g_GL.BlendFuncSeparatei(index, target.Src, target.Dst, target.SrcAlpha, target.DstAlpha);

            if (isForced || target.Op != current.Op || target.OpAlpha != current.OpAlpha)
                g_GL.BlendEquationSeparatei(index, target.Op, target.OpAlpha);

            if (isForced || target.WriteMask != current.WriteMask)
            {
                g_GL.ColorMaski(index, (target.WriteMask & ColorWriteMask::Red) != 0, (target.WriteMask & ColorWriteMask::Green) != 0,
                    (target.WriteMask & ColorWriteMask::Blue) != 0, (target.WriteMask & ColorWriteMask::Alpha) != 0);
            }

            current = target;
        }

        static void SetCapability(GLenum capability, bool isEnabled)
        {
            if (isEnabled)
                g_GL.Enable(capability);
            else
                g_GL.Disable(capability);
        }

        // Applies the fixed function state of a pipeline, skipping values the context already has.
        void ApplyState(const GLRenderState& state)
        {
            auto& current = Cache->State;
            bool isForced = !Cache->IsStateValid;

            for (int i = 0; i < MAX_COLOR_TARGETS; i++)
                ApplyBlendState(state.Blend[i], current.Blend[i], i, isForced);

            if (isForced || state.IsCullEnabled != current.IsCullEnabled)
                SetCapability(GL_CULL_FACE, state.IsCullEnabled);

            if (isForced || state.CullFace != current.CullFace)
                g_GL.CullFace(state.CullFace);

            auto frontFace = IsWindowTarget ? ((state.FrontFace == GL_CCW) ? GL_CW : GL_CCW) : state.FrontFace;
            if (isForced || frontFace != current.FrontFace)
                g_GL.FrontFace(frontFace);

            if (isForced || state.PolygonMode != current.PolygonMode)
                g_GL.PolygonMode(GL_FRONT_AND_BACK, state.PolygonMode);

            if (isForced || state.IsDepthBiasEnabled != current.IsDepthBiasEnabled)
                SetCapability(GL_POLYGON_OFFSET_FILL, state.IsDepthBiasEnabled);

            if (isForced || state.DepthBiasSlope != current.DepthBiasSlope || state.DepthBiasConstant != current.DepthBiasConstant)
                g_GL.PolygonOffset(state.DepthBiasSlope, state.DepthBiasConstant);

            if (isForced || state.IsDepthClampEnabled != current.IsDepthClampEnabled)
                SetCapability(GL_DEPTH_CLAMP, state.IsDepthClampEnabled);

            if (isForced || state.IsDepthTestEnabled != current.IsDepthTestEnabled)
                SetCapability(GL_DEPTH_TEST, state.IsDepthTestEnabled);

            if (isForced || state.IsDepthWriteEnabled != current.IsDepthWriteEnabled)
                g_GL.DepthMask(state.IsDepthWriteEnabled ? GL_TRUE : GL_FALSE);

            if (isForced || state.DepthFunc != current.DepthFunc)
                g_GL.DepthFunc(state.DepthFunc);

            if (isForced || state.IsStencilTestEnabled != current.IsStencilTestEnabled)
                SetCapability(GL_STENCIL_TEST, state.IsStencilTestEnabled);

            if (isForced || state.StencilFunc != current.StencilFunc || state.StencilReference != current.StencilReference)
                g_GL.StencilFunc(state.StencilFunc, state.StencilReference, 0xFF);

            if (isForced || state.StencilPassOp != current.StencilPassOp)
                g_GL.StencilOp(GL_KEEP, GL_KEEP, state.StencilPassOp);

            current = state;
            current.FrontFace = frontFace;
            Cache->IsStateValid = true;
        }
    };

    class GLCopyPass : public ICopyPass
    {
    public:
        CommandCounters* Counters = nullptr;

        // Data is copied at call time and the driver keeps resources in use intact, so cycle is not needed.
        void UploadToBuffer(ITransferBuffer* source, int sourceOffset, IBuffer* destination, int destinationOffset, int size, bool cycle) override
        {
            const auto& data = ((GLTransferBuffer*)source)->Data;
            g_GL.NamedBufferSubData(GetId(destination), destinationOffset, size, data.data() + sourceOffset);
            Counters->Uploads++;
            Counters->UploadBytes += size;
        }

        void UploadToTexture(const TextureTransferInfo& source, const TextureRegion& destination, bool cycle) override
        {
            auto* texture = (GLTexture*)destination.Texture;
            const auto& desc = texture->Desc;
            const auto* data = ((GLTransferBuffer*)source.TransferBuffer)->Data.data() + source.Offset;

            int pixelsPerRow = (source.PixelsPerRow > 0) ? source.PixelsPerRow : destination.Width;
            int rowsPerLayer = (source.RowsPerLayer > 0) ? source.RowsPerLayer : destination.Height;
            bool is3D = (desc.Type == TextureType::Texture3D);
            bool isLayered = (texture->Target != GL_TEXTURE_2D);
            int z = is3D ? destination.Z : destination.Layer;
            int depth = is3D ? destination.Depth : 1;

            if (texture->Format.IsCompressed)
            {
                UploadCompressed(*texture, destination, data, pixelsPerRow, rowsPerLayer, z, depth);
            }
            else
            {
                // Zero row length and image height mean tightly packed rows.
                g_GL.PixelStorei(GL_UNPACK_ROW_LENGTH, (pixelsPerRow != destination.Width) ? pixelsPerRow : 0);
                g_GL.PixelStorei(GL_UNPACK_IMAGE_HEIGHT, (rowsPerLayer != destination.Height) ? rowsPerLayer : 0);

                const auto& format = texture->Format;
                if (isLayered)
                {
                    g_GL.TextureSubImage3D(texture->Id, destination.MipLevel, destination.X, destination.Y, z,
                        destination.Width, destination.Height, depth, format.Format, format.Type, data);
                }
                else
                {
                    g_GL.TextureSubImage2D(texture->Id, destination.MipLevel, destination.X, destination.Y,
                        destination.Width, destination.Height, format.Format, format.Type, data);
                }

                g_GL.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                g_GL.PixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
            }

            Counters->Uploads++;
        }

        void CopyTexture(const TextureRegion& source, const TextureRegion& destination) override
        {
            auto* sourceTexture = (GLTexture*)source.Texture;
            auto* destinationTexture = (GLTexture*)destination.Texture;
            int sourceZ = (sourceTexture->Desc.Type == TextureType::Texture3D) ? source.Z : source.Layer;
            int destinationZ = (destinationTexture->Desc.Type == TextureType::Texture3D) ? destination.Z : destination.Layer;

            g_GL.CopyImageSubData(sourceTexture->Id, sourceTexture->Target, source.MipLevel, source.X, source.Y, sourceZ,
                destinationTexture->Id, destinationTexture->Target, destination.MipLevel, destination.X, destination.Y, destinationZ,
                source.Width, source.Height, source.Depth);
        }

        // Synchronous readback; data is available as soon as this returns.
        void DownloadFromTexture(const TextureRegion& source, const TextureTransferInfo& destination) override
        {
            auto* texture = (GLTexture*)source.Texture;
            auto& data = ((GLTransferBuffer*)destination.TransferBuffer)->Data;
            int z = (texture->Desc.Type == TextureType::Texture3D) ? source.Z : source.Layer;
            int size = (int)data.size() - destination.Offset;

            g_GL.GetTextureSubImage(texture->Id, source.MipLevel, source.X, source.Y, z, source.Width, source.Height, 1,
                texture->Format.Format, texture->Format.Type, size, data.data() + destination.Offset);
        }

    private:
        // Block compressed rows are packed unless the source has a different pitch, which the block size describes.
        static void UploadCompressed(const GLTexture& texture, const TextureRegion& destination, const unsigned char* data, int pixelsPerRow, int rowsPerLayer, int z, int depth)
        {
            const auto& desc = texture.Desc;
            bool isPitched = (pixelsPerRow != destination.Width || rowsPerLayer != destination.Height);
            int blockDimension = GetFormatBlockDimension(desc.Format);
            int size = isPitched ? GetTextureDataSize(desc.Format, pixelsPerRow, rowsPerLayer, depth) : GetTextureDataSize(desc.Format, destination.Width, destination.Height, depth);

            if (isPitched)
            {
                g_GL.PixelStorei(GL_UNPACK_COMPRESSED_BLOCK_WIDTH, blockDimension);
                g_GL.PixelStorei(GL_UNPACK_COMPRESSED_BLOCK_HEIGHT, blockDimension);
                g_GL.PixelStorei(GL_UNPACK_COMPRESSED_BLOCK_DEPTH, 1);
                g_GL.PixelStorei(GL_UNPACK_COMPRESSED_BLOCK_SIZE, GetFormatBlockSize(desc.Format));
                g_GL.PixelStorei(GL_UNPACK_ROW_LENGTH, pixelsPerRow);
                g_GL.PixelStorei(GL_UNPACK_IMAGE_HEIGHT, rowsPerLayer);
            }

            if (texture.Target != GL_TEXTURE_2D)
            {
                g_GL.CompressedTextureSubImage3D(texture.Id, destination.MipLevel, destination.X, destination.Y, z,
                    destination.Width, destination.Height, depth, texture.Format.Internal, size, data);
            }
            else
            {
                g_GL.CompressedTextureSubImage2D(texture.Id, destination.MipLevel, destination.X, destination.Y,
                    destination.Width, destination.Height, texture.Format.Internal, size, data);
            }

            if (isPitched)
            {
                g_GL.PixelStorei(GL_UNPACK_COMPRESSED_BLOCK_WIDTH, 0);
                g_GL.PixelStorei(GL_UNPACK_COMPRESSED_BLOCK_HEIGHT, 0);
                g_GL.PixelStorei(GL_UNPACK_COMPRESSED_BLOCK_DEPTH, 0);
                g_GL.PixelStorei(GL_UNPACK_COMPRESSED_BLOCK_SIZE, 0);
                g_GL.PixelStorei(GL_UNPACK_ROW_LENGTH, 0);
                g_GL.PixelStorei(GL_UNPACK_IMAGE_HEIGHT, 0);
            }
        }
    };

    class GLComputePass : public IComputePass
    {
    public:
        GLComputePipeline* Pipeline = nullptr;
        CommandCounters*   Counters = nullptr;

        void BindPipeline(IComputePipeline* pipeline) override
        {
            Pipeline = (GLComputePipeline*)pipeline;
            g_GL.BindProgramPipeline(Pipeline->ProgramPipeline);
        }

        void BindSamplers(int firstSlot, const TextureSamplerBinding* bindings, int count) override
        {
            GLRenderPass::BindSamplers(firstSlot, bindings, count);
        }

        void BindStorageBuffers(int firstSlot, IBuffer* const* buffers, int count) override
        {
            int base = (Pipeline != nullptr) ? Pipeline->StorageBufferBase : 0;
            for (int i = 0; i < count; i++)
                g_GL.BindBufferBase(GL_SHADER_STORAGE_BUFFER, base + firstSlot + i, GetId(buffers[i]));
        }

        void Dispatch(int groupCountX, int groupCountY, int groupCountZ) override
        {
            g_GL.DispatchCompute(groupCountX, groupCountY, groupCountZ);
            Counters->Dispatches++;
        }
    };

    class GLDevice;

    class GLCommandBuffer : public ICommandBuffer
    {
    public:
        GLDevice*     Owner     = nullptr;
        GLContext*    Context   = nullptr;
        GLStateCache  Cache     = {};
        GLUniformRing Uniforms  = {};
        GLRenderPass  RenderPass  = {};
        GLCopyPass    CopyPass    = {};
        GLComputePass ComputePass = {};
        GLGpuProfiler Profiler    = {};
        bool          HasSwapchainTexture = false;
        bool          IsRenderPassOpen    = false; // SDL GPU rules: no copy or compute pass inside a render pass.
        int           DebugGroupDepth     = 0;

        ITexture* AcquireSwapchainTexture() override;
        void      Submit() override;
        std::unique_ptr<IFence> SubmitWithFence() override;
        void      BlitTexture(ITexture* source, ITexture* destination) override;

        IRenderPass* BeginRenderPass(const ColorTargetInfo* colorTargets, int colorTargetCount, const DepthTargetInfo* depthTarget) override
        {
            bool isWindowTarget = (colorTargetCount > 0 && GetId(colorTargets[0].Texture) == 0);
            auto framebuffer = isWindowTarget ? 0 : GetFramebuffer(colorTargets, colorTargetCount, depthTarget);
            g_GL.BindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer);
            SetWindowTarget(isWindowTarget);

            int width = 0;
            int height = 0;
            if (colorTargetCount > 0)
            {
                const auto& desc = colorTargets[0].Texture->GetDesc();
                width = std::max(1, desc.Width >> colorTargets[0].MipLevel);
                height = std::max(1, desc.Height >> colorTargets[0].MipLevel);
            }
            else if (depthTarget != nullptr)
            {
                width = depthTarget->Texture->GetDesc().Width;
                height = depthTarget->Texture->GetDesc().Height;
            }

            RenderPass.Cache = &Cache;
            RenderPass.Counters = &Counters;
            RenderPass.IsWindowTarget = isWindowTarget;
            RenderPass.TargetHeight = height;
            RenderPass.Reset();

            // Match SDL GPU, which resets viewport and scissor to the full target at pass start.
            RenderPass.SetViewport(Viewport{ 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f });
            RenderPass.SetScissor(ScissorRect{ 0, 0, width, height });

            ClearTargets(framebuffer, colorTargets, colorTargetCount, depthTarget);

            IsRenderPassOpen = true;
            Counters.RenderPasses++;
            return &RenderPass;
        }

        void EndRenderPass() override
        {
            IsRenderPassOpen = false;
            RenderPass.Reset();
        }

        ICopyPass* BeginCopyPass() override
        {
            if (IsRenderPassOpen)
                TENLog("OpenGL: copy pass begun inside a render pass.", LogLevel::Error);

            CopyPass.Counters = &Counters;
            Counters.CopyPasses++;
            return &CopyPass;
        }

        void EndCopyPass() override {}

        // Read-write textures are image units 0 onwards. Read-write buffer N is storage binding COMPUTE_RW_STORAGE_BASE
        // plus the read-write texture count plus N, as OffsetBindings moves it.
        IComputePass* BeginComputePass(const ComputePassDesc& desc) override
        {
            if (IsRenderPassOpen)
                TENLog("OpenGL: compute pass begun inside a render pass.", LogLevel::Error);

            for (int i = 0; i < desc.NumRWTextures; i++)
            {
                auto* texture = (GLTexture*)desc.RWTextures[i];
                g_GL.BindImageTexture(i, texture->Id, 0, (texture->Target != GL_TEXTURE_2D) ? GL_TRUE : GL_FALSE, 0, GL_READ_WRITE, texture->Format.Internal);
            }

            for (int i = 0; i < desc.NumRWBuffers; i++)
                g_GL.BindBufferBase(GL_SHADER_STORAGE_BUFFER, COMPUTE_RW_STORAGE_BASE + desc.NumRWTextures + i, GetId(desc.RWBuffers[i]));

            ComputePass.Counters = &Counters;
            ComputePass.Pipeline = nullptr;
            Counters.ComputePasses++;
            return &ComputePass;
        }

        // Makes the writes of the pass visible to every later use, as SDL GPU does between passes.
        void EndComputePass() override
        {
            g_GL.MemoryBarrier(GL_ALL_BARRIER_BITS);
        }

        // Writes the data to the ring and binds that range to the uniform binding of the stage and slot.
        void PushUniforms(ShaderStage stage, int slot, const void* data, int size) override
        {
            Counters.UniformPushes++;
            Counters.UniformBytes += size;
            if (Uniforms.GetReservedSize(size) > UNIFORM_RING_SEGMENT_SIZE)
            {
                TENLog("OpenGL: uniform push of " + std::to_string(size) + " bytes exceeds the uniform ring segment.", LogLevel::Error);
                return;
            }

            int base = (stage == ShaderStage::Pixel) ? PIXEL_UNIFORM_BASE : ((stage == ShaderStage::Compute) ? COMPUTE_UNIFORM_BASE : 0);
            int offset = Uniforms.Write(data, size);
            g_GL.BindBufferRange(GL_UNIFORM_BUFFER, base + slot, Uniforms.GetBuffer(), offset, Uniforms.GetReservedSize(size));
        }

        void PushDebugGroup(const char* name) override
        {
            g_GL.PushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
            Profiler.OnPushGroup(++DebugGroupDepth, name);
        }

        void PopDebugGroup() override
        {
            g_GL.PopDebugGroup();
            Profiler.OnPopGroup(DebugGroupDepth--);
        }

        void InsertDebugLabel(const char* name) override
        {
            g_GL.DebugMessageInsert(GL_DEBUG_SOURCE_APPLICATION, GL_DEBUG_TYPE_MARKER, 0, GL_DEBUG_SEVERITY_NOTIFICATION, -1, name);
        }

        // The window framebuffer has its origin at the lower left, so it is drawn with that clip origin, and
        // sRGB conversion is off for it as for the swapchains of the other backends.
        void SetWindowTarget(bool isWindowTarget)
        {
            auto clipOrigin = isWindowTarget ? GL_LOWER_LEFT : GL_UPPER_LEFT;
            if (Cache.ClipOrigin != clipOrigin)
            {
                g_GL.ClipControl(clipOrigin, GL_ZERO_TO_ONE);
                Cache.ClipOrigin = clipOrigin;
                Cache.IsStateValid = false;
            }

            SetSrgbWrite(!isWindowTarget);
        }

        void SetSrgbWrite(bool isEnabled)
        {
            if (Cache.IsSrgbWrite == isEnabled)
                return;

            if (isEnabled)
                g_GL.Enable(GL_FRAMEBUFFER_SRGB);
            else
                g_GL.Disable(GL_FRAMEBUFFER_SRGB);

            Cache.IsSrgbWrite = isEnabled;
        }

        // Returns the cached framebuffer of this combination of attachments, creating it on first use.
        GLuint GetFramebuffer(const ColorTargetInfo* colorTargets, int colorTargetCount, const DepthTargetInfo* depthTarget)
        {
            auto key = FramebufferKey{};
            for (int i = 0; i < colorTargetCount; i++)
            {
                key.Values[i * 2] = GetId(colorTargets[i].Texture);
                key.Values[(i * 2) + 1] = (colorTargets[i].MipLevel << 16) | colorTargets[i].Layer;
            }

            if (depthTarget != nullptr)
            {
                key.Values[MAX_COLOR_TARGETS * 2] = GetId(depthTarget->Texture);
                key.Values[(MAX_COLOR_TARGETS * 2) + 1] = depthTarget->Layer;
            }

            key.Values[(MAX_COLOR_TARGETS + 1) * 2] = colorTargetCount;

            auto it = Context->Framebuffers.find(key);
            if (it != Context->Framebuffers.end())
                return it->second;

            auto framebuffer = CreateFramebuffer(colorTargets, colorTargetCount, depthTarget);
            Context->Framebuffers[key] = framebuffer;
            return framebuffer;
        }

    private:
        static void AttachTexture(GLuint framebuffer, GLenum attachment, const GLTexture& texture, int mipLevel, int layer)
        {
            if (texture.Target == GL_TEXTURE_2D || texture.Target == GL_TEXTURE_2D_MULTISAMPLE)
                g_GL.NamedFramebufferTexture(framebuffer, attachment, texture.Id, mipLevel);
            else
                g_GL.NamedFramebufferTextureLayer(framebuffer, attachment, texture.Id, mipLevel, layer);
        }

        static GLuint CreateFramebuffer(const ColorTargetInfo* colorTargets, int colorTargetCount, const DepthTargetInfo* depthTarget)
        {
            GLuint framebuffer = 0;
            g_GL.CreateFramebuffers(1, &framebuffer);

            GLenum drawBuffers[MAX_COLOR_TARGETS] = {};
            for (int i = 0; i < colorTargetCount; i++)
            {
                AttachTexture(framebuffer, GL_COLOR_ATTACHMENT0 + i, *(GLTexture*)colorTargets[i].Texture, colorTargets[i].MipLevel, colorTargets[i].Layer);
                drawBuffers[i] = GL_COLOR_ATTACHMENT0 + i;
            }

            if (depthTarget != nullptr)
            {
                auto* texture = (GLTexture*)depthTarget->Texture;
                auto attachment = HasStencil(texture->Desc.Format) ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
                AttachTexture(framebuffer, attachment, *texture, 0, depthTarget->Layer);
            }

            if (colorTargetCount > 0)
            {
                g_GL.NamedFramebufferDrawBuffers(framebuffer, colorTargetCount, drawBuffers);
                g_GL.NamedFramebufferReadBuffer(framebuffer, GL_COLOR_ATTACHMENT0);
            }
            else
            {
                g_GL.NamedFramebufferDrawBuffer(framebuffer, GL_NONE);
                g_GL.NamedFramebufferReadBuffer(framebuffer, GL_NONE);
            }

            auto status = g_GL.CheckNamedFramebufferStatus(framebuffer, GL_DRAW_FRAMEBUFFER);
            if (status != GL_FRAMEBUFFER_COMPLETE)
                TENLog("OpenGL: incomplete framebuffer (status " + std::to_string(status) + ").", LogLevel::Error);

            return framebuffer;
        }

        // Clears ignore the pipeline state except the write masks, which are reset here and in the state cache.
        void ClearTargets(GLuint framebuffer, const ColorTargetInfo* colorTargets, int colorTargetCount, const DepthTargetInfo* depthTarget)
        {
            for (int i = 0; i < colorTargetCount; i++)
            {
                const auto& target = colorTargets[i];
                if (target.Load != LoadOp::Clear)
                    continue;

                g_GL.ColorMaski(i, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
                Cache.State.Blend[i].WriteMask = ColorWriteMask::All;

                if (target.Texture->GetDesc().Format == PixelFormat::R32Uint)
                {
                    GLuint value[4] = { (GLuint)target.ClearColor[0], (GLuint)target.ClearColor[1], (GLuint)target.ClearColor[2], (GLuint)target.ClearColor[3] };
                    g_GL.ClearNamedFramebufferuiv(framebuffer, GL_COLOR, i, value);
                }
                else
                {
                    g_GL.ClearNamedFramebufferfv(framebuffer, GL_COLOR, i, target.ClearColor);
                }
            }

            if (depthTarget == nullptr || depthTarget->Load != LoadOp::Clear)
                return;

            g_GL.DepthMask(GL_TRUE);
            Cache.State.IsDepthWriteEnabled = true;

            if (HasStencil(depthTarget->Texture->GetDesc().Format))
                g_GL.ClearNamedFramebufferfi(framebuffer, GL_DEPTH_STENCIL, 0, depthTarget->ClearDepth, depthTarget->ClearStencil);
            else
                g_GL.ClearNamedFramebufferfv(framebuffer, GL_DEPTH, 0, &depthTarget->ClearDepth);
        }
    };

    // Owns the context, shared with the resources, and the single command buffer.
    class GLDevice : public IDevice
    {
    private:
        std::shared_ptr<GLContext> _context          = nullptr;
        GLCommandBuffer            _commandBuffer    = {};
        GLTexture                  _swapchainTexture = {};
        std::string                _rendererName     = {};

    public:
        GLDevice(std::shared_ptr<GLContext> context) :
            _context(context)
        {
            _rendererName = (const char*)g_GL.GetString(GL_RENDERER);
            _commandBuffer.Owner = this;
            _commandBuffer.Context = context.get();
            if (!_commandBuffer.Uniforms.Initialize())
                TENLog("OpenGL: cannot map the uniform ring.", LogLevel::Error);

            // Every pass sets its scissor; seamless filtering of cube maps and sRGB writes match the other backends.
            g_GL.Enable(GL_SCISSOR_TEST);
            g_GL.Enable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
            g_GL.Enable(GL_FRAMEBUFFER_SRGB);
            g_GL.ClipControl(GL_UPPER_LEFT, GL_ZERO_TO_ONE);
            g_GL.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
            g_GL.PixelStorei(GL_PACK_ALIGNMENT, 1);

            _swapchainTexture.Desc.Format = PixelFormat::BGRA8Unorm;
            _swapchainTexture.Desc.Usage = TextureUsage::ColorTarget;
            _swapchainTexture.Desc.Name = "Swapchain";
            UpdateSwapchainTexture();

            GLint storageBindings = 0;
            g_GL.GetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &storageBindings);
            if (storageBindings < STORAGE_BINDINGS_REQUIRED)
                TENLog("OpenGL: the driver has " + std::to_string(storageBindings) + " storage buffer bindings, fewer than the " + std::to_string(STORAGE_BINDINGS_REQUIRED) + " used.", LogLevel::Warning);
        }

        ~GLDevice() override
        {
            if (_context->MakeCurrent())
            {
                g_GL.Finish();
                for (auto& [key, framebuffer] : _context->Framebuffers)
                    g_GL.DeleteFramebuffers(1, &framebuffer);

                _context->Framebuffers.clear();
                _commandBuffer.Uniforms.Release();
                _commandBuffer.Profiler.Release();
                SDL_GL_MakeCurrent(_context->Window, nullptr);
            }

            SDL_GL_DestroyContext(_context->Handle);
            _context->IsAlive = false;
        }

        // The window framebuffer has no texture; the swapchain texture only carries its size.
        void UpdateSwapchainTexture()
        {
            SDL_GetWindowSizeInPixels(_context->Window, &_swapchainTexture.Desc.Width, &_swapchainTexture.Desc.Height);
        }

        const std::shared_ptr<GLContext>& GetContext() const
        {
            return _context;
        }

        ITexture* GetSwapchainTexture()
        {
            UpdateSwapchainTexture();
            return &_swapchainTexture;
        }

        void Present()
        {
            SDL_GL_SwapWindow(_context->Window);
        }

        GraphicsApi GetBackend() const override
        {
            return GraphicsApi::OpenGL;
        }

        ShaderFormat GetShaderFormat() const override
        {
            return ShaderFormat::Glsl;
        }

        std::string GetDeviceName() const override
        {
            return _rendererName + " (opengl)";
        }

        std::unique_ptr<IBuffer> CreateBuffer(const BufferDesc& desc) override
        {
            if (!_context->MakeCurrent())
                return nullptr;

            // Immutable storage updated with glNamedBufferSubData; usage flags need no equivalent.
            auto buffer = std::make_unique<GLBuffer>();
            buffer->Context = _context;
            g_GL.CreateBuffers(1, &buffer->Id);
            g_GL.NamedBufferStorage(buffer->Id, std::max(desc.Size, 1), nullptr, GL_DYNAMIC_STORAGE_BIT);
            SetDebugName(GL_BUFFER, buffer->Id, desc.Name);

            buffer->Size = desc.Size;
            buffer->Usage = desc.Usage;
            buffer->Allocation = MemoryAllocation(MemoryCategory::Buffer, desc.Size);
            return buffer;
        }

        std::unique_ptr<ITexture> CreateTexture(const TextureDesc& desc) override
        {
            auto format = ToGL(desc.Format);
            if (format.Internal == 0 || !_context->MakeCurrent())
            {
                TENLog("OpenGL: cannot create texture " + desc.Name, LogLevel::Error);
                return nullptr;
            }

            auto texture = std::make_unique<GLTexture>();
            texture->Context = _context;
            texture->Desc = desc;
            texture->Format = format;
            texture->Target = ToGLTarget(desc.Type, desc.SampleCount);
            g_GL.CreateTextures(texture->Target, 1, &texture->Id);

            switch (texture->Target)
            {
            case GL_TEXTURE_2D_MULTISAMPLE:
                g_GL.TextureStorage2DMultisample(texture->Id, desc.SampleCount, format.Internal, desc.Width, desc.Height, GL_TRUE);
                break;

            case GL_TEXTURE_2D:
            case GL_TEXTURE_CUBE_MAP:
                g_GL.TextureStorage2D(texture->Id, desc.MipLevels, format.Internal, desc.Width, desc.Height);
                break;

            default:
                g_GL.TextureStorage3D(texture->Id, desc.MipLevels, format.Internal, desc.Width, desc.Height, desc.DepthOrLayers);
                break;
            }

            SetDebugName(GL_TEXTURE, texture->Id, desc.Name);
            texture->Allocation = MemoryAllocation(GetTextureMemoryCategory(desc), GetTextureMemorySize(desc));
            return texture;
        }

        std::unique_ptr<ISampler> CreateSampler(const SamplerDesc& desc) override
        {
            if (!_context->MakeCurrent())
                return nullptr;

            auto sampler = std::make_unique<GLSampler>();
            sampler->Context = _context;
            g_GL.CreateSamplers(1, &sampler->Id);

            auto id = sampler->Id;
            bool isAnisotropic = (desc.MaxAnisotropy > 1);
            g_GL.SamplerParameteri(id, GL_TEXTURE_MIN_FILTER, ToGLMinFilter(desc));
            g_GL.SamplerParameteri(id, GL_TEXTURE_MAG_FILTER, (isAnisotropic || desc.MagFilter == Filter::Linear) ? GL_LINEAR : GL_NEAREST);
            g_GL.SamplerParameteri(id, GL_TEXTURE_WRAP_S, ToGL(desc.AddressU));
            g_GL.SamplerParameteri(id, GL_TEXTURE_WRAP_T, ToGL(desc.AddressV));
            g_GL.SamplerParameteri(id, GL_TEXTURE_WRAP_R, ToGL(desc.AddressW));
            g_GL.SamplerParameterf(id, GL_TEXTURE_LOD_BIAS, desc.MipLodBias);
            g_GL.SamplerParameterf(id, GL_TEXTURE_MIN_LOD, desc.MinLod);
            g_GL.SamplerParameterf(id, GL_TEXTURE_MAX_LOD, desc.MaxLod);
            g_GL.SamplerParameterf(id, GL_TEXTURE_MAX_ANISOTROPY_EXT, (float)std::max(1, desc.MaxAnisotropy));
            g_GL.SamplerParameteri(id, GL_TEXTURE_COMPARE_MODE, desc.EnableCompare ? GL_COMPARE_REF_TO_TEXTURE : GL_NONE);
            g_GL.SamplerParameteri(id, GL_TEXTURE_COMPARE_FUNC, ToGL(desc.Compare));
            SetDebugName(GL_SAMPLER, id, desc.Name);
            return sampler;
        }

        // Compiles and links the GLSL source as a separable program, then moves its bindings to the range of its stage.
        std::unique_ptr<IShader> CreateShader(const ShaderDesc& desc) override
        {
            if (!_context->MakeCurrent())
                return nullptr;

            auto source = std::string((const char*)desc.Code, desc.CodeSize);
            const auto* sourceText = source.c_str();
            auto type = (desc.Stage == ShaderStage::Pixel) ? GL_FRAGMENT_SHADER : ((desc.Stage == ShaderStage::Compute) ? GL_COMPUTE_SHADER : GL_VERTEX_SHADER);

            auto shader = std::make_unique<GLShader>();
            shader->Context = _context;
            shader->Desc = desc;
            shader->Desc.Code = nullptr;
            shader->Desc.CodeSize = 0;
            shader->Program = g_GL.CreateShaderProgramv(type, 1, &sourceText);

            GLint isLinked = GL_FALSE;
            g_GL.GetProgramiv(shader->Program, GL_LINK_STATUS, &isLinked);
            if (!isLinked)
            {
                char log[4096] = {};
                g_GL.GetProgramInfoLog(shader->Program, (GLsizei)sizeof(log), nullptr, log);
                TENLog("OpenGL: cannot create shader " + desc.Name + ": " + log, LogLevel::Error);
                return nullptr;
            }

            OffsetBindings(shader->Program, desc.Stage);
            SetDebugName(GL_PROGRAM, shader->Program, desc.Name);
            return shader;
        }

        std::unique_ptr<IGraphicsPipeline> CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) override
        {
            if (!_context->MakeCurrent())
                return nullptr;

            auto* vertexShader = (GLShader*)desc.VertexShader;
            auto* pixelShader = (GLShader*)desc.PixelShader;

            auto pipeline = std::make_unique<GLGraphicsPipeline>();
            pipeline->Context = _context;
            pipeline->Mode = ToGL(desc.Primitive);
            pipeline->Primitive = desc.Primitive;
            pipeline->VertexStorageBufferBase = vertexShader->Desc.NumSamplers + vertexShader->Desc.NumStorageTextures;
            pipeline->PixelStorageBufferBase = (pixelShader != nullptr) ? (pixelShader->Desc.NumSamplers + pixelShader->Desc.NumStorageTextures) : 0;

            g_GL.CreateProgramPipelines(1, &pipeline->ProgramPipeline);
            g_GL.UseProgramStages(pipeline->ProgramPipeline, GL_VERTEX_SHADER_BIT, vertexShader->Program);
            if (pixelShader != nullptr)
                g_GL.UseProgramStages(pipeline->ProgramPipeline, GL_FRAGMENT_SHADER_BIT, pixelShader->Program);

            SetDebugName(GL_PROGRAM_PIPELINE, pipeline->ProgramPipeline, desc.Name);
            pipeline->VertexArray = CreateVertexArray(desc.Layout, pipeline->VertexStrides);
            pipeline->State = GetRenderState(desc);
            return pipeline;
        }

        std::unique_ptr<IComputePipeline> CreateComputePipeline(const ComputePipelineDesc& desc) override
        {
            if (!_context->MakeCurrent())
                return nullptr;

            auto* shader = (GLShader*)desc.Shader;

            auto pipeline = std::make_unique<GLComputePipeline>();
            pipeline->Context = _context;
            pipeline->StorageBufferBase = shader->Desc.NumSamplers + shader->Desc.NumStorageTextures;
            g_GL.CreateProgramPipelines(1, &pipeline->ProgramPipeline);
            g_GL.UseProgramStages(pipeline->ProgramPipeline, GL_COMPUTE_SHADER_BIT, shader->Program);
            SetDebugName(GL_PROGRAM_PIPELINE, pipeline->ProgramPipeline, desc.Name);
            return pipeline;
        }

        std::unique_ptr<ITransferBuffer> CreateTransferBuffer(TransferBufferUsage usage, int size) override
        {
            auto buffer = std::make_unique<GLTransferBuffer>();
            buffer->Data.resize(size);
            buffer->Allocation = MemoryAllocation(MemoryCategory::TransferBuffer, size);
            return buffer;
        }

        // Transfer buffers are system memory, so cycle is not needed.
        void* MapTransferBuffer(ITransferBuffer* buffer, bool cycle) override
        {
            return ((GLTransferBuffer*)buffer)->Data.data();
        }

        void UnmapTransferBuffer(ITransferBuffer* buffer) override {}

        void GenerateMipmaps(ICommandBuffer* commandBuffer, ITexture* texture) override
        {
            g_GL.GenerateTextureMipmap(GetId(texture));
        }

        ICommandBuffer* AcquireCommandBuffer() override
        {
            if (!_context->MakeCurrent())
                return nullptr;

            _commandBuffer.ResetCounters();
            return &_commandBuffer;
        }

        PixelFormat GetSwapchainFormat() const override
        {
            return PixelFormat::BGRA8Unorm;
        }

        void SetPresentMode(PresentMode mode) override
        {
            if (_context->MakeCurrent())
                SDL_GL_SetSwapInterval((mode == PresentMode::Vsync) ? 1 : 0);
        }

        bool SupportsPresentMode(PresentMode mode) const override
        {
            return (mode == PresentMode::Vsync || mode == PresentMode::Immediate);
        }

        void ResizeSwapchain(int width, int height) override
        {
            UpdateSwapchainTexture();
        }

        void WaitIdle() override
        {
            if (_context->MakeCurrent())
                g_GL.Finish();
        }

        bool IsFenceSignaled(IFence* fence) override
        {
            if (fence == nullptr || !_context->MakeCurrent())
                return true;

            auto result = g_GL.ClientWaitSync(((GLFence*)fence)->Sync, 0, 0);
            return (result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED);
        }

        void WaitForFence(IFence* fence) override
        {
            if (fence == nullptr || !_context->MakeCurrent())
                return;

            while (g_GL.ClientWaitSync(((GLFence*)fence)->Sync, GL_SYNC_FLUSH_COMMANDS_BIT, FENCE_WAIT_TIMEOUT) == GL_TIMEOUT_EXPIRED)
                TENLog("OpenGL: GPU fence wait timed out.", LogLevel::Warning);
        }

        bool SupportsTextureFormat(PixelFormat format, TextureType type, unsigned int usage) const override
        {
            auto glFormat = ToGL(format);
            if (glFormat.Internal == 0 || !_context->MakeCurrent())
                return false;

            auto target = ToGLTarget(type, 1);
            GLint isSupported = GL_FALSE;
            g_GL.GetInternalformativ(target, glFormat.Internal, GL_INTERNALFORMAT_SUPPORTED, 1, &isSupported);
            if (isSupported != GL_TRUE)
                return false;

            if (usage & (TextureUsage::ColorTarget | TextureUsage::DepthStencilTarget))
            {
                GLint renderable = GL_NONE;
                g_GL.GetInternalformativ(target, glFormat.Internal, GL_FRAMEBUFFER_RENDERABLE, 1, &renderable);
                if (renderable == GL_NONE)
                    return false;
            }

            return true;
        }

        bool RequiresWindowThreadPresent() const override
        {
            return false;
        }

        // Flushes the work of this thread and makes the context current on no thread, so another can take it.
        void ReleaseThread() override
        {
            if (SDL_GL_GetCurrentContext() != _context->Handle)
                return;

            g_GL.Flush();
            SDL_GL_MakeCurrent(_context->Window, nullptr);
        }

        std::vector<GpuTiming> ConsumeGpuTimings() override
        {
            return _commandBuffer.Profiler.Consume();
        }

    private:
        // Offsets the texture units and buffer bindings of a pixel or compute program by the base of its stage.
        // Read-write storage buffers of compute programs are told apart by the block name of SPIRV-Cross.
        static void OffsetBindings(GLuint program, ShaderStage stage)
        {
            int textureBase = (stage == ShaderStage::Pixel) ? PIXEL_TEXTURE_UNIT_BASE : 0;
            int uniformBase = (stage == ShaderStage::Pixel) ? PIXEL_UNIFORM_BASE : ((stage == ShaderStage::Compute) ? COMPUTE_UNIFORM_BASE : 0);
            int storageBase = (stage == ShaderStage::Pixel) ? PIXEL_STORAGE_BASE : 0;
            int rwStorageBase = (stage == ShaderStage::Compute) ? COMPUTE_RW_STORAGE_BASE : 0;

            if (textureBase != 0)
                OffsetTextureUnits(program, textureBase);

            GLint count = 0;
            g_GL.GetProgramInterfaceiv(program, GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES, &count);
            for (int i = 0; i < count && uniformBase != 0; i++)
                g_GL.UniformBlockBinding(program, i, GetResourceBinding(program, GL_UNIFORM_BLOCK, i) + uniformBase);

            count = 0;
            g_GL.GetProgramInterfaceiv(program, GL_SHADER_STORAGE_BLOCK, GL_ACTIVE_RESOURCES, &count);
            for (int i = 0; i < count; i++)
            {
                char name[256] = {};
                g_GL.GetProgramResourceName(program, GL_SHADER_STORAGE_BLOCK, i, (GLsizei)sizeof(name), nullptr, name);
                int base = (strstr(name, "RWStructuredBuffer") != nullptr) ? rwStorageBase : storageBase;
                if (base != 0)
                    g_GL.ShaderStorageBlockBinding(program, i, GetResourceBinding(program, GL_SHADER_STORAGE_BLOCK, i) + base);
            }
        }

        // Binding point of a uniform or storage block, as set by the layout qualifier of SPIRV-Cross.
        static int GetResourceBinding(GLuint program, GLenum interfaceType, int index)
        {
            GLenum property = GL_BUFFER_BINDING;
            GLint binding = 0;
            g_GL.GetProgramResourceiv(program, interfaceType, index, 1, &property, 1, nullptr, &binding);
            return binding;
        }

        // Adds the base to the unit of every sampler uniform outside of uniform blocks.
        static void OffsetTextureUnits(GLuint program, int base)
        {
            GLint count = 0;
            g_GL.GetProgramInterfaceiv(program, GL_UNIFORM, GL_ACTIVE_RESOURCES, &count);
            for (int i = 0; i < count; i++)
            {
                GLenum properties[] = { GL_TYPE, GL_LOCATION, GL_BLOCK_INDEX };
                GLint values[3] = {};
                g_GL.GetProgramResourceiv(program, GL_UNIFORM, i, 3, properties, 3, nullptr, values);
                if (values[2] != -1 || values[1] < 0 || !IsSamplerType(values[0]))
                    continue;

                GLint unit = 0;
                g_GL.GetUniformiv(program, values[1], &unit);
                g_GL.ProgramUniform1i(program, values[1], unit + base);
            }
        }

        // Records the attribute formats and buffer divisors; the buffers themselves are bound per pass. Returns the
        // strides of each slot, given when vertex buffers are bound.
        static GLuint CreateVertexArray(const VertexLayout& layout, int* strides)
        {
            GLuint vertexArray = 0;
            g_GL.CreateVertexArrays(1, &vertexArray);

            for (int i = 0; i < layout.NumBuffers; i++)
            {
                const auto& buffer = layout.Buffers[i];
                strides[buffer.Slot] = buffer.Stride;
                g_GL.VertexArrayBindingDivisor(vertexArray, buffer.Slot, (buffer.Rate == VertexInputRate::Instance) ? 1 : 0);
            }

            for (int i = 0; i < layout.NumAttributes; i++)
            {
                const auto& attribute = layout.Attributes[i];
                auto format = ToGL(attribute.Format);

                g_GL.EnableVertexArrayAttrib(vertexArray, attribute.Location);
                if (format.IsInteger)
                    g_GL.VertexArrayAttribIFormat(vertexArray, attribute.Location, format.Size, format.Type, attribute.Offset);
                else
                    g_GL.VertexArrayAttribFormat(vertexArray, attribute.Location, format.Size, format.Type, format.IsNormalized ? GL_TRUE : GL_FALSE, attribute.Offset);

                g_GL.VertexArrayAttribBinding(vertexArray, attribute.Location, attribute.BufferSlot);
            }

            return vertexArray;
        }

        // Translates the blend, rasterizer, depth and stencil state of the description.
        static GLRenderState GetRenderState(const GraphicsPipelineDesc& desc)
        {
            auto state = GLRenderState{};
            for (int i = 0; i < desc.NumColorTargets; i++)
            {
                const auto& blend = desc.ColorTargets[i].Blend;
                auto& target = state.Blend[i];
                target.IsEnabled = blend.EnableBlend;
                target.Src = ToGL(blend.SrcColor);
                target.Dst = ToGL(blend.DstColor);
                target.SrcAlpha = ToGL(blend.SrcAlpha);
                target.DstAlpha = ToGL(blend.DstAlpha);
                target.Op = ToGL(blend.ColorOp);
                target.OpAlpha = ToGL(blend.AlphaOp);
                target.WriteMask = blend.WriteMask;
            }

            // With the upper left clip origin of texture targets, the winding of OpenGL matches the one of Direct3D.
            const auto& rasterizer = desc.Rasterizer;
            state.IsCullEnabled = (rasterizer.Cull != CullMode::None);
            state.CullFace = (rasterizer.Cull == CullMode::Front) ? GL_FRONT : GL_BACK;
            state.FrontFace = (rasterizer.Front == FrontFace::Clockwise) ? GL_CW : GL_CCW;
            state.PolygonMode = (rasterizer.Fill == FillMode::Wireframe) ? GL_LINE : GL_FILL;
            state.IsDepthBiasEnabled = rasterizer.EnableDepthBias;
            state.DepthBiasConstant = rasterizer.EnableDepthBias ? rasterizer.DepthBiasConstant : 0.0f;
            state.DepthBiasSlope = rasterizer.EnableDepthBias ? rasterizer.DepthBiasSlope : 0.0f;
            state.IsDepthClampEnabled = !rasterizer.EnableDepthClip;

            state.IsDepthTestEnabled = desc.DepthStencil.EnableDepthTest && desc.DepthFormat != PixelFormat::Unknown;
            state.IsDepthWriteEnabled = desc.DepthStencil.EnableDepthWrite && desc.DepthFormat != PixelFormat::Unknown;
            state.DepthFunc = ToGL(desc.DepthStencil.DepthCompare);
            state.IsStencilTestEnabled = desc.DepthStencil.EnableStencilTest && HasStencil(desc.DepthFormat);
            state.StencilFunc = ToGL(desc.DepthStencil.StencilCompare);
            state.StencilPassOp = ToGL(desc.DepthStencil.StencilPassOp);
            state.StencilReference = desc.DepthStencil.StencilReference;
            return state;
        }
    };

    ITexture* GLCommandBuffer::AcquireSwapchainTexture()
    {
        HasSwapchainTexture = true;
        return Owner->GetSwapchainTexture();
    }

    // Commands already went to the context: submission swaps the window buffers, or flushes when nothing is presented.
    void GLCommandBuffer::Submit()
    {
        if (HasSwapchainTexture)
            Owner->Present();
        else
            g_GL.Flush();

        HasSwapchainTexture = false;
    }

    std::unique_ptr<IFence> GLCommandBuffer::SubmitWithFence()
    {
        Submit();

        auto fence = std::make_unique<GLFence>();
        fence->Context = Owner->GetContext();
        fence->Sync = g_GL.FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        g_GL.Flush();
        return fence;
    }

    // The window framebuffer is upside down, so the copy to it is flipped. The scissor test applies to blits, so it is off
    // during the copy.
    void GLCommandBuffer::BlitTexture(ITexture* source, ITexture* destination)
    {
        const auto& sourceDesc = source->GetDesc();
        const auto& destinationDesc = destination->GetDesc();
        bool isWindowTarget = (GetId(destination) == 0);

        auto sourceTarget = ColorTargetInfo{};
        sourceTarget.Texture = source;
        auto readFramebuffer = GetFramebuffer(&sourceTarget, 1, nullptr);

        auto destinationTarget = ColorTargetInfo{};
        destinationTarget.Texture = destination;
        auto drawFramebuffer = isWindowTarget ? 0 : GetFramebuffer(&destinationTarget, 1, nullptr);

        bool isScaled = (sourceDesc.Width != destinationDesc.Width || sourceDesc.Height != destinationDesc.Height);
        int destinationTop = isWindowTarget ? destinationDesc.Height : 0;
        int destinationBottom = isWindowTarget ? 0 : destinationDesc.Height;

        g_GL.Disable(GL_SCISSOR_TEST);
        SetSrgbWrite(!isWindowTarget);
        g_GL.BlitNamedFramebuffer(readFramebuffer, drawFramebuffer, 0, 0, sourceDesc.Width, sourceDesc.Height,
            0, destinationTop, destinationDesc.Width, destinationBottom, GL_COLOR_BUFFER_BIT, isScaled ? GL_LINEAR : GL_NEAREST);
        g_GL.Enable(GL_SCISSOR_TEST);
        Counters.Blits++;
    }

    // Attributes of a 4.5 core context. The window framebuffer needs no depth or stencil buffer: depth targets are textures.
    unsigned int PrepareGLWindow()
    {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, REQUIRED_MAJOR_VERSION);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, REQUIRED_MINOR_VERSION);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
        SDL_GL_SetAttribute(SDL_GL_FRAMEBUFFER_SRGB_CAPABLE, 0);
        return SDL_WINDOW_OPENGL;
    }

    // Creates the context, current on the calling thread, and loads the entry points it needs.
    std::unique_ptr<IDevice> CreateGLDevice(SDL_Window* window, bool debugMode)
    {
        if ((SDL_GetWindowFlags(window) & SDL_WINDOW_OPENGL) == 0)
        {
            TENLog("OpenGL: the window was not created for OpenGL.", LogLevel::Warning);
            return nullptr;
        }

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, debugMode ? SDL_GL_CONTEXT_DEBUG_FLAG : 0);
        auto handle = SDL_GL_CreateContext(window);
        if (handle == nullptr)
        {
            TENLog(std::string("OpenGL: cannot create a 4.5 core context: ") + SDL_GetError(), LogLevel::Warning);
            return nullptr;
        }

        if (!LoadFunctions())
        {
            SDL_GL_DestroyContext(handle);
            return nullptr;
        }

        if (debugMode)
        {
            g_GL.Enable(GL_DEBUG_OUTPUT);
            g_GL.Enable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
            g_GL.DebugMessageCallback(OnDebugMessage, nullptr);
        }

        auto context = std::make_shared<GLContext>();
        context->Window = window;
        context->Handle = handle;
        context->IsAlive = true;

        auto device = std::make_unique<GLDevice>(context);
        TENLog("OpenGL RHI device created: " + device->GetDeviceName(), LogLevel::Info);
        return device;
    }
}
