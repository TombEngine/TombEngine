#pragma once

#include <mutex>

#include "Renderer/Graphics/Vertices/Vertex.h"
#include "Renderer/RHI/RHIDevice.h"

struct TEXTURE;

namespace TEN::Renderer
{
    struct RenderView;
}

namespace TEN::Renderer::Structures
{
    struct RendererRoom;
    struct RendererAnimatedTextureSet;
    struct RendererBucket;
    struct RendererMesh;
    struct RendererObject;
}

namespace TEN::Renderer::Next
{
    // Level geometry shared with the legacy renderer during the migration. The new renderer reads the
    // already prepared vertex and index arrays, so both renderers draw exactly the same data.
    struct LegacyLevelGeometry
    {
        const std::vector<Graphics::Vertices::Vertex>*  RoomVertices = nullptr;
        const std::vector<int>*                         RoomIndices  = nullptr;
        const std::vector<Structures::RendererRoom>*    Rooms        = nullptr;
        const std::vector<Structures::RendererAnimatedTextureSet>* AnimatedTextureSets = nullptr;

        const std::vector<Graphics::Vertices::Vertex>*                 StaticVertices   = nullptr;
        const std::vector<int>*                                        StaticIndices    = nullptr;
        const std::vector<Graphics::Vertices::Vertex>*                 MoveableVertices = nullptr;
        const std::vector<int>*                                        MoveableIndices  = nullptr;
        const std::vector<std::optional<Structures::RendererObject>>*  StaticObjects    = nullptr;
        const std::vector<std::optional<Structures::RendererObject>>*  MoveableObjects  = nullptr;
        const std::vector<Structures::RendererMesh*>*                  Meshes           = nullptr;
    };

    // Per-frame values owned by the game and the legacy renderer.
    struct FrameEnvironment
    {
        Vector3      CameraPosition    = Vector3::Zero;
        Vector4      FogColor          = Vector4::Zero;
        float        FogMinDistance    = 0.0f;
        float        FogMaxDistance    = 0.0f;
        unsigned int Frame             = 0;
        float        InterpolatedFrame = 0.0f;
    };

    // New RHI-based renderer. During the migration it draws into a separate preview window, fed by the
    // legacy renderer's culling results, so the two outputs can be compared side by side.
    class NextRenderer
    {
    private:
        struct RoomTexture
        {
            std::unique_ptr<RHI::ITexture> ColorMap = nullptr;
        };

        enum class MeshSource
        {
            Statics,
            Moveables
        };

        // Instanced draw of one mesh bucket; instances are contiguous in the frame instance buffer.
        struct ObjectDraw
        {
            MeshSource                        Source        = MeshSource::Statics;
            const Structures::RendererBucket* Bucket        = nullptr;
            int                               FirstInstance = 0;
            int                               InstanceCount = 0;
        };

        struct GeometryBuffers
        {
            std::unique_ptr<RHI::IBuffer> Vertices = nullptr;
            std::unique_ptr<RHI::IBuffer> Indices  = nullptr;
        };

        std::unique_ptr<RHI::IDevice> _device = nullptr;
        SDL_Window*                   _window = nullptr;
        std::string                   _shaderDirectory = {};

        std::unique_ptr<RHI::IShader>           _roomsVertexShader   = nullptr;
        std::unique_ptr<RHI::IShader>           _roomsPixelShader    = nullptr;
        std::unique_ptr<RHI::IGraphicsPipeline> _roomsPipeline       = nullptr;
        std::unique_ptr<RHI::IGraphicsPipeline> _roomsAdditivePipeline = nullptr;
        std::unique_ptr<RHI::IShader>           _objectsVertexShader = nullptr;
        std::unique_ptr<RHI::IShader>           _objectsPixelShader  = nullptr;
        std::unique_ptr<RHI::IGraphicsPipeline> _objectsPipeline     = nullptr;
        std::unique_ptr<RHI::IGraphicsPipeline> _objectsAdditivePipeline = nullptr;
        std::unique_ptr<RHI::ISampler>          _anisotropicSampler  = nullptr;
        std::unique_ptr<RHI::ITexture>          _whiteTexture        = nullptr;

        std::unique_ptr<RHI::IBuffer> _roomVertexBuffer = nullptr;
        std::unique_ptr<RHI::IBuffer> _roomIndexBuffer  = nullptr;
        std::vector<RoomTexture>      _roomTextures     = {};
        std::vector<RoomTexture>      _animatedTextures = {};
        std::unique_ptr<RHI::IBuffer> _animatedFramesBuffer = nullptr;
        std::unique_ptr<RHI::IBuffer> _animatedSetsBuffer   = nullptr;

        GeometryBuffers               _staticGeometry   = {};
        GeometryBuffers               _moveableGeometry = {};
        std::vector<RoomTexture>      _staticTextures   = {};
        std::vector<RoomTexture>      _moveableTextures = {};
        LegacyLevelGeometry           _legacyGeometry   = {};

        // Frame instance data: collected on the CPU, uploaded once per frame before the scene pass.
        std::unique_ptr<RHI::IBuffer>         _instanceIndexBuffer    = nullptr;
        std::unique_ptr<RHI::IBuffer>         _instanceBuffer         = nullptr;
        std::unique_ptr<RHI::ITransferBuffer> _instanceTransferBuffer = nullptr;
        int                                   _instanceCapacity       = 0;
        std::vector<unsigned char>            _instanceData           = {};
        std::vector<ObjectDraw>               _objectDraws            = {};
        bool                          _isLevelLoaded    = false;

        // Offscreen frame handed from the render thread to the window thread.
        std::unique_ptr<RHI::ITexture> _colorTarget  = nullptr;
        std::unique_ptr<RHI::ITexture> _depthTarget  = nullptr;
        std::mutex                     _frameMutex   = {};
        std::atomic<bool>              _isFrameReady = false;
        std::atomic<int>               _windowWidth  = 0;
        std::atomic<int>               _windowHeight = 0;
        std::string                    _screenshotPath = {};

        bool CreatePipelines();
        bool EnsureRenderTargets(int width, int height);
        std::unique_ptr<RHI::ITexture> CreateLevelTexture(const std::vector<unsigned char>& encodedData, const std::string& name);
        void UploadBuffer(RHI::IBuffer* buffer, const void* data, int size);
        void LoadAnimatedTextures(const LegacyLevelGeometry& geometry);
        std::unique_ptr<RHI::IGraphicsPipeline> CreateRoomsPipeline(bool additive);
        std::unique_ptr<RHI::IGraphicsPipeline> CreateObjectsPipeline(bool additive);
        void LoadTextureSet(const std::vector<::TEXTURE>& source, std::vector<RoomTexture>& destination, const std::string& name);
        void CreateGeometryBuffers(GeometryBuffers& buffers, const std::vector<Graphics::Vertices::Vertex>& vertices, const std::vector<int>& indices, const std::string& name);
        void CollectObjects(const RenderView& view);
        void UploadInstances(RHI::ICommandBuffer& commandBuffer);
        void DrawObjects(RHI::ICommandBuffer& commandBuffer, RHI::IRenderPass& renderPass, bool additive);
        void DrawRooms(RHI::ICommandBuffer& commandBuffer, RHI::IRenderPass& renderPass, const RenderView& view, bool additive);

    public:
        bool Initialize(SDL_Window* window, RHI::Backend backend, const std::string& gameDirectory);
        void Shutdown();
        bool IsActive() const;

        void LoadLevel(const LegacyLevelGeometry& geometry);
        void RenderFrame(const RenderView& view, const FrameEnvironment& environment);
        void PresentOnWindowThread();
        void RequestScreenshot(const std::string& path);
    };

    extern NextRenderer g_NextRenderer;
}
