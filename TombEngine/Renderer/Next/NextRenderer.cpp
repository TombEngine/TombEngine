#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Renderer/Next/TextureLoader.h"
#include "Renderer/RenderView.h"
#include "Renderer/RHI/ShaderLibrary.h"
#include "Renderer/RHI/TextureCapture.h"
#include "Renderer/Structures/RendererAnimatedTextureSet.h"
#include "Renderer/Structures/RendererRoom.h"
#include "Specific/level.h"

using namespace TEN::Renderer::RHI;
using namespace TEN::Renderer::Structures;
using TEN::Renderer::Graphics::Vertices::Vertex;

namespace TEN::Renderer::Next
{
    constexpr auto ALPHA_TEST_THRESHOLD = 0.5f;

    NextRenderer g_NextRenderer;

    // Uniform layouts mirror the cbuffers in Shaders/RHI/Rooms.hlsl.
    struct FrameVertexUniforms
    {
        Matrix       ViewProjection    = Matrix::Identity;
        Vector3      CameraPosition    = Vector3::Zero;
        float        FogMinDistance    = 0.0f;
        float        FogMaxDistance    = 0.0f;
        unsigned int Frame             = 0;
        float        InterpolatedFrame = 0.0f;
        float        Padding           = 0.0f;
    };

    struct DrawVertexUniforms
    {
        int   AnimatedSetIndex = NO_VALUE;
        float Padding[3]       = {};
    };

    struct FramePixelUniforms
    {
        Vector4 FogColor = Vector4::Zero;
    };

    struct DrawPixelUniforms
    {
        float        AlphaThreshold = 0.0f;
        unsigned int BlendMode      = 0;
        float        Padding[2]     = {};
    };

    struct GpuAnimatedFrame
    {
        Vector2 Corners[4];
    };

    struct GpuAnimatedSet
    {
        unsigned int FirstFrame        = 0;
        unsigned int NumFrames         = 0;
        unsigned int Fps               = 0;
        unsigned int Type              = 0;
        float        UVRotateDirection = 0.0f;
        float        UVRotateSpeed     = 0.0f;
        float        Padding[2]        = {};
    };

    static_assert(sizeof(FrameVertexUniforms) == 96, "FrameVertexUniforms must match Rooms.hlsl.");
    static_assert(sizeof(GpuAnimatedFrame) == 32, "GpuAnimatedFrame must match Common.hlsli.");
    static_assert(sizeof(GpuAnimatedSet) == 32, "GpuAnimatedSet must match Common.hlsli.");

    bool NextRenderer::Initialize(SDL_Window* window, Backend backend, const std::string& gameDirectory)
    {
        _device = CreateDevice(backend, window, true);
        if (_device == nullptr)
            return false;

        _window = window;
        _shaderDirectory = gameDirectory + "Shaders/RHI/Bin";

        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        _windowWidth = width;
        _windowHeight = height;

        if (!CreatePipelines())
        {
            TENLog("Next renderer: pipeline creation failed.", LogLevel::Error);
            _device = nullptr;
            return false;
        }

        TENLog("Next renderer initialized on " + _device->GetDeviceName(), LogLevel::Info);
        return true;
    }

    void NextRenderer::Shutdown()
    {
        if (_device == nullptr)
            return;

        _device->WaitIdle();
        _roomTextures.clear();
        _roomVertexBuffer = nullptr;
        _roomIndexBuffer = nullptr;
        _colorTarget = nullptr;
        _depthTarget = nullptr;
        _roomsPipeline = nullptr;
        _roomsVertexShader = nullptr;
        _roomsPixelShader = nullptr;
        _anisotropicSampler = nullptr;
        _whiteTexture = nullptr;
        _device = nullptr;
    }

    bool NextRenderer::IsActive() const
    {
        return (_device != nullptr);
    }

    bool NextRenderer::CreatePipelines()
    {
        _roomsVertexShader = LoadShader(*_device, _shaderDirectory, "Rooms", "VS");
        _roomsPixelShader = LoadShader(*_device, _shaderDirectory, "Rooms", "PS");
        if (_roomsVertexShader == nullptr || _roomsPixelShader == nullptr)
            return false;

        _roomsPipeline = CreateRoomsPipeline(false);
        _roomsAdditivePipeline = CreateRoomsPipeline(true);
        if (_roomsPipeline == nullptr || _roomsAdditivePipeline == nullptr)
            return false;

        auto samplerDesc = SamplerDesc{};
        samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = AddressMode::ClampToEdge;
        samplerDesc.MaxAnisotropy = 16;
        samplerDesc.Name = "AnisotropicClamp";
        _anisotropicSampler = _device->CreateSampler(samplerDesc);

        unsigned char whitePixel[4] = { 255, 255, 255, 255 };
        auto whiteTextureDesc = TextureDesc{};
        whiteTextureDesc.Name = "White";
        _whiteTexture = _device->CreateTexture(whiteTextureDesc);
        if (_whiteTexture == nullptr || _anisotropicSampler == nullptr)
            return false;

        auto transferBuffer = _device->CreateTransferBuffer(TransferBufferUsage::Upload, sizeof(whitePixel));
        memcpy(_device->MapTransferBuffer(transferBuffer.get(), false), whitePixel, sizeof(whitePixel));
        _device->UnmapTransferBuffer(transferBuffer.get());

        auto* commandBuffer = _device->AcquireCommandBuffer();
        auto* copyPass = commandBuffer->BeginCopyPass();
        auto region = TextureRegion{};
        region.Texture = _whiteTexture.get();
        region.Width = region.Height = 1;
        copyPass->UploadToTexture(TextureTransferInfo{ transferBuffer.get(), 0, 0, 0 }, region, false);
        commandBuffer->EndCopyPass();
        commandBuffer->Submit();
        _device->WaitIdle();

        return true;
    }

    std::unique_ptr<IGraphicsPipeline> NextRenderer::CreateRoomsPipeline(bool additive)
    {
        auto desc = GraphicsPipelineDesc{};
        desc.VertexShader = _roomsVertexShader.get();
        desc.PixelShader = _roomsPixelShader.get();
        desc.Layout.NumBuffers = 1;
        desc.Layout.Buffers[0] = VertexBufferLayout{ 0, (int)sizeof(Vertex), VertexInputRate::Vertex };
        desc.Layout.NumAttributes = 5;
        desc.Layout.Attributes[0] = VertexAttribute{ 0, 0, VertexElementFormat::Float3, (int)offsetof(Vertex, Position) };
        desc.Layout.Attributes[1] = VertexAttribute{ 1, 0, VertexElementFormat::Float2, (int)offsetof(Vertex, UV) };
        desc.Layout.Attributes[2] = VertexAttribute{ 2, 0, VertexElementFormat::UByte4Norm, (int)offsetof(Vertex, Color) };
        desc.Layout.Attributes[3] = VertexAttribute{ 3, 0, VertexElementFormat::UInt, (int)offsetof(Vertex, Effects) };
        desc.Layout.Attributes[4] = VertexAttribute{ 4, 0, VertexElementFormat::UInt, (int)offsetof(Vertex, AnimationFrameOffsetIndexHash) };
        desc.NumColorTargets = 1;
        desc.ColorTargets[0].Format = _device->GetSwapchainFormat();
        desc.DepthFormat = PixelFormat::D32Float;
        desc.Name = additive ? "Rooms.Additive" : "Rooms.Opaque";

        if (additive)
        {
            auto& blend = desc.ColorTargets[0].Blend;
            blend.EnableBlend = true;
            blend.SrcColor = BlendFactor::SrcAlpha;
            blend.DstColor = BlendFactor::One;
            blend.SrcAlpha = BlendFactor::SrcAlpha;
            blend.DstAlpha = BlendFactor::One;
            desc.DepthStencil.EnableDepthWrite = false;
        }

        return _device->CreateGraphicsPipeline(desc);
    }

    bool NextRenderer::EnsureRenderTargets(int width, int height)
    {
        if (_colorTarget != nullptr && _colorTarget->GetDesc().Width == width && _colorTarget->GetDesc().Height == height)
            return true;

        auto colorDesc = TextureDesc{};
        colorDesc.Width = width;
        colorDesc.Height = height;
        colorDesc.Format = _device->GetSwapchainFormat();
        colorDesc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        colorDesc.Name = "Scene.Color";
        _colorTarget = _device->CreateTexture(colorDesc);

        auto depthDesc = TextureDesc{};
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.Format = PixelFormat::D32Float;
        depthDesc.Usage = TextureUsage::DepthStencilTarget;
        depthDesc.Name = "Scene.Depth";
        _depthTarget = _device->CreateTexture(depthDesc);

        return (_colorTarget != nullptr && _depthTarget != nullptr);
    }

    void NextRenderer::UploadBuffer(IBuffer* buffer, const void* data, int size)
    {
        auto transferBuffer = _device->CreateTransferBuffer(TransferBufferUsage::Upload, size);
        memcpy(_device->MapTransferBuffer(transferBuffer.get(), false), data, size);
        _device->UnmapTransferBuffer(transferBuffer.get());

        auto* commandBuffer = _device->AcquireCommandBuffer();
        auto* copyPass = commandBuffer->BeginCopyPass();
        copyPass->UploadToBuffer(transferBuffer.get(), 0, buffer, 0, size, false);
        commandBuffer->EndCopyPass();
        commandBuffer->Submit();
        _device->WaitIdle();
    }

    std::unique_ptr<ITexture> NextRenderer::CreateLevelTexture(const std::vector<unsigned char>& encodedData, const std::string& name)
    {
        auto image = DecodedImage{};
        if (encodedData.empty() || !DecodeImage(encodedData, image))
            return nullptr;

        // Single-level images get their mip chain generated on the GPU, which needs a color target.
        bool generateMips = (image.MipLevels == 1);

        auto desc = TextureDesc{};
        desc.Width = image.Width;
        desc.Height = image.Height;
        desc.Format = image.Format;
        desc.MipLevels = generateMips ? GetMipLevelCount(image.Width, image.Height) : image.MipLevels;
        desc.Usage = TextureUsage::Sampler | (generateMips ? TextureUsage::ColorTarget : 0);
        desc.Name = name;

        auto texture = _device->CreateTexture(desc);
        if (texture == nullptr)
            return nullptr;

        auto transferBuffer = _device->CreateTransferBuffer(TransferBufferUsage::Upload, (int)image.Data.size());
        memcpy(_device->MapTransferBuffer(transferBuffer.get(), false), image.Data.data(), image.Data.size());
        _device->UnmapTransferBuffer(transferBuffer.get());

        auto* commandBuffer = _device->AcquireCommandBuffer();
        auto* copyPass = commandBuffer->BeginCopyPass();

        int offset = 0;
        for (int mip = 0; mip < image.MipLevels; mip++)
        {
            int mipWidth = std::max(1, image.Width >> mip);
            int mipHeight = std::max(1, image.Height >> mip);

            auto region = TextureRegion{};
            region.Texture = texture.get();
            region.MipLevel = mip;
            region.Width = mipWidth;
            region.Height = mipHeight;
            copyPass->UploadToTexture(TextureTransferInfo{ transferBuffer.get(), offset, 0, 0 }, region, false);

            offset += GetTextureDataSize(image.Format, mipWidth, mipHeight, 1);
        }

        commandBuffer->EndCopyPass();

        if (generateMips)
            _device->GenerateMipmaps(commandBuffer, texture.get());

        commandBuffer->Submit();
        _device->WaitIdle();
        return texture;
    }

    void NextRenderer::LoadLevel(const LegacyLevelGeometry& geometry)
    {
        if (_device == nullptr)
            return;

        auto lock = std::lock_guard<std::mutex>(_frameMutex);
        auto startTime = std::chrono::steady_clock::now();

        _device->WaitIdle();
        _isLevelLoaded = false;
        _roomTextures.clear();
        _animatedTextures.clear();

        int vertexDataSize = (int)(geometry.RoomVertices->size() * sizeof(Vertex));
        int indexDataSize = (int)(geometry.RoomIndices->size() * sizeof(int));
        if (vertexDataSize == 0 || indexDataSize == 0)
            return;

        _roomVertexBuffer = _device->CreateBuffer(BufferDesc{ vertexDataSize, 0, BufferUsage::Vertex, "Rooms.Vertices" });
        _roomIndexBuffer = _device->CreateBuffer(BufferDesc{ indexDataSize, 0, BufferUsage::Index, "Rooms.Indices" });
        UploadBuffer(_roomVertexBuffer.get(), geometry.RoomVertices->data(), vertexDataSize);
        UploadBuffer(_roomIndexBuffer.get(), geometry.RoomIndices->data(), indexDataSize);

        for (int i = 0; i < (int)g_Level.RoomTextures.size(); i++)
        {
            auto roomTexture = RoomTexture{};
            roomTexture.ColorMap = CreateLevelTexture(g_Level.RoomTextures[i].colorMapData, "RoomAtlas[" + std::to_string(i) + "].Color");
            _roomTextures.push_back(std::move(roomTexture));
        }

        LoadAnimatedTextures(geometry);

        _isLevelLoaded = true;

        int bucketCounts[2][16] = {};
        for (const auto& room : *geometry.Rooms)
        {
            for (const auto& bucket : room.Buckets)
                bucketCounts[bucket.Animated ? 1 : 0][std::clamp((int)bucket.BlendMode, 0, 15)]++;
        }

        auto bucketSummary = std::string();
        for (int animated = 0; animated < 2; animated++)
        {
            for (int mode = 0; mode < 16; mode++)
            {
                if (bucketCounts[animated][mode] > 0)
                    bucketSummary += fmt::format(" {}{}={}", animated ? "anim:" : "", mode, bucketCounts[animated][mode]);
            }
        }

        TENLog("Next renderer: room buckets by blend mode:" + bucketSummary, LogLevel::Info);

        float elapsed = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - startTime).count();
        TENLog(fmt::format("Next renderer: level data uploaded in {:.1f} ms ({} room vertices, {} room textures).",
            elapsed, geometry.RoomVertices->size(), _roomTextures.size()), LogLevel::Info);
    }

    // All animated texture sets live in two static storage buffers, so drawing an animated bucket only
    // pushes its set index; frames are selected in the vertex shader.
    void NextRenderer::LoadAnimatedTextures(const LegacyLevelGeometry& geometry)
    {
        for (int i = 0; i < (int)g_Level.AnimatedTextures.size(); i++)
        {
            auto animatedTexture = RoomTexture{};
            animatedTexture.ColorMap = CreateLevelTexture(g_Level.AnimatedTextures[i].colorMapData, "AnimatedAtlas[" + std::to_string(i) + "].Color");
            _animatedTextures.push_back(std::move(animatedTexture));
        }

        auto frames = std::vector<GpuAnimatedFrame>{};
        auto sets = std::vector<GpuAnimatedSet>{};
        for (const auto& set : *geometry.AnimatedTextureSets)
        {
            auto gpuSet = GpuAnimatedSet{};
            gpuSet.FirstFrame = (unsigned int)frames.size();
            gpuSet.NumFrames = (unsigned int)set.Textures.size();
            gpuSet.Fps = (unsigned int)set.Fps;
            gpuSet.Type = (unsigned int)set.Type;
            gpuSet.UVRotateDirection = set.UVRotateDirection;
            gpuSet.UVRotateSpeed = set.UVRotateSpeed;
            sets.push_back(gpuSet);

            for (const auto& texture : set.Textures)
                frames.push_back(GpuAnimatedFrame{ { texture.UV[0], texture.UV[1], texture.UV[2], texture.UV[3] } });
        }

        // Storage buffers cannot be empty, keep one placeholder entry.
        if (frames.empty())
            frames.push_back(GpuAnimatedFrame{});

        if (sets.empty())
            sets.push_back(GpuAnimatedSet{});

        int framesSize = (int)(frames.size() * sizeof(GpuAnimatedFrame));
        int setsSize = (int)(sets.size() * sizeof(GpuAnimatedSet));
        _animatedFramesBuffer = _device->CreateBuffer(BufferDesc{ framesSize, (int)sizeof(GpuAnimatedFrame), BufferUsage::GraphicsStorageRead, "AnimatedTextures.Frames" });
        _animatedSetsBuffer = _device->CreateBuffer(BufferDesc{ setsSize, (int)sizeof(GpuAnimatedSet), BufferUsage::GraphicsStorageRead, "AnimatedTextures.Sets" });
        UploadBuffer(_animatedFramesBuffer.get(), frames.data(), framesSize);
        UploadBuffer(_animatedSetsBuffer.get(), sets.data(), setsSize);
    }

    void NextRenderer::DrawRooms(ICommandBuffer& commandBuffer, IRenderPass& renderPass, const RenderView& view, bool additive)
    {
        commandBuffer.PushDebugGroup(additive ? "Rooms additive" : "Rooms opaque");

        renderPass.BindPipeline(additive ? _roomsAdditivePipeline.get() : _roomsPipeline.get());

        auto vertexBinding = BufferBinding{ _roomVertexBuffer.get(), 0 };
        renderPass.BindVertexBuffers(0, &vertexBinding, 1);
        renderPass.BindIndexBuffer(BufferBinding{ _roomIndexBuffer.get(), 0 }, IndexSize::Bits32);

        IBuffer* storageBuffers[2] = { _animatedFramesBuffer.get(), _animatedSetsBuffer.get() };
        renderPass.BindVertexStorageBuffers(0, storageBuffers, 2);

        auto lastPixelUniforms = DrawPixelUniforms{ -1.0f };
        int lastAnimatedSet = INT_MIN;
        const ITexture* lastTexture = nullptr;

        // Opaque rooms are drawn front to back (portal traversal order) for early depth rejection,
        // additive rooms back to front.
        int roomCount = (int)view.RoomsToDraw.size();
        for (int roomIndex = 0; roomIndex < roomCount; roomIndex++)
        {
            const auto* room = view.RoomsToDraw[additive ? (roomCount - 1 - roomIndex) : roomIndex];

            for (const auto& bucket : room->Buckets)
            {
                if (bucket.NumIndices == 0)
                    continue;

                bool isAdditive = (bucket.BlendMode == BlendMode::Additive);
                bool isOpaque = (bucket.BlendMode == BlendMode::Opaque || bucket.BlendMode == BlendMode::AlphaTest);
                if ((additive && !isAdditive) || (!additive && !isOpaque))
                    continue;

                auto pixelUniforms = DrawPixelUniforms{};
                pixelUniforms.AlphaThreshold = (bucket.BlendMode == BlendMode::AlphaTest) ? ALPHA_TEST_THRESHOLD : 0.0f;
                pixelUniforms.BlendMode = (unsigned int)bucket.BlendMode;
                if (pixelUniforms.AlphaThreshold != lastPixelUniforms.AlphaThreshold || pixelUniforms.BlendMode != lastPixelUniforms.BlendMode)
                {
                    commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 1, &pixelUniforms, sizeof(pixelUniforms));
                    lastPixelUniforms = pixelUniforms;
                }

                int animatedSet = bucket.Animated ? bucket.Texture : NO_VALUE;
                if (animatedSet != lastAnimatedSet)
                {
                    auto vertexUniforms = DrawVertexUniforms{ animatedSet };
                    commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 1, &vertexUniforms, sizeof(vertexUniforms));
                    lastAnimatedSet = animatedSet;
                }

                const auto& textures = bucket.Animated ? _animatedTextures : _roomTextures;
                auto* texture = (bucket.Texture >= 0 && bucket.Texture < (int)textures.size() && textures[bucket.Texture].ColorMap != nullptr) ?
                    textures[bucket.Texture].ColorMap.get() : _whiteTexture.get();

                if (texture != lastTexture)
                {
                    auto binding = TextureSamplerBinding{ texture, _anisotropicSampler.get() };
                    renderPass.BindPixelSamplers(0, &binding, 1);
                    lastTexture = texture;
                }

                renderPass.DrawIndexed(bucket.NumIndices, 1, bucket.StartIndex, 0, 0);
            }
        }

        commandBuffer.PopDebugGroup();
    }

    void NextRenderer::RenderFrame(const RenderView& view, const FrameEnvironment& environment)
    {
        if (_device == nullptr || !_isLevelLoaded)
            return;

        int width = _windowWidth;
        int height = _windowHeight;
        if (width <= 0 || height <= 0)
            return;

        auto lock = std::lock_guard<std::mutex>(_frameMutex);
        if (!EnsureRenderTargets(width, height))
            return;

        auto* commandBuffer = _device->AcquireCommandBuffer();
        if (commandBuffer == nullptr)
            return;

        commandBuffer->PushDebugGroup("Frame");

        // Projection is rebuilt for the preview window aspect ratio; position and orientation follow the game camera.
        auto frameVertexUniforms = FrameVertexUniforms{};
        frameVertexUniforms.ViewProjection = view.Camera.View * Matrix::CreatePerspectiveFieldOfView(view.Camera.FOV, (float)width / (float)height, view.Camera.NearPlane, view.Camera.FarPlane);
        frameVertexUniforms.CameraPosition = environment.CameraPosition;
        frameVertexUniforms.FogMinDistance = environment.FogMinDistance;
        frameVertexUniforms.FogMaxDistance = environment.FogMaxDistance;
        frameVertexUniforms.Frame = environment.Frame;
        frameVertexUniforms.InterpolatedFrame = environment.InterpolatedFrame;
        commandBuffer->PushUniforms(RHI::ShaderStage::Vertex, 0, &frameVertexUniforms, sizeof(frameVertexUniforms));

        auto framePixelUniforms = FramePixelUniforms{ environment.FogColor };
        commandBuffer->PushUniforms(RHI::ShaderStage::Pixel, 0, &framePixelUniforms, sizeof(framePixelUniforms));

        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = _colorTarget.get();
        colorTarget.ClearColor[0] = environment.FogColor.x;
        colorTarget.ClearColor[1] = environment.FogColor.y;
        colorTarget.ClearColor[2] = environment.FogColor.z;
        colorTarget.ClearColor[3] = 1.0f;

        auto depthTarget = DepthTargetInfo{};
        depthTarget.Texture = _depthTarget.get();
        depthTarget.Store = StoreOp::DontCare;

        commandBuffer->PushDebugGroup("Scene");
        auto* renderPass = commandBuffer->BeginRenderPass(&colorTarget, 1, &depthTarget);
        DrawRooms(*commandBuffer, *renderPass, view, false);
        DrawRooms(*commandBuffer, *renderPass, view, true);
        commandBuffer->EndRenderPass();
        commandBuffer->PopDebugGroup();

        if (!_device->RequiresWindowThreadPresent())
        {
            auto* swapchainTexture = commandBuffer->AcquireSwapchainTexture();
            if (swapchainTexture != nullptr)
                commandBuffer->BlitTexture(_colorTarget.get(), swapchainTexture);
        }

        commandBuffer->PopDebugGroup();
        commandBuffer->Submit();

        if (!_screenshotPath.empty())
        {
            TENLog(fmt::format("Next renderer environment: fog color ({:.2f}, {:.2f}, {:.2f}), fog {:.1f}-{:.1f}, frame {}.",
                environment.FogColor.x, environment.FogColor.y, environment.FogColor.z, environment.FogMinDistance, environment.FogMaxDistance, environment.Frame), LogLevel::Info);

            bool saved = SaveTextureToPng(*_device, _colorTarget.get(), _screenshotPath);
            TENLog("Next renderer screenshot " + _screenshotPath + (saved ? " saved." : " failed."), LogLevel::Info);
            _screenshotPath.clear();
        }

        if (_device->RequiresWindowThreadPresent())
            _isFrameReady = true;
    }

    void NextRenderer::RequestScreenshot(const std::string& path)
    {
        _screenshotPath = path;
    }

    void NextRenderer::PresentOnWindowThread()
    {
        if (_device == nullptr)
            return;

        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(_window, &width, &height);
        _windowWidth = width;
        _windowHeight = height;

        if (!_device->RequiresWindowThreadPresent() || !_isFrameReady)
            return;

        auto* commandBuffer = _device->AcquireCommandBuffer();
        if (commandBuffer == nullptr)
            return;

        // Acquisition may wait for vsync, so the frame lock is taken only around the blit.
        commandBuffer->PushDebugGroup("Present");
        auto* swapchainTexture = commandBuffer->AcquireSwapchainTexture();
        if (swapchainTexture != nullptr)
        {
            auto lock = std::lock_guard<std::mutex>(_frameMutex);
            if (_colorTarget != nullptr)
                commandBuffer->BlitTexture(_colorTarget.get(), swapchainTexture);

            _isFrameReady = false;
        }

        commandBuffer->PopDebugGroup();
        commandBuffer->Submit();
    }
}
