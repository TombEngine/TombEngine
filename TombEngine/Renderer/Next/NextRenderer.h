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
    struct RendererLight;
    struct RendererItem;
    struct RendererSortableObject;
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
        const std::vector<Structures::RendererItem>*                   Items            = nullptr;
    };

    // Per-frame values owned by the game and the legacy renderer.
    struct FrameEnvironment
    {
        Vector3      CameraPosition    = Vector3::Zero;
        Vector4      FogColor          = Vector4::Zero;
        float        FogMinDistance    = 0.0f;
        float        FogMaxDistance    = 0.0f;
        unsigned int Frame               = 0;
        float        InterpolatedFrame   = 0.0f;
        float        InterpolationFactor = 0.0f;
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

        // GPU buffer rewritten every frame from CPU data through a cycled transfer buffer.
        struct FrameStorageBuffer
        {
            std::unique_ptr<RHI::IBuffer>         Buffer   = nullptr;
            std::unique_ptr<RHI::ITransferBuffer> Transfer = nullptr;
            int                                   Capacity = 0;
        };

        enum class SceneShader
        {
            Rooms,
            Objects,

            Count
        };

        // Run of consecutive sorted polygons sharing one draw group, drawn with one call.
        struct SortedDraw
        {
            MeshSource                        Source     = MeshSource::Statics;
            bool                              IsRoom     = false;
            const Structures::RendererBucket* Bucket     = nullptr;
            BlendMode                         BlendMode  = BlendMode::AlphaBlend;
            int                               FirstIndex = 0;
            int                               IndexCount = 0;
            int                               Instance   = 0;
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
        std::unique_ptr<RHI::IGraphicsPipeline> _scenePipelines[(int)SceneShader::Count][16] = {};
        std::unique_ptr<RHI::IShader>           _objectsVertexShader = nullptr;
        std::unique_ptr<RHI::IShader>           _objectsPixelShader  = nullptr;

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
        std::unique_ptr<RHI::IBuffer> _instanceIndexBuffer = nullptr;
        FrameStorageBuffer            _instanceBuffer      = {};
        FrameStorageBuffer            _lightBuffer         = {};
        FrameStorageBuffer            _boneBuffer          = {};
        std::vector<unsigned char>    _boneData            = {};
        std::vector<unsigned char>    _instanceData        = {};
        std::vector<unsigned char>    _lightData           = {};
        std::vector<ObjectDraw>       _objectDraws         = {};
        FrameStorageBuffer            _sortedIndexBuffer   = {};
        std::vector<unsigned char>    _sortedIndexData     = {};
        std::vector<SortedDraw>       _sortedDraws         = {};
        std::unordered_map<const void*, int> _sortedInstances = {};
        int                           _roomLightCount      = 0;
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
        RHI::IGraphicsPipeline* GetScenePipeline(SceneShader shader, BlendMode blendMode);
        void LoadTextureSet(const std::vector<::TEXTURE>& source, std::vector<RoomTexture>& destination, const std::string& name);
        void CreateGeometryBuffers(GeometryBuffers& buffers, const std::vector<Graphics::Vertices::Vertex>& vertices, const std::vector<int>& indices, const std::string& name);
        void CollectObjects(const RenderView& view, float interpolationFactor);
        void CollectPlayer(float interpolationFactor);
        void CollectPlayerHair(const Structures::RendererItem& item, const Vector4& ambientLight, int lightOffset, int lightCount, float interpolationFactor);
        int  AddInstance(const Matrix& world, const Vector4& color, const Vector4& ambientLight, LightMode lightMode, int lightOffset, int lightCount, int boneOffset, int skinningMode);
        int  AddBones(const Matrix* bones, int count);
        void AddMeshDraws(const Structures::RendererMesh& mesh, int instance);
        int  AddLights(const std::vector<Structures::RendererLight*>& lights, float interpolationFactor, int roomNumber, int prevRoomNumber, float fade, int& lightOffset);
        void UploadFrameStorage(RHI::ICommandBuffer& commandBuffer, FrameStorageBuffer& storage, const std::vector<unsigned char>& data, int stride, unsigned int usage, const char* name);
        void CollectSortedFaces(const RenderView& view, float interpolationFactor);
        int  GetSortedObjectInstance(const Structures::RendererSortableObject& object, float interpolationFactor);
        void DrawSortedFaces(RHI::ICommandBuffer& commandBuffer, RHI::IRenderPass& renderPass);
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
