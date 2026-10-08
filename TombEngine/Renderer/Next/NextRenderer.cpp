#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Renderer/Next/TextureLoader.h"
#include "Renderer/RenderView.h"
#include "Renderer/RHI/ShaderLibrary.h"
#include "Renderer/RHI/TextureCapture.h"
#include "Game/Setup.h"
#include "Objects/game_object_ids.h"
#include "Renderer/Structures/RendererAnimatedTextureSet.h"
#include "Renderer/Structures/RendererLight.h"
#include "Renderer/Structures/RendererMesh.h"
#include "Renderer/Structures/RendererObject.h"
#include "Renderer/Structures/RendererRoom.h"
#include "Specific/level.h"

using namespace TEN::Renderer::RHI;
using namespace TEN::Renderer::Structures;
using TEN::Renderer::Graphics::Vertices::Vertex;

namespace TEN::Renderer::Next
{
    constexpr auto ALPHA_TEST_THRESHOLD      = 0.5f;
    constexpr auto INSTANCE_INDEX_COUNT      = 65536;
    constexpr auto INITIAL_INSTANCE_CAPACITY = 4096;

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
        unsigned int LightOffset    = 0;
        unsigned int LightCount     = 0;
    };

    // Same layout as the legacy ShaderLight and GpuLight in Lighting.hlsli.
    struct GpuLight
    {
        Vector3      Position  = Vector3::Zero;
        unsigned int Type      = 0;
        Vector3      Color     = Vector3::Zero;
        float        Intensity = 0.0f;
        Vector3      Direction = Vector3::Zero;
        float        In        = 0.0f;
        float        Out       = 0.0f;
        float        InRange   = 0.0f;
        float        OutRange  = 0.0f;
        float        Padding   = 0.0f;
    };

    static_assert(sizeof(GpuLight) == 64, "GpuLight must match Lighting.hlsli.");

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

    struct GpuObjectInstance
    {
        Matrix       World        = Matrix::Identity;
        Vector4      Color        = Vector4::One;
        Vector4      AmbientLight = Vector4::One;
        unsigned int LightMode    = 0;
        unsigned int LightOffset  = 0;
        unsigned int LightCount   = 0;
        unsigned int Padding      = 0;
    };

    static_assert(sizeof(GpuObjectInstance) == 112, "GpuObjectInstance must match ObjectInstance in Common.hlsli.");
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
        _animatedTextures.clear();
        _staticTextures.clear();
        _moveableTextures.clear();
        _staticGeometry = {};
        _moveableGeometry = {};
        _instanceBuffer = {};
        _lightBuffer = {};
        _instanceIndexBuffer = nullptr;
        _animatedFramesBuffer = nullptr;
        _animatedSetsBuffer = nullptr;
        _objectsPipeline = nullptr;
        _objectsAdditivePipeline = nullptr;
        _objectsVertexShader = nullptr;
        _objectsPixelShader = nullptr;
        _roomsAdditivePipeline = nullptr;
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

        _objectsVertexShader = LoadShader(*_device, _shaderDirectory, "Objects", "VS");
        _objectsPixelShader = LoadShader(*_device, _shaderDirectory, "Objects", "PS");
        if (_objectsVertexShader == nullptr || _objectsPixelShader == nullptr)
            return false;

        _objectsPipeline = CreateObjectsPipeline(false);
        _objectsAdditivePipeline = CreateObjectsPipeline(true);
        if (_objectsPipeline == nullptr || _objectsAdditivePipeline == nullptr)
            return false;

        // Instance index stream: entry N holds N, so with an instance-rate fetch the shader receives
        // firstInstance + instance on every backend, unlike SV_InstanceID.
        auto instanceIndices = std::vector<unsigned int>(INSTANCE_INDEX_COUNT);
        for (int i = 0; i < INSTANCE_INDEX_COUNT; i++)
            instanceIndices[i] = i;

        int instanceIndexSize = INSTANCE_INDEX_COUNT * (int)sizeof(unsigned int);
        _instanceIndexBuffer = _device->CreateBuffer(BufferDesc{ instanceIndexSize, 0, BufferUsage::Vertex, "InstanceIndices" });
        UploadBuffer(_instanceIndexBuffer.get(), instanceIndices.data(), instanceIndexSize);

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
        desc.Layout.NumAttributes = 6;
        desc.Layout.Attributes[0] = VertexAttribute{ 0, 0, VertexElementFormat::Float3, (int)offsetof(Vertex, Position) };
        desc.Layout.Attributes[1] = VertexAttribute{ 1, 0, VertexElementFormat::Float2, (int)offsetof(Vertex, UV) };
        desc.Layout.Attributes[2] = VertexAttribute{ 2, 0, VertexElementFormat::UByte4Norm, (int)offsetof(Vertex, Color) };
        desc.Layout.Attributes[3] = VertexAttribute{ 3, 0, VertexElementFormat::UInt, (int)offsetof(Vertex, Effects) };
        desc.Layout.Attributes[4] = VertexAttribute{ 4, 0, VertexElementFormat::UInt, (int)offsetof(Vertex, AnimationFrameOffsetIndexHash) };
        desc.Layout.Attributes[5] = VertexAttribute{ 5, 0, VertexElementFormat::Byte4Norm, (int)offsetof(Vertex, Normal) };
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

    std::unique_ptr<IGraphicsPipeline> NextRenderer::CreateObjectsPipeline(bool additive)
    {
        auto desc = GraphicsPipelineDesc{};
        desc.VertexShader = _objectsVertexShader.get();
        desc.PixelShader = _objectsPixelShader.get();
        desc.Layout.NumBuffers = 2;
        desc.Layout.Buffers[0] = VertexBufferLayout{ 0, (int)sizeof(Vertex), VertexInputRate::Vertex };
        desc.Layout.Buffers[1] = VertexBufferLayout{ 1, (int)sizeof(unsigned int), VertexInputRate::Instance };
        desc.Layout.NumAttributes = 7;
        desc.Layout.Attributes[0] = VertexAttribute{ 0, 0, VertexElementFormat::Float3, (int)offsetof(Vertex, Position) };
        desc.Layout.Attributes[1] = VertexAttribute{ 1, 0, VertexElementFormat::Float2, (int)offsetof(Vertex, UV) };
        desc.Layout.Attributes[2] = VertexAttribute{ 2, 0, VertexElementFormat::UByte4Norm, (int)offsetof(Vertex, Color) };
        desc.Layout.Attributes[3] = VertexAttribute{ 3, 0, VertexElementFormat::UInt, (int)offsetof(Vertex, Effects) };
        desc.Layout.Attributes[4] = VertexAttribute{ 4, 0, VertexElementFormat::UInt, (int)offsetof(Vertex, AnimationFrameOffsetIndexHash) };
        desc.Layout.Attributes[5] = VertexAttribute{ 5, 1, VertexElementFormat::UInt, 0 };
        desc.Layout.Attributes[6] = VertexAttribute{ 6, 0, VertexElementFormat::Byte4Norm, (int)offsetof(Vertex, Normal) };
        desc.NumColorTargets = 1;
        desc.ColorTargets[0].Format = _device->GetSwapchainFormat();
        desc.DepthFormat = PixelFormat::D32Float;
        desc.Name = additive ? "Objects.Additive" : "Objects.Opaque";

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
        _staticTextures.clear();
        _moveableTextures.clear();

        int vertexDataSize = (int)(geometry.RoomVertices->size() * sizeof(Vertex));
        int indexDataSize = (int)(geometry.RoomIndices->size() * sizeof(int));
        if (vertexDataSize == 0 || indexDataSize == 0)
            return;

        _roomVertexBuffer = _device->CreateBuffer(BufferDesc{ vertexDataSize, 0, BufferUsage::Vertex, "Rooms.Vertices" });
        _roomIndexBuffer = _device->CreateBuffer(BufferDesc{ indexDataSize, 0, BufferUsage::Index, "Rooms.Indices" });
        UploadBuffer(_roomVertexBuffer.get(), geometry.RoomVertices->data(), vertexDataSize);
        UploadBuffer(_roomIndexBuffer.get(), geometry.RoomIndices->data(), indexDataSize);

        LoadTextureSet(g_Level.RoomTextures, _roomTextures, "RoomAtlas");
        LoadTextureSet(g_Level.StaticsTextures, _staticTextures, "StaticAtlas");
        LoadTextureSet(g_Level.MoveablesTextures, _moveableTextures, "MoveableAtlas");
        LoadAnimatedTextures(geometry);

        CreateGeometryBuffers(_staticGeometry, *geometry.StaticVertices, *geometry.StaticIndices, "Statics");
        CreateGeometryBuffers(_moveableGeometry, *geometry.MoveableVertices, *geometry.MoveableIndices, "Moveables");
        _legacyGeometry = geometry;

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

    void NextRenderer::LoadTextureSet(const std::vector<::TEXTURE>& source, std::vector<RoomTexture>& destination, const std::string& name)
    {
        destination.clear();
        for (int i = 0; i < (int)source.size(); i++)
        {
            auto texture = RoomTexture{};
            texture.ColorMap = CreateLevelTexture(source[i].colorMapData, name + "[" + std::to_string(i) + "].Color");
            destination.push_back(std::move(texture));
        }
    }

    void NextRenderer::CreateGeometryBuffers(GeometryBuffers& buffers, const std::vector<Vertex>& vertices, const std::vector<int>& indices, const std::string& name)
    {
        buffers.Vertices = nullptr;
        buffers.Indices = nullptr;

        int vertexDataSize = (int)(vertices.size() * sizeof(Vertex));
        int indexDataSize = (int)(indices.size() * sizeof(int));
        if (vertexDataSize == 0 || indexDataSize == 0)
            return;

        buffers.Vertices = _device->CreateBuffer(BufferDesc{ vertexDataSize, 0, BufferUsage::Vertex, name + ".Vertices" });
        buffers.Indices = _device->CreateBuffer(BufferDesc{ indexDataSize, 0, BufferUsage::Index, name + ".Indices" });
        UploadBuffer(buffers.Vertices.get(), vertices.data(), vertexDataSize);
        UploadBuffer(buffers.Indices.get(), indices.data(), indexDataSize);
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

        IBuffer* lightBuffer = _lightBuffer.Buffer.get();
        renderPass.BindPixelStorageBuffers(0, &lightBuffer, 1);

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
                pixelUniforms.LightOffset = 0;
                pixelUniforms.LightCount = (unsigned int)_roomLightCount;
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

    // Builds the frame instance data and the instanced draw list. Statics are grouped by object type, so each
    // mesh bucket of a type is a single draw; rigid moveable meshes get one instance each.
    // Appends lights to the frame light buffer as the legacy BindLight does: interpolated positions, spot
    // ranges as cosines and, for moveables, intensity faded between the current and previous room.
    // Returns the light count with the light type mask in the high bits.
    int NextRenderer::AddLights(const std::vector<RendererLight*>& lights, float interpolationFactor, int roomNumber, int prevRoomNumber, float fade, int& lightOffset)
    {
        lightOffset = (int)(_lightData.size() / sizeof(GpuLight));

        int count = 0;
        int typeMask = 0;
        for (const auto* light : lights)
        {
            float fadeCoeff = 1.0f;
            if (!light->AffectNeighbourRooms && roomNumber != NO_VALUE && light->RoomNumber != NO_VALUE)
            {
                if (light->RoomNumber == roomNumber)
                    fadeCoeff = fade;
                else if (light->RoomNumber == prevRoomNumber)
                    fadeCoeff = 1.0f - fade;
                else
                    continue;
            }

            if (fadeCoeff == 0.0f)
                continue;

            auto gpuLight = GpuLight{};
            gpuLight.Position = light->Position;
            gpuLight.Type = (unsigned int)light->Type;
            gpuLight.Color = light->Color;
            gpuLight.Intensity = light->Intensity * fadeCoeff;
            gpuLight.Direction = light->Direction;
            gpuLight.In = light->In;
            gpuLight.Out = light->Out;
            gpuLight.InRange = light->InRange;
            gpuLight.OutRange = light->OutRange;

            if (light->Type == LightType::Spot)
            {
                gpuLight.InRange = cos(light->InRange * (PI / 180.0f));
                gpuLight.OutRange = cos(light->OutRange * (PI / 180.0f));
            }

            if (light->Hash != 0)
            {
                gpuLight.Position = Vector3::Lerp(light->PrevPosition, light->Position, interpolationFactor);
                gpuLight.Direction = Vector3::Lerp(light->PrevDirection, light->Direction, interpolationFactor);
            }

            gpuLight.Direction.Normalize();

            const auto* bytes = (const unsigned char*)&gpuLight;
            _lightData.insert(_lightData.end(), bytes, bytes + sizeof(gpuLight));

            typeMask |= (1 << (31 - (int)light->Type));
            count++;
        }

        return count | typeMask;
    }

    void NextRenderer::CollectObjects(const RenderView& view, float interpolationFactor)
    {
        _instanceData.clear();
        _lightData.clear();
        _objectDraws.clear();

        // Rooms share the view light list, as in the legacy renderer.
        int roomLightOffset = 0;
        _roomLightCount = AddLights(view.LightsToDraw, interpolationFactor, NO_VALUE, NO_VALUE, 1.0f, roomLightOffset);

        int instanceCount = 0;
        auto addInstance = [&](const Matrix& world, const Vector4& color, const Vector4& ambientLight, LightMode lightMode, int lightOffset, int lightCount)
        {
            auto instance = GpuObjectInstance{};
            instance.World = world;
            instance.Color = color;
            instance.AmbientLight = ambientLight;
            instance.LightMode = (unsigned int)lightMode;
            instance.LightOffset = (unsigned int)lightOffset;
            instance.LightCount = (unsigned int)lightCount;

            const auto* bytes = (const unsigned char*)&instance;
            _instanceData.insert(_instanceData.end(), bytes, bytes + sizeof(instance));
            return instanceCount++;
        };

        for (const auto& [objectNumber, statics] : view.SortedStaticsToDraw)
        {
            if (statics.empty())
                continue;

            int objectIndex = Statics.GetIndex(objectNumber);
            if (objectIndex < 0 || objectIndex >= (int)_legacyGeometry.StaticObjects->size())
                continue;

            const auto& object = (*_legacyGeometry.StaticObjects)[objectIndex];
            if (!object.has_value() || object->ObjectMeshes.empty())
                continue;

            const auto* mesh = object->ObjectMeshes[0];
            int firstInstance = instanceCount;
            for (const auto* staticMesh : statics)
            {
                int lightOffset = 0;
                int lightCount = AddLights(staticMesh->LightsToDraw, interpolationFactor, NO_VALUE, NO_VALUE, 1.0f, lightOffset);
                addInstance(staticMesh->World, staticMesh->Color, staticMesh->AmbientLight, mesh->LightMode, lightOffset, lightCount);
            }

            for (const auto& bucket : mesh->Buckets)
            {
                if (bucket.NumIndices > 0)
                    _objectDraws.push_back(ObjectDraw{ MeshSource::Statics, &bucket, firstInstance, (int)statics.size() });
            }
        }

        for (const auto* room : view.RoomsToDraw)
        {
            for (const auto* item : room->ItemsToDraw)
            {
                // The player, waterfalls and skinned objects are ported in a later milestone.
                if (item->ObjectID == ID_LARA || item->SkinIndex != NO_VALUE ||
                    (item->ObjectID >= ID_WATERFALL1 && item->ObjectID <= ID_WATERFALLSS2))
                {
                    continue;
                }

                const auto& object = (*_legacyGeometry.MoveableObjects)[item->ObjectID];
                if (!object.has_value() || object->Hidden)
                    continue;

                const auto& nativeItem = g_Level.Items[item->ItemNumber];

                int lightOffset = 0;
                int lightCount = AddLights(item->LightsToDraw, interpolationFactor, item->RoomNumber, item->PrevRoomNumber, item->LightFade, lightOffset);

                for (int meshIndex = 0; meshIndex < (int)item->MeshIndex.size(); meshIndex++)
                {
                    if (!nativeItem.MeshBits.Test(meshIndex))
                        continue;

                    const auto* mesh = (*_legacyGeometry.Meshes)[item->MeshIndex[meshIndex]];
                    auto world = item->InterpolatedAnimationTransforms[meshIndex] * item->InterpolatedWorld;
                    int instance = addInstance(world, item->Color, item->AmbientLight, mesh->LightMode, lightOffset, lightCount);

                    for (const auto& bucket : mesh->Buckets)
                    {
                        if (bucket.NumIndices > 0)
                            _objectDraws.push_back(ObjectDraw{ MeshSource::Moveables, &bucket, instance, 1 });
                    }
                }
            }
        }
    }

    void NextRenderer::UploadFrameStorage(ICommandBuffer& commandBuffer, FrameStorageBuffer& storage, const std::vector<unsigned char>& data, int stride, const char* name)
    {
        // Storage buffers cannot be empty, so the smallest buffer holds one element.
        int dataSize = std::max((int)data.size(), stride);
        if (dataSize > storage.Capacity || storage.Buffer == nullptr)
        {
            storage.Capacity = std::max(dataSize * 2, stride * INITIAL_INSTANCE_CAPACITY);
            storage.Buffer = _device->CreateBuffer(BufferDesc{ storage.Capacity, stride, BufferUsage::GraphicsStorageRead, name });
            storage.Transfer = _device->CreateTransferBuffer(TransferBufferUsage::Upload, storage.Capacity);
        }

        if (data.empty())
            return;

        // Cycling renames buffers still in use by previous frames instead of stalling.
        memcpy(_device->MapTransferBuffer(storage.Transfer.get(), true), data.data(), data.size());
        _device->UnmapTransferBuffer(storage.Transfer.get());

        auto* copyPass = commandBuffer.BeginCopyPass();
        copyPass->UploadToBuffer(storage.Transfer.get(), 0, storage.Buffer.get(), 0, (int)data.size(), true);
        commandBuffer.EndCopyPass();
    }

    void NextRenderer::DrawObjects(ICommandBuffer& commandBuffer, IRenderPass& renderPass, bool additive)
    {
        if (_objectDraws.empty() || _instanceBuffer.Buffer == nullptr)
            return;

        commandBuffer.PushDebugGroup(additive ? "Objects additive" : "Objects opaque");
        renderPass.BindPipeline(additive ? _objectsAdditivePipeline.get() : _objectsPipeline.get());

        IBuffer* storageBuffers[3] = { _animatedFramesBuffer.get(), _animatedSetsBuffer.get(), _instanceBuffer.Buffer.get() };
        renderPass.BindVertexStorageBuffers(0, storageBuffers, 3);

        IBuffer* lightBuffer = _lightBuffer.Buffer.get();
        renderPass.BindPixelStorageBuffers(0, &lightBuffer, 1);

        auto lastSource = (MeshSource)-1;
        auto lastPixelUniforms = DrawPixelUniforms{ -1.0f };
        int lastAnimatedSet = INT_MIN;
        const ITexture* lastTexture = nullptr;

        for (const auto& draw : _objectDraws)
        {
            const auto& bucket = *draw.Bucket;
            bool isAdditive = (bucket.BlendMode == BlendMode::Additive);
            bool isOpaque = (bucket.BlendMode == BlendMode::Opaque || bucket.BlendMode == BlendMode::AlphaTest);
            if ((additive && !isAdditive) || (!additive && !isOpaque))
                continue;

            if (draw.Source != lastSource)
            {
                const auto& geometry = (draw.Source == MeshSource::Statics) ? _staticGeometry : _moveableGeometry;
                BufferBinding vertexBindings[2] = { { geometry.Vertices.get(), 0 }, { _instanceIndexBuffer.get(), 0 } };
                renderPass.BindVertexBuffers(0, vertexBindings, 2);
                renderPass.BindIndexBuffer(BufferBinding{ geometry.Indices.get(), 0 }, IndexSize::Bits32);
                lastSource = draw.Source;
            }

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

            const auto& textures = bucket.Animated ? _animatedTextures : ((draw.Source == MeshSource::Statics) ? _staticTextures : _moveableTextures);
            auto* texture = (bucket.Texture >= 0 && bucket.Texture < (int)textures.size() && textures[bucket.Texture].ColorMap != nullptr) ?
                textures[bucket.Texture].ColorMap.get() : _whiteTexture.get();

            if (texture != lastTexture)
            {
                auto binding = TextureSamplerBinding{ texture, _anisotropicSampler.get() };
                renderPass.BindPixelSamplers(0, &binding, 1);
                lastTexture = texture;
            }

            renderPass.DrawIndexed(bucket.NumIndices, draw.InstanceCount, bucket.StartIndex, 0, draw.FirstInstance);
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

        CollectObjects(view, environment.InterpolationFactor);

        commandBuffer->PushDebugGroup("Upload frame data");
        UploadFrameStorage(*commandBuffer, _instanceBuffer, _instanceData, (int)sizeof(GpuObjectInstance), "Frame.Instances");
        UploadFrameStorage(*commandBuffer, _lightBuffer, _lightData, (int)sizeof(GpuLight), "Frame.Lights");
        commandBuffer->PopDebugGroup();

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
        DrawObjects(*commandBuffer, *renderPass, false);
        DrawRooms(*commandBuffer, *renderPass, view, true);
        DrawObjects(*commandBuffer, *renderPass, true);
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
