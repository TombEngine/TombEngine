#pragma once

#include <mutex>

#include "Renderer/Graphics/Vertices/Vertex.h"
#include "Renderer/RHI/RHIDevice.h"

namespace TEN::Renderer
{
    struct RenderView;
}

namespace TEN::Renderer::Structures
{
    struct RendererRoom;
    struct RendererAnimatedTextureSet;
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

        std::unique_ptr<RHI::IDevice> _device = nullptr;
        SDL_Window*                   _window = nullptr;
        std::string                   _shaderDirectory = {};

        std::unique_ptr<RHI::IShader>           _roomsVertexShader   = nullptr;
        std::unique_ptr<RHI::IShader>           _roomsPixelShader    = nullptr;
        std::unique_ptr<RHI::IGraphicsPipeline> _roomsPipeline       = nullptr;
        std::unique_ptr<RHI::IGraphicsPipeline> _roomsAdditivePipeline = nullptr;
        std::unique_ptr<RHI::ISampler>          _anisotropicSampler  = nullptr;
        std::unique_ptr<RHI::ITexture>          _whiteTexture        = nullptr;

        std::unique_ptr<RHI::IBuffer> _roomVertexBuffer = nullptr;
        std::unique_ptr<RHI::IBuffer> _roomIndexBuffer  = nullptr;
        std::vector<RoomTexture>      _roomTextures     = {};
        std::vector<RoomTexture>      _animatedTextures = {};
        std::unique_ptr<RHI::IBuffer> _animatedFramesBuffer = nullptr;
        std::unique_ptr<RHI::IBuffer> _animatedSetsBuffer   = nullptr;
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
