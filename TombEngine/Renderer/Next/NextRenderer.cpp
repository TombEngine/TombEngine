#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Game/Animation/Animation.h"
#include "Game/control/control.h"
#include "Game/effects/hair.h"
#include "Game/effects/Decal.h"
#include "Game/effects/weather.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Objects/game_object_ids.h"
#include "Renderer/Next/GpuTypes.h"
#include "Renderer/Next/TextureLoader.h"
#include "Renderer/RenderView.h"
#include "Renderer/RHI/ShaderLibrary.h"
#include "Renderer/RHI/TextureCapture.h"
#include "Renderer/Structures/RendererAnimatedTextureSet.h"
#include "Renderer/Structures/RendererLight.h"
#include "Renderer/Structures/RendererMesh.h"
#include "Renderer/Structures/RendererObject.h"
#include "Renderer/Structures/RendererRoom.h"
#include "Renderer/Structures/RendererSortableObject.h"
#include "Renderer/Structures/RendererSprite.h"
#include "Renderer/Structures/RendererSpriteToDraw.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Specific/configuration.h"
#include "Specific/level.h"
#include "Specific/Structures/MaterialData.h"

using namespace TEN::Renderer::RHI;
using namespace TEN::Renderer::Structures;
using TEN::Renderer::Graphics::Vertices::Vertex;

namespace TEN::Renderer::Next
{
    constexpr auto INITIAL_INSTANCE_CAPACITY = 4096;

    NextRenderer g_NextRenderer;

    bool NextRenderer::Initialize(SDL_Window* window, Backend backend, const std::string& gameDirectory, bool preferLowPowerGpu)
    {
        _device = CreateDevice(backend, window, DEBUG_BUILD, preferLowPowerGpu);
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
        _boneBuffer = {};
        _instanceIndexBuffer = nullptr;
        _animatedFramesBuffer = nullptr;
        _animatedSetsBuffer = nullptr;
        LogGpuTimings();

        for (auto& shaderPipelines : _scenePipelines)
        {
            for (auto& pipeline : shaderPipelines)
                pipeline = nullptr;
        }

        _sortedIndexBuffer = {};
        _objectsVertexShader = nullptr;
        _objectsPixelShader = nullptr;
        _roomVertexBuffer = nullptr;
        _roomIndexBuffer = nullptr;
        _colorTarget = nullptr;
        _depthTarget = nullptr;
        _roomsVertexShader = nullptr;
        _roomsPixelShader = nullptr;
        _anisotropicSampler = nullptr;
        _whiteTexture = nullptr;
        _flatNormalTexture = nullptr;
        _defaultORSHTexture = nullptr;
        _blackTexture = nullptr;
        _materialBuffer = {};
        _decalBuffer = {};
        _skyTexture = nullptr;
        _skyVertexBuffer = nullptr;
        _skyIndexBuffer = nullptr;
        _skyVertexShader = nullptr;
        _skyPixelShader = nullptr;
        _objectsShadowPixelShader = nullptr;
        _shadowMap = nullptr;
        _dummyShadowMap = nullptr;
        _shadowDepth = nullptr;
        _postProcessPipeline = nullptr;
        _postProcessVertexShader = nullptr;
        _postProcessPixelShader = nullptr;
        _sceneTarget = nullptr;
        _ssaoPipeline = nullptr;
        _ssaoBlurHorizontalPipeline = nullptr;
        _ssaoBlurVerticalPipeline = nullptr;
        _ssaoVertexShader = nullptr;
        _ssaoPixelShader = nullptr;
        _ssaoBlurHorizontalShader = nullptr;
        _ssaoBlurVerticalShader = nullptr;
        _roomsPrepassPixelShader = nullptr;
        _objectsPrepassPixelShader = nullptr;
        _ssaoNoiseTexture = nullptr;
        _normalTarget = nullptr;
        _ssaoTarget = nullptr;
        _ssaoBlurTarget = nullptr;
        _pointClampSampler = nullptr;
        _pointWrapSampler = nullptr;
        _spritesVertexShader = nullptr;
        _spritesPixelShader = nullptr;
        _spriteCornerBuffer = nullptr;
        _linearClampSampler = nullptr;
        _spriteBuffer = {};
        _spriteTextures.clear();
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


        _objectsVertexShader = LoadShader(*_device, _shaderDirectory, "Objects", "VS");
        _objectsPixelShader = LoadShader(*_device, _shaderDirectory, "Objects", "PS");
        if (_objectsVertexShader == nullptr || _objectsPixelShader == nullptr)
            return false;

        _skyVertexShader = LoadShader(*_device, _shaderDirectory, "Sky", "VS");
        _skyPixelShader = LoadShader(*_device, _shaderDirectory, "Sky", "PS");
        if (_skyVertexShader == nullptr || _skyPixelShader == nullptr || !CreateSkyGeometry())
            return false;

        if (!CreateSpriteResources() || !CreateSSAOResources() || !CreatePostProcessResources() || !CreateShadowResources())
            return false;

        if (GetScenePipeline(SceneShader::Rooms, BlendMode::Opaque) == nullptr ||
            GetScenePipeline(SceneShader::Objects, BlendMode::Opaque) == nullptr)
        {
            return false;
        }

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

        // Defaults of the legacy renderer for atlas pages without material maps.
        constexpr unsigned char WHITE_PIXEL[4]       = { 255, 255, 255, 255 };
        constexpr unsigned char FLAT_NORMAL_PIXEL[4] = { 128, 128, 255, 255 };
        constexpr unsigned char ORSH_PIXEL[4]        = { 255, 255, 0, 255 };
        constexpr unsigned char BLACK_PIXEL[4]       = { 0, 0, 0, 0 };

        _whiteTexture = CreateSolidTexture(WHITE_PIXEL, "White");
        _flatNormalTexture = CreateSolidTexture(FLAT_NORMAL_PIXEL, "FlatNormal");
        _defaultORSHTexture = CreateSolidTexture(ORSH_PIXEL, "DefaultORSH");
        _blackTexture = CreateSolidTexture(BLACK_PIXEL, "Black");

        return (_anisotropicSampler != nullptr && _whiteTexture != nullptr && _flatNormalTexture != nullptr &&
                _defaultORSHTexture != nullptr && _blackTexture != nullptr);
    }

    std::unique_ptr<ITexture> NextRenderer::CreateSolidTexture(const unsigned char color[4], const std::string& name)
    {
        auto desc = TextureDesc{};
        desc.Name = name;
        auto texture = _device->CreateTexture(desc);
        if (texture != nullptr)
            UploadTexture(texture.get(), color, 4);

        return texture;
    }

    // Synchronous upload of the first mip level, for small textures created at initialization.
    void NextRenderer::UploadTexture(ITexture* texture, const void* data, int size)
    {
        auto transferBuffer = _device->CreateTransferBuffer(TransferBufferUsage::Upload, size);
        memcpy(_device->MapTransferBuffer(transferBuffer.get(), false), data, size);
        _device->UnmapTransferBuffer(transferBuffer.get());

        auto* commandBuffer = _device->AcquireCommandBuffer();
        auto* copyPass = commandBuffer->BeginCopyPass();
        auto region = TextureRegion{};
        region.Texture = texture;
        region.Width = texture->GetDesc().Width;
        region.Height = texture->GetDesc().Height;
        copyPass->UploadToTexture(TextureTransferInfo{ transferBuffer.get(), 0, 0, 0 }, region, false);
        commandBuffer->EndCopyPass();
        commandBuffer->Submit();
        _device->WaitIdle();
    }

    // Blend states of the legacy DX11 renderer, per blend mode.
    static BlendState GetBlendState(BlendMode blendMode)
    {
        auto state = BlendState{};
        state.EnableBlend = true;

        switch (blendMode)
        {
        case BlendMode::Additive:
            state.SrcColor = state.SrcAlpha = BlendFactor::SrcAlpha;
            state.DstColor = state.DstAlpha = BlendFactor::One;
            break;

        case BlendMode::AlphaBlend:
            state.SrcColor = state.SrcAlpha = BlendFactor::SrcAlpha;
            state.DstColor = state.DstAlpha = BlendFactor::OneMinusSrcAlpha;
            break;

        case BlendMode::PremultipliedAlphaBlend:
            state.SrcColor = state.SrcAlpha = BlendFactor::One;
            state.DstColor = state.DstAlpha = BlendFactor::OneMinusSrcAlpha;
            break;

        case BlendMode::Subtractive:
            state.SrcColor = BlendFactor::SrcAlpha;
            state.DstColor = BlendFactor::One;
            state.ColorOp = BlendOp::ReverseSubtract;
            state.SrcAlpha = BlendFactor::SrcAlpha;
            state.DstAlpha = BlendFactor::OneMinusSrcAlpha;
            break;

        case BlendMode::Screen:
            state.SrcColor = BlendFactor::One;
            state.DstColor = BlendFactor::OneMinusSrcColor;
            state.SrcAlpha = BlendFactor::One;
            state.DstAlpha = BlendFactor::OneMinusSrcAlpha;
            break;

        case BlendMode::Lighten:
            state.SrcColor = BlendFactor::One;
            state.DstColor = BlendFactor::One;
            state.ColorOp = BlendOp::Max;
            state.SrcAlpha = BlendFactor::SrcAlpha;
            state.DstAlpha = BlendFactor::DstAlpha;
            break;

        case BlendMode::Exclude:
            state.SrcColor = BlendFactor::OneMinusDstColor;
            state.DstColor = BlendFactor::OneMinusSrcColor;
            state.ColorOp = BlendOp::Subtract;
            state.SrcAlpha = BlendFactor::One;
            state.DstAlpha = BlendFactor::Zero;
            break;

        default:
            state.EnableBlend = false;
            break;
        }

        return state;
    }

    // Pipelines are created on first use per shader and blend mode; opaque and alpha test share one.
    IGraphicsPipeline* NextRenderer::GetScenePipeline(SceneShader shader, BlendMode blendMode)
    {
        int modeIndex = (blendMode == BlendMode::AlphaTest) ? (int)BlendMode::Opaque : std::clamp((int)blendMode, 0, 15);
        auto& pipeline = _scenePipelines[(int)shader][modeIndex];
        if (pipeline != nullptr)
            return pipeline.get();

        bool isObjects = (shader == SceneShader::Objects || shader == SceneShader::ObjectsPrepass);
        bool isSky = (shader == SceneShader::Sky);
        bool isSprites = (shader == SceneShader::Sprites);
        bool isPrepass = (shader == SceneShader::RoomsPrepass || shader == SceneShader::ObjectsPrepass);
        bool isShadow = (shader == SceneShader::ObjectsShadow);
        isObjects = isObjects || isShadow;

        IShader* vertexShaders[(int)SceneShader::Count] =
        {
            _roomsVertexShader.get(), _objectsVertexShader.get(), _skyVertexShader.get(), _spritesVertexShader.get(),
            _roomsVertexShader.get(), _objectsVertexShader.get(), _objectsVertexShader.get()
        };
        IShader* pixelShaders[(int)SceneShader::Count] =
        {
            _roomsPixelShader.get(), _objectsPixelShader.get(), _skyPixelShader.get(), _spritesPixelShader.get(),
            _roomsPrepassPixelShader.get(), _objectsPrepassPixelShader.get(), _objectsShadowPixelShader.get()
        };
        const char* names[(int)SceneShader::Count] = { "Rooms", "Objects", "Sky", "Sprites", "RoomsPrepass", "ObjectsPrepass", "ObjectsShadow" };

        auto desc = GraphicsPipelineDesc{};
        desc.VertexShader = vertexShaders[(int)shader];
        desc.PixelShader = pixelShaders[(int)shader];
        desc.Layout.NumBuffers = (isObjects || isSprites) ? 2 : 1;
        desc.Layout.Buffers[0] = VertexBufferLayout{ 0, isSprites ? (int)sizeof(unsigned int) : (int)sizeof(Vertex), VertexInputRate::Vertex };
        desc.Layout.Buffers[1] = VertexBufferLayout{ 1, (int)sizeof(unsigned int), VertexInputRate::Instance };

        auto& attributes = desc.Layout.Attributes;
        attributes[0] = VertexAttribute{ 0, 0, VertexElementFormat::Float3, (int)offsetof(Vertex, Position) };
        attributes[1] = VertexAttribute{ 1, 0, VertexElementFormat::Float2, (int)offsetof(Vertex, UV) };
        attributes[2] = VertexAttribute{ 2, 0, VertexElementFormat::UByte4Norm, (int)offsetof(Vertex, Color) };
        attributes[3] = VertexAttribute{ 3, 0, VertexElementFormat::UInt, (int)offsetof(Vertex, Effects) };
        attributes[4] = VertexAttribute{ 4, 0, VertexElementFormat::UInt, (int)offsetof(Vertex, AnimationFrameOffsetIndexHash) };

        if (isSprites)
        {
            attributes[0] = VertexAttribute{ 0, 0, VertexElementFormat::UInt, 0 };
            attributes[1] = VertexAttribute{ 1, 1, VertexElementFormat::UInt, 0 };
            desc.Layout.NumAttributes = 2;
            desc.Rasterizer.Cull = RHI::CullMode::None;
        }
        else if (isObjects)
        {
            attributes[5] = VertexAttribute{ 5, 1, VertexElementFormat::UInt, 0 };
            attributes[6] = VertexAttribute{ 6, 0, VertexElementFormat::Byte4Norm, (int)offsetof(Vertex, Normal) };
            attributes[7] = VertexAttribute{ 7, 0, VertexElementFormat::UByte4, (int)offsetof(Vertex, BoneIndex) };
            attributes[8] = VertexAttribute{ 8, 0, VertexElementFormat::UByte4Norm, (int)offsetof(Vertex, BoneWeight) };
            attributes[9] = VertexAttribute{ 9, 0, VertexElementFormat::Byte4Norm, (int)offsetof(Vertex, Tangent) };
            attributes[10] = VertexAttribute{ 10, 0, VertexElementFormat::Byte4Norm, (int)offsetof(Vertex, FaceNormal) };
            desc.Layout.NumAttributes = 11;
        }
        else if (isSky)
        {
            desc.Layout.NumAttributes = 5;
        }
        else
        {
            attributes[5] = VertexAttribute{ 5, 0, VertexElementFormat::Byte4Norm, (int)offsetof(Vertex, Normal) };
            attributes[6] = VertexAttribute{ 6, 0, VertexElementFormat::Byte4Norm, (int)offsetof(Vertex, Tangent) };
            attributes[7] = VertexAttribute{ 7, 0, VertexElementFormat::Byte4Norm, (int)offsetof(Vertex, FaceNormal) };
            desc.Layout.NumAttributes = 8;
        }

        desc.NumColorTargets = 1;
        desc.ColorTargets[0].Format = isPrepass ? PixelFormat::RGBA8Unorm : (isShadow ? PixelFormat::R32Float : _device->GetSwapchainFormat());
        desc.ColorTargets[0].Blend = GetBlendState((isPrepass || isShadow) ? BlendMode::Opaque : (BlendMode)modeIndex);
        desc.DepthStencil.EnableDepthWrite = (modeIndex == (int)BlendMode::Opaque) && !isSky && !isSprites;
        desc.DepthStencil.EnableDepthTest = !isSky;
        desc.DepthFormat = PixelFormat::D32Float;
        desc.Name = std::string(names[(int)shader]) + ".BlendMode" + std::to_string(modeIndex);

        pipeline = _device->CreateGraphicsPipeline(desc);
        if (pipeline == nullptr)
            TENLog("Next renderer: cannot create pipeline " + desc.Name, LogLevel::Error);

        return pipeline.get();
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
        colorDesc.Name = "Frame.Color";
        _colorTarget = _device->CreateTexture(colorDesc);

        colorDesc.Name = "Scene.Color";
        _sceneTarget = _device->CreateTexture(colorDesc);

        auto depthDesc = TextureDesc{};
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.Format = PixelFormat::D32Float;
        depthDesc.Usage = TextureUsage::DepthStencilTarget | TextureUsage::Sampler;
        depthDesc.Name = "Scene.Depth";
        _depthTarget = _device->CreateTexture(depthDesc);

        return (_colorTarget != nullptr && _sceneTarget != nullptr && _depthTarget != nullptr && EnsureSSAOTargets(width, height));
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
        LoadTextureSet(g_Level.SpritesTextures, _spriteTextures, "SpriteAtlas");
        LoadAnimatedTextures(geometry);
        _skyTexture = CreateLevelTexture(g_Level.SkyTexture.colorMapData, "Sky");

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
            auto pageName = name + "[" + std::to_string(i) + "]";

            auto texture = RoomTexture{};
            texture.ColorMap = CreateLevelTexture(source[i].colorMapData, pageName + ".Color");
            if (!source[i].normalMapData.empty())
                texture.NormalMap = CreateLevelTexture(source[i].normalMapData, pageName + ".Normal");

            if (!source[i].ORSHMapData.empty())
                texture.ORSHMap = CreateLevelTexture(source[i].ORSHMapData, pageName + ".ORSH");

            if (!source[i].emissiveMapData.empty())
                texture.EmissiveMap = CreateLevelTexture(source[i].emissiveMapData, pageName + ".Emissive");

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

        AddWaterfallSets(geometry, frames, sets);

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

    // Legacy DrawWaterfalls scrolls waterfall objects vertically within the V range of their first triangle at a fixed
    // speed. That range only depends on the object, so each waterfall object gets a set with a single frame.
    void NextRenderer::AddWaterfallSets(const LegacyLevelGeometry& geometry, std::vector<GpuAnimatedFrame>& frames, std::vector<GpuAnimatedSet>& sets)
    {
        constexpr auto WATERFALL_SPEED = 10;

        _waterfallSets.clear();
        for (int objectID = ID_WATERFALL1; objectID <= ID_WATERFALLSS2; objectID++)
        {
            const auto& object = (*geometry.MoveableObjects)[objectID];
            if (!object.has_value() || object->ObjectMeshes.empty() || object->ObjectMeshes[0]->Buckets.empty())
                continue;

            const auto& bucket = object->ObjectMeshes[0]->Buckets[0];
            if (bucket.StartVertex + 3 > (int)geometry.MoveableVertices->size())
                continue;

            float minV = FLT_MAX;
            float maxV = -FLT_MAX;
            for (int i = 0; i < 3; i++)
            {
                float v = (*geometry.MoveableVertices)[bucket.StartVertex + i].UV.y;
                minV = std::min(minV, v);
                maxV = std::max(maxV, v);
            }

            auto set = GpuAnimatedSet{};
            set.FirstFrame = (unsigned int)frames.size();
            set.NumFrames = 1;
            set.Fps = WATERFALL_SPEED;
            set.Type = ANIM_TYPE_LEGACY_WATERFALL;

            _waterfallSets[objectID] = (int)sets.size();
            sets.push_back(set);
            frames.push_back(GpuAnimatedFrame{ { Vector2(0.0f, minV), Vector2(1.0f, minV), Vector2(1.0f, maxV), Vector2(0.0f, maxV) } });
        }
    }

    // Material properties can be animated by scripts, so the whole table is rebuilt every frame.
    void NextRenderer::CollectMaterials(float interpolationFactor)
    {
        _materialData.resize(g_Level.Materials.size() * sizeof(GpuMaterial));
        auto* materials = (GpuMaterial*)_materialData.data();

        for (int i = 0; i < (int)g_Level.Materials.size(); i++)
        {
            const auto& material = g_Level.Materials[i];
            auto properties = material.GetInterpolatedProperties(interpolationFactor);

            auto& gpuMaterial = materials[i];
            gpuMaterial = GpuMaterial{};
            for (int j = 0; j < MaterialData::PropertyCount; j++)
                gpuMaterial.Properties[j] = properties[j];

            gpuMaterial.TypeAndFlags = (unsigned int)material.Type |
                                       ((unsigned int)material.HasHeightMap << 8) |
                                       ((unsigned int)material.HasAmbientOcclusionMap << 9) |
                                       ((unsigned int)material.HasEmissiveMap << 10);
        }
    }

    void NextRenderer::BindScenePixelStorage(IRenderPass& renderPass)
    {
        IBuffer* buffers[3] = { _lightBuffer.Buffer.get(), _materialBuffer.Buffer.get(), _decalBuffer.Buffer.get() };
        renderPass.BindPixelStorageBuffers(0, buffers, 3);
    }

    NextRenderer::BucketDrawInfo NextRenderer::GetBucketDrawInfo(int roomNumber, bool hasDecals)
    {
        auto info = BucketDrawInfo{};
        info.IsWaterRoom = (g_Level.Rooms[roomNumber].flags & ENV_FLAG_WATER) ? 1 : 0;
        info.IsCausticsRoom = IsCausticsRoom(roomNumber) ? 1 : 0;

        auto it = _roomDecalSlices.find(roomNumber);
        if (hasDecals && it != _roomDecalSlices.end())
        {
            info.DecalOffset = (unsigned int)it->second.first;
            info.DecalCount = (unsigned int)it->second.second;
        }

        return info;
    }

    // Decals of the visible rooms, as in the legacy BindRoomDecals.
    void NextRenderer::CollectDecals(const RenderView& view)
    {
        _decalData.clear();
        _roomDecalSlices.clear();
        if (!g_Configuration.EnableDecals)
            return;

        for (const auto* room : view.RoomsToDraw)
        {
            if (room->Decals.empty())
                continue;

            int offset = (int)(_decalData.size() / sizeof(GpuDecal));
            int count = std::min((int)room->Decals.size(), (int)TEN::Effects::Decal::Decal::COUNT_MAX);
            for (int i = 0; i < count; i++)
            {
                const auto& decal = room->Decals[i];
                auto gpuDecal = GpuDecal{ decal.Position, decal.Radius, decal.Opacity, decal.Pattern };

                const auto* bytes = (const unsigned char*)&gpuDecal;
                _decalData.insert(_decalData.end(), bytes, bytes + sizeof(gpuDecal));
            }

            _roomDecalSlices[room->RoomNumber] = std::pair<int, int>(offset, count);
        }
    }

    void NextRenderer::BindTexturePage(IRenderPass& renderPass, const RoomTexture* page)
    {
        auto getMap = [&](const std::unique_ptr<ITexture>* map, ITexture* fallback)
        {
            return (page != nullptr && *map != nullptr) ? map->get() : fallback;
        };

        auto* sampler = _anisotropicSampler.get();
        auto* ambientOcclusion = _isAmbientOcclusionActive ? _ssaoTarget.get() : _whiteTexture.get();
        auto* shadowMap = _isRenderingShadowMap ? _dummyShadowMap.get() : _shadowMap.get();
        auto* caustics = (_causticsTexture != nullptr) ? _causticsTexture : _whiteTexture.get();
        TextureSamplerBinding bindings[7] =
        {
            { getMap(page ? &page->ColorMap : nullptr, _whiteTexture.get()), sampler },
            { getMap(page ? &page->NormalMap : nullptr, _flatNormalTexture.get()), sampler },
            { getMap(page ? &page->ORSHMap : nullptr, _defaultORSHTexture.get()), sampler },
            { getMap(page ? &page->EmissiveMap : nullptr, _blackTexture.get()), sampler },
            { ambientOcclusion, _linearClampSampler.get() },
            { shadowMap, _pointClampSampler.get() },
            { caustics, sampler }
        };
        renderPass.BindPixelSamplers(0, bindings, 7);
    }

    void NextRenderer::BindBucket(ICommandBuffer& commandBuffer, IRenderPass& renderPass, const RendererBucket& bucket, const std::vector<RoomTexture>& pages,
                                  BlendMode blendMode, unsigned int lightCount, const BucketDrawInfo& room, BucketBindState& state)
    {
        auto pixelUniforms = DrawPixelUniforms{};
        pixelUniforms.AlphaThreshold = (bucket.BlendMode == BlendMode::AlphaTest) ? ALPHA_TEST_THRESHOLD : 0.0f;
        pixelUniforms.BlendMode = (unsigned int)blendMode;
        if (room.IsAlphaEdgePass)
        {
            pixelUniforms.AlphaThreshold = -ALPHA_TEST_THRESHOLD;
            pixelUniforms.BlendMode = (unsigned int)BlendMode::AlphaBlend;
        }
        pixelUniforms.LightCount = lightCount;
        pixelUniforms.MaterialIndex = (bucket.MaterialIndex >= 0 && bucket.MaterialIndex < (int)g_Level.Materials.size()) ? (unsigned int)bucket.MaterialIndex : 0;
        pixelUniforms.IsCausticsRoom = room.IsCausticsRoom;
        pixelUniforms.DecalOffset = room.DecalOffset;
        pixelUniforms.DecalCount = bucket.Animated ? 0 : room.DecalCount;
        if (memcmp(&pixelUniforms, &state.PixelUniforms, sizeof(pixelUniforms)) != 0)
        {
            commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 1, &pixelUniforms, sizeof(pixelUniforms));
            state.PixelUniforms = pixelUniforms;
        }

        int animatedSet = (room.AnimatedSetOverride != NO_VALUE) ? room.AnimatedSetOverride : (bucket.Animated ? bucket.Texture : NO_VALUE);
        auto vertexUniforms = DrawVertexUniforms{ animatedSet, room.IsWaterRoom };
        if (vertexUniforms.AnimatedSetIndex != state.VertexUniforms.AnimatedSetIndex || vertexUniforms.IsWaterRoom != state.VertexUniforms.IsWaterRoom)
        {
            commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 1, &vertexUniforms, sizeof(vertexUniforms));
            state.VertexUniforms = vertexUniforms;
        }

        const auto& sourcePages = bucket.Animated ? _animatedTextures : pages;
        const auto* page = (bucket.Texture >= 0 && bucket.Texture < (int)sourcePages.size()) ? &sourcePages[bucket.Texture] : nullptr;
        if (!state.IsPageBound || page != state.Page)
        {
            BindTexturePage(renderPass, page);
            state.Page = page;
            state.IsPageBound = true;
        }
    }

    bool NextRenderer::IsBucketInPass(const RendererBucket& bucket, ScenePass pass)
    {
        switch (pass)
        {
        case ScenePass::Prepass:
        case ScenePass::Opaque:
            return (bucket.BlendMode == BlendMode::Opaque || bucket.BlendMode == BlendMode::AlphaTest);

        case ScenePass::AlphaEdges:
            return (bucket.BlendMode == BlendMode::AlphaTest);

        case ScenePass::Additive:
            return (bucket.BlendMode == BlendMode::Additive);

        default:
            return false;
        }
    }

    void NextRenderer::DrawRooms(ICommandBuffer& commandBuffer, IRenderPass& renderPass, const RenderView& view, ScenePass pass)
    {
        constexpr const char* GROUP_NAMES[] = { "Rooms prepass", "Rooms opaque", "Rooms alpha edges", "Rooms additive" };

        bool additive = (pass == ScenePass::Additive);
        bool isAlphaEdges = (pass == ScenePass::AlphaEdges);
        commandBuffer.PushDebugGroup(GROUP_NAMES[(int)pass]);

        auto shader = (pass == ScenePass::Prepass) ? SceneShader::RoomsPrepass : SceneShader::Rooms;
        renderPass.BindPipeline(GetScenePipeline(shader, additive ? BlendMode::Additive : (isAlphaEdges ? BlendMode::AlphaBlend : BlendMode::Opaque)));

        auto vertexBinding = BufferBinding{ _roomVertexBuffer.get(), 0 };
        renderPass.BindVertexBuffers(0, &vertexBinding, 1);
        renderPass.BindIndexBuffer(BufferBinding{ _roomIndexBuffer.get(), 0 }, IndexSize::Bits32);

        IBuffer* storageBuffers[2] = { _animatedFramesBuffer.get(), _animatedSetsBuffer.get() };
        renderPass.BindVertexStorageBuffers(0, storageBuffers, 2);

        BindScenePixelStorage(renderPass);
        auto state = BucketBindState{};

        // Opaque rooms are drawn front to back (portal traversal order) for early depth rejection,
        // additive rooms back to front.
        int roomCount = (int)view.RoomsToDraw.size();
        for (int roomIndex = 0; roomIndex < roomCount; roomIndex++)
        {
            const auto* room = view.RoomsToDraw[additive ? (roomCount - 1 - roomIndex) : roomIndex];
            auto roomInfo = GetBucketDrawInfo(room->RoomNumber, pass != ScenePass::Prepass);
            roomInfo.IsAlphaEdgePass = isAlphaEdges;

            for (const auto& bucket : room->Buckets)
            {
                if (bucket.NumIndices == 0)
                    continue;

                if (!IsBucketInPass(bucket, pass))
                    continue;

                BindBucket(commandBuffer, renderPass, bucket, _roomTextures, bucket.BlendMode, (unsigned int)_roomLightCount, roomInfo, state);

                _frameStats.DrawCalls++;
                renderPass.DrawIndexed(bucket.NumIndices, 1, bucket.StartIndex, 0, 0);
            }
        }

        commandBuffer.PopDebugGroup();
    }

    // Same conversion as the legacy BindLight: interpolated positions and spot ranges as cosines.
    GpuLight NextRenderer::ToGpuLight(const RendererLight& light, float interpolationFactor, float fade)
    {
        auto gpuLight = GpuLight{};
        gpuLight.Position = light.Position;
        gpuLight.Type = (unsigned int)light.Type;
        gpuLight.Color = light.Color;
        gpuLight.Intensity = light.Intensity * fade;
        gpuLight.Direction = light.Direction;
        gpuLight.In = light.In;
        gpuLight.Out = light.Out;
        gpuLight.InRange = light.InRange;
        gpuLight.OutRange = light.OutRange;

        if (light.Type == LightType::Spot)
        {
            gpuLight.InRange = cos(light.InRange * (PI / 180.0f));
            gpuLight.OutRange = cos(light.OutRange * (PI / 180.0f));
        }

        if (light.Hash != 0)
        {
            gpuLight.Position = Vector3::Lerp(light.PrevPosition, light.Position, interpolationFactor);
            gpuLight.Direction = Vector3::Lerp(light.PrevDirection, light.Direction, interpolationFactor);
        }

        gpuLight.Direction.Normalize();
        return gpuLight;
    }

    // Appends lights to the frame light buffer, with moveable intensity faded between the current and previous room.
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

            auto gpuLight = ToGpuLight(*light, interpolationFactor, fadeCoeff);

            const auto* bytes = (const unsigned char*)&gpuLight;
            _lightData.insert(_lightData.end(), bytes, bytes + sizeof(gpuLight));

            typeMask |= (1 << (31 - (int)light->Type));
            count++;
        }

        return count | typeMask;
    }

    // Builds the frame instance data and the instanced draw list. Statics are grouped by object type, so each
    // mesh bucket of a type is a single draw; rigid moveable meshes get one instance each.
    void NextRenderer::CollectObjects(const RenderView& view, float interpolationFactor)
    {
        _instanceData.clear();
        _lightData.clear();
        _objectDraws.clear();
        _shadowDraws.clear();

        // Rooms share the view light list, as in the legacy renderer.
        int roomLightOffset = 0;
        _roomLightCount = AddLights(view.LightsToDraw, interpolationFactor, NO_VALUE, NO_VALUE, 1.0f, roomLightOffset);

        _boneData.clear();

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
            int firstInstance = (int)(_instanceData.size() / sizeof(GpuObjectInstance));
            for (const auto* staticMesh : statics)
            {
                int lightOffset = 0;
                int lightCount = AddLights(staticMesh->LightsToDraw, interpolationFactor, NO_VALUE, NO_VALUE, 1.0f, lightOffset);
                AddInstance(staticMesh->World, staticMesh->Color, staticMesh->AmbientLight, mesh->LightMode, lightOffset, lightCount, 0, SKINNING_NONE);
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
                // The player has its own path.
                if (item->ObjectID == ID_LARA)
                    continue;

                const auto& object = (*_legacyGeometry.MoveableObjects)[item->ObjectID];
                if (!object.has_value() || object->Hidden)
                    continue;

                const auto& nativeItem = g_Level.Items[item->ItemNumber];

                int lightOffset = 0;
                int lightCount = AddLights(item->LightsToDraw, interpolationFactor, item->RoomNumber, item->PrevRoomNumber, item->LightFade, lightOffset);
                lightCount |= GetShadowReceiverMask(item->ObjectID);

                int firstDraw = (int)_objectDraws.size();

                // Full skin: the skin mesh with bind pose corrected bones, and only the rigid meshes not hidden by it.
                bool isFullSkin = (item->SkinIndex != NO_VALUE);
                if (isFullSkin)
                {
                    Matrix skinBones[BONE_COUNT_MAX];
                    int boneCount = std::min((int)object->AnimationTransforms.size(), (int)BONE_COUNT_MAX);
                    for (int i = 0; i < boneCount; i++)
                        skinBones[i] = object->BindPoseTransforms[i] * item->InterpolatedAnimationTransforms[i];

                    const auto* skinMesh = (*_legacyGeometry.Meshes)[item->SkinIndex];
                    int skinInstance = AddInstance(item->InterpolatedWorld, item->Color, item->AmbientLight, skinMesh->LightMode, lightOffset, lightCount,
                                                   AddBones(skinBones, boneCount), SKINNING_FULL);
                    AddMeshDraws(*skinMesh, skinInstance);
                }

                for (int meshIndex = 0; meshIndex < (int)item->MeshIndex.size(); meshIndex++)
                {
                    if (!nativeItem.MeshBits.Test(meshIndex))
                        continue;

                    if (isFullSkin && g_Level.Meshes[nativeItem.Model.MeshIndex[meshIndex]].hidden)
                        continue;

                    const auto* mesh = (*_legacyGeometry.Meshes)[item->MeshIndex[meshIndex]];
                    auto world = item->InterpolatedAnimationTransforms[meshIndex] * item->InterpolatedWorld;
                    int instance = AddInstance(world, item->Color, item->AmbientLight, mesh->LightMode, lightOffset, lightCount, 0, SKINNING_NONE);
                    AddMeshDraws(*mesh, instance);
                }

                auto waterfallSet = _waterfallSets.find(item->ObjectID);
                if (waterfallSet != _waterfallSets.end())
                {
                    for (int i = firstDraw; i < (int)_objectDraws.size(); i++)
                        _objectDraws[i].AnimatedSetOverride = waterfallSet->second;
                }

                if (IsShadowCaster(item->ObjectID))
                    MarkShadowCasterDraws(firstDraw);
            }
        }

        CollectPlayer(interpolationFactor);
        CollectEffectMeshes(view, interpolationFactor);
    }

    int NextRenderer::AddInstance(const Matrix& world, const Vector4& color, const Vector4& ambientLight, LightMode lightMode, int lightOffset, int lightCount, int boneOffset, int skinningMode)
    {
        auto instance = GpuObjectInstance{};
        instance.World = world;
        instance.Color = color;
        instance.AmbientLight = ambientLight;
        instance.LightMode = (unsigned int)lightMode;
        instance.LightOffset = (unsigned int)lightOffset;
        instance.LightCount = (unsigned int)lightCount;
        instance.Skinning = ((unsigned int)boneOffset & 0xFFFFFF) | ((unsigned int)skinningMode << 24);

        int index = (int)(_instanceData.size() / sizeof(GpuObjectInstance));
        const auto* bytes = (const unsigned char*)&instance;
        _instanceData.insert(_instanceData.end(), bytes, bytes + sizeof(instance));
        return index;
    }

    int NextRenderer::AddBones(const Matrix* bones, int count)
    {
        int offset = (int)(_boneData.size() / sizeof(Matrix));
        const auto* bytes = (const unsigned char*)bones;
        _boneData.insert(_boneData.end(), bytes, bytes + (count * sizeof(Matrix)));
        return offset;
    }

    void NextRenderer::AddMeshDraws(const RendererMesh& mesh, int firstInstance, int instanceCount)
    {
        for (const auto& bucket : mesh.Buckets)
        {
            if (bucket.NumIndices > 0)
                _objectDraws.push_back(ObjectDraw{ MeshSource::Moveables, &bucket, firstInstance, instanceCount });
        }
    }

    // Player meshes are skinned in the vertex shader against the animation bone palette, as in the
    // legacy DrawLara: rigid parts and classic joints blend per-vertex bone indices, the full skin uses
    // bind pose corrected bones. Hair is ported in a later milestone.
    void NextRenderer::CollectPlayer(float interpolationFactor)
    {
        if (LaraItem == nullptr || Lara.Control.Look.OpticRange != 0)
            return;

        const auto& nativeItem = g_Level.Items[LaraItem->Index];
        if (nativeItem.Flags & IFLAG_INVISIBLE)
            return;

        const auto& laraObject = (*_legacyGeometry.MoveableObjects)[ID_LARA];
        const auto* item = _legacyGeometry.Items != nullptr ? &(*_legacyGeometry.Items)[LaraItem->Index] : nullptr;
        if (!laraObject.has_value() || item == nullptr)
            return;

        int lightOffset = 0;
        int lightCount = AddLights(item->LightsToDraw, interpolationFactor, item->RoomNumber, item->PrevRoomNumber, item->LightFade, lightOffset);
        lightCount |= GetShadowReceiverMask(ID_LARA);
        int firstDraw = (int)_objectDraws.size();

        int boneCount = (int)laraObject->AnimationTransforms.size();
        int animationBones = AddBones(item->InterpolatedAnimationTransforms, boneCount);
        auto lightMode = (*_legacyGeometry.Meshes)[item->MeshIndex[0]]->LightMode;

        bool isFullSkin = (item->SkinIndex != NO_VALUE);
        if (isFullSkin)
        {
            Matrix skinBones[BONE_COUNT_MAX];
            for (int i = 0; i < boneCount; i++)
                skinBones[i] = laraObject->BindPoseTransforms[i] * item->InterpolatedAnimationTransforms[i];

            int skinBoneOffset = AddBones(skinBones, boneCount);
            int skinInstance = AddInstance(item->InterpolatedWorld, item->Color, item->AmbientLight, lightMode, lightOffset, lightCount, skinBoneOffset, SKINNING_FULL);
            AddMeshDraws(*(*_legacyGeometry.Meshes)[item->SkinIndex], skinInstance);
        }

        int instance = AddInstance(item->InterpolatedWorld, item->Color, item->AmbientLight, lightMode, lightOffset, lightCount, animationBones, SKINNING_CLASSIC);

        for (int meshIndex = 0; meshIndex < (int)item->MeshIndex.size(); meshIndex++)
        {
            if (!nativeItem.MeshBits.Test(meshIndex))
                continue;

            if (isFullSkin && g_Level.Meshes[nativeItem.Model.MeshIndex[meshIndex]].hidden)
                continue;

            AddMeshDraws(*(*_legacyGeometry.Meshes)[item->MeshIndex[meshIndex]], instance);
        }

        if (!isFullSkin)
        {
            const auto& joints = (*_legacyGeometry.MoveableObjects)[Lara.Skin.SkinJoints];
            if (joints.has_value())
            {
                for (int meshIndex = 1; meshIndex < (int)joints->ObjectMeshes.size(); meshIndex++)
                    AddMeshDraws(*joints->ObjectMeshes[meshIndex], instance);
            }
        }

        // Left and right holsters always draw their slot object (the empty slot is the empty holster mesh),
        // the back holster only when occupied, as in the legacy DrawLaraHolsters.
        auto addHolster = [&](HolsterSlot slot, int boneIndex, bool skipEmpty)
        {
            if (skipEmpty && slot == HolsterSlot::Empty)
                return;

            const auto& holster = (*_legacyGeometry.MoveableObjects)[(int)slot];
            if (holster.has_value() && boneIndex < (int)holster->ObjectMeshes.size())
                AddMeshDraws(*holster->ObjectMeshes[boneIndex], instance);
        };

        addHolster(Lara.Control.Weapon.HolsterInfo.LeftHolster, LM_LTHIGH, false);
        addHolster(Lara.Control.Weapon.HolsterInfo.RightHolster, LM_RTHIGH, false);
        addHolster(Lara.Control.Weapon.HolsterInfo.BackHolster, LM_TORSO, true);

        CollectPlayerHair(*item, item->AmbientLight, lightOffset, lightCount, interpolationFactor);

        if (IsShadowCaster(ID_LARA))
            MarkShadowCasterDraws(firstDraw);
    }

    // Hair units use their own bone palette: the head attachment followed by one matrix per simulated
    // segment, as in the legacy DrawLaraHair. The legacy renderer updates segment interpolation data while
    // drawing, so the matrices are only read here.
    void NextRenderer::CollectPlayerHair(const RendererItem& item, const Vector4& ambientLight, int lightOffset, int lightCount, float interpolationFactor)
    {
        using namespace TEN::Effects::Hair;

        for (int unitIndex = 0; unitIndex < (int)HairEffect.Units.size(); unitIndex++)
        {
            const auto& unit = HairEffect.Units[unitIndex];
            if (!unit.IsEnabled || !Objects[unit.ObjectID].loaded)
                continue;

            const auto& hairObject = (*_legacyGeometry.MoveableObjects)[unit.ObjectID];
            if (!hairObject.has_value())
                continue;

            bool isSkinned = (Objects[unit.ObjectID].skinIndex != NO_VALUE && g_GameFlow->GetSettings()->Graphics.Skinning);
            bool isFlipped = isSkinned || GetJointOffset(unit.ObjectID, 1, false).z < 0.0f;

            Matrix bones[BONE_COUNT_MAX];
            int boneCount = 1;
            bones[0] = item.InterpolatedAnimationTransforms[HairUnit::GetRootMeshID(unitIndex)] * item.InterpolatedWorld;

            for (const auto& segment : unit.Segments)
            {
                if (boneCount >= BONE_COUNT_MAX)
                    break;

                auto world = Matrix::CreateFromQuaternion(Quaternion::Lerp(segment.PrevOrientation, segment.Orientation, interpolationFactor)) *
                             Matrix::CreateTranslation(Vector3::Lerp(segment.PrevPosition, segment.Position, interpolationFactor));

                if (isFlipped)
                    world = Matrix::CreateRotationY(PI) * world;

                bones[boneCount++] = world;
            }

            int boneOffset = AddBones(bones, boneCount);
            int instance = AddInstance(Matrix::Identity, item.Color, ambientLight, LightMode::Dynamic, lightOffset, lightCount, boneOffset,
                isSkinned ? SKINNING_FULL : SKINNING_RIGID);

            if (isSkinned)
            {
                AddMeshDraws(*(*_legacyGeometry.Meshes)[Objects[unit.ObjectID].skinIndex], instance);
            }
            else
            {
                for (const auto* mesh : hairObject->ObjectMeshes)
                    AddMeshDraws(*mesh, instance);
            }
        }
    }

    void NextRenderer::UploadFrameStorage(ICopyPass& copyPass, FrameStorageBuffer& storage, const std::vector<unsigned char>& data, int stride, unsigned int usage, const char* name)
    {
        // Storage buffers cannot be empty, so the smallest buffer holds one element.
        int dataSize = std::max((int)data.size(), stride);
        if (dataSize > storage.Capacity || storage.Buffer == nullptr)
        {
            storage.Capacity = std::max(dataSize * 2, stride * INITIAL_INSTANCE_CAPACITY);
            storage.Buffer = _device->CreateBuffer(BufferDesc{ storage.Capacity, stride, usage, name });
            storage.Transfer = _device->CreateTransferBuffer(TransferBufferUsage::Upload, storage.Capacity);
        }

        if (data.empty())
            return;

        // Cycling renames buffers still in use by previous frames instead of stalling.
        memcpy(_device->MapTransferBuffer(storage.Transfer.get(), true), data.data(), data.size());
        _device->UnmapTransferBuffer(storage.Transfer.get());

        copyPass.UploadToBuffer(storage.Transfer.get(), 0, storage.Buffer.get(), 0, (int)data.size(), true);
    }

    // All per-frame data goes to the GPU in a single copy pass before the scene pass.
    void NextRenderer::UploadFrameData(ICommandBuffer& commandBuffer)
    {
        commandBuffer.PushDebugGroup("Upload frame data");
        auto* copyPass = commandBuffer.BeginCopyPass();

        UploadFrameStorage(*copyPass, _instanceBuffer, _instanceData, (int)sizeof(GpuObjectInstance), BufferUsage::GraphicsStorageRead, "Frame.Instances");
        UploadFrameStorage(*copyPass, _lightBuffer, _lightData, (int)sizeof(GpuLight), BufferUsage::GraphicsStorageRead, "Frame.Lights");
        UploadFrameStorage(*copyPass, _boneBuffer, _boneData, (int)sizeof(Matrix), BufferUsage::GraphicsStorageRead, "Frame.Bones");
        UploadFrameStorage(*copyPass, _sortedIndexBuffer, _sortedIndexData, (int)sizeof(int), BufferUsage::Index, "Frame.SortedIndices");
        UploadFrameStorage(*copyPass, _spriteBuffer, _spriteData, (int)sizeof(GpuSprite), BufferUsage::GraphicsStorageRead, "Frame.Sprites");
        UploadFrameStorage(*copyPass, _materialBuffer, _materialData, (int)sizeof(GpuMaterial), BufferUsage::GraphicsStorageRead, "Frame.Materials");
        UploadFrameStorage(*copyPass, _decalBuffer, _decalData, (int)sizeof(GpuDecal), BufferUsage::GraphicsStorageRead, "Frame.Decals");

        commandBuffer.EndCopyPass();
        commandBuffer.PopDebugGroup();
    }

    void NextRenderer::DrawObjects(ICommandBuffer& commandBuffer, IRenderPass& renderPass, ScenePass pass)
    {
        constexpr const char* GROUP_NAMES[] = { "Objects prepass", "Objects opaque", "Objects alpha edges", "Objects additive" };

        if (_objectDraws.empty() || _instanceBuffer.Buffer == nullptr)
            return;

        bool additive = (pass == ScenePass::Additive);
        bool isAlphaEdges = (pass == ScenePass::AlphaEdges);
        commandBuffer.PushDebugGroup(GROUP_NAMES[(int)pass]);

        auto shader = (pass == ScenePass::Prepass) ? SceneShader::ObjectsPrepass : SceneShader::Objects;
        renderPass.BindPipeline(GetScenePipeline(shader, additive ? BlendMode::Additive : (isAlphaEdges ? BlendMode::AlphaBlend : BlendMode::Opaque)));

        IBuffer* storageBuffers[4] = { _animatedFramesBuffer.get(), _animatedSetsBuffer.get(), _instanceBuffer.Buffer.get(), _boneBuffer.Buffer.get() };
        renderPass.BindVertexStorageBuffers(0, storageBuffers, 4);

        BindScenePixelStorage(renderPass);

        auto lastSource = (MeshSource)-1;
        auto state = BucketBindState{};

        for (const auto& draw : _objectDraws)
        {
            const auto& bucket = *draw.Bucket;
            if (!IsBucketInPass(bucket, pass))
                continue;

            if (draw.Source != lastSource)
            {
                const auto& geometry = (draw.Source == MeshSource::Statics) ? _staticGeometry : _moveableGeometry;
                BufferBinding vertexBindings[2] = { { geometry.Vertices.get(), 0 }, { _instanceIndexBuffer.get(), 0 } };
                renderPass.BindVertexBuffers(0, vertexBindings, 2);
                renderPass.BindIndexBuffer(BufferBinding{ geometry.Indices.get(), 0 }, IndexSize::Bits32);
                lastSource = draw.Source;
            }

            const auto& pages = (draw.Source == MeshSource::Statics) ? _staticTextures : _moveableTextures;
            auto drawInfo = BucketDrawInfo{};
            drawInfo.AnimatedSetOverride = draw.AnimatedSetOverride;
            drawInfo.IsAlphaEdgePass = isAlphaEdges;
            BindBucket(commandBuffer, renderPass, bucket, pages, bucket.BlendMode, 0, drawInfo, state);

            _frameStats.DrawCalls++;
            renderPass.DrawIndexed(bucket.NumIndices, draw.InstanceCount, bucket.StartIndex, 0, draw.FirstInstance);
        }

        commandBuffer.PopDebugGroup();
    }

    // Instance of a sorted object, created once per frame even when interleaved clusters split the object
    // into several runs. Mirrors the per-type state of the legacy Draw*Sorted functions.
    int NextRenderer::GetSortedObjectInstance(const RendererSortableObject& object, float interpolationFactor)
    {
        const void* key = (object.ObjectType == RendererObjectType::Static) ? (const void*)object.Static :
                          (object.ObjectType == RendererObjectType::Effect) ? (const void*)object.Effect :
                          (object.ObjectType == RendererObjectType::Moveable) ? (const void*)object.Item : (const void*)&object;

        auto it = _sortedInstances.find(key);
        if (it != _sortedInstances.end())
            return it->second;

        int instance = 0;
        int lightOffset = 0;
        int lightCount = 0;

        switch (object.ObjectType)
        {
        case RendererObjectType::Moveable:
        {
            const auto* item = object.Item;
            const auto& moveableObject = (*_legacyGeometry.MoveableObjects)[item->ObjectID];
            int boneCount = moveableObject.has_value() ? (int)moveableObject->AnimationTransforms.size() : 0;

            int boneOffset = 0;
            if (object.Skinned && moveableObject.has_value())
            {
                Matrix skinBones[BONE_COUNT_MAX];
                for (int i = 0; i < boneCount; i++)
                    skinBones[i] = moveableObject->BindPoseTransforms[i] * item->InterpolatedAnimationTransforms[i];

                boneOffset = AddBones(skinBones, boneCount);
            }
            else
            {
                boneOffset = AddBones(item->InterpolatedAnimationTransforms, BONE_COUNT_MAX);
            }

            lightCount = AddLights(item->LightsToDraw, interpolationFactor, item->RoomNumber, item->PrevRoomNumber, item->LightFade, lightOffset);
            instance = AddInstance(item->InterpolatedWorld, item->Color, item->AmbientLight, object.LightMode, lightOffset, lightCount, boneOffset,
                object.Skinned ? SKINNING_FULL : SKINNING_RIGID);
            break;
        }

        case RendererObjectType::Static:
            lightCount = AddLights(object.Static->LightsToDraw, interpolationFactor, NO_VALUE, NO_VALUE, 1.0f, lightOffset);
            instance = AddInstance(object.Static->World, object.Static->Color, object.Room->AmbientLight, object.LightMode, lightOffset, lightCount, 0, SKINNING_NONE);
            break;

        case RendererObjectType::MoveableAsStatic:
            lightCount = AddLights(object.Room->LightsToDraw, interpolationFactor, NO_VALUE, NO_VALUE, 1.0f, lightOffset);
            instance = AddInstance(object.World, Vector4::One, object.Room->AmbientLight, object.LightMode, lightOffset, lightCount, 0, SKINNING_NONE);
            break;

        case RendererObjectType::Effect:
            lightCount = AddLights(object.Effect->LightsToDraw, interpolationFactor, NO_VALUE, NO_VALUE, 1.0f, lightOffset);
            instance = AddInstance(object.Effect->InterpolatedWorld, object.Effect->Color, object.Effect->AmbientLight, LightMode::Dynamic, lightOffset, lightCount, 0, SKINNING_NONE);
            break;

        default:
            break;
        }

        _sortedInstances[key] = instance;
        return instance;
    }

    // Walks the transparent polygons in the order computed by the legacy SortTransparentFaces and turns each
    // group run into one draw. All indices go into a single frame index buffer, so a run costs one draw call
    // and object state comes from the instance buffer instead of per-run constant buffer uploads.
    void NextRenderer::CollectSortedFaces(const RenderView& view, float interpolationFactor)
    {
        _sortedIndexData.clear();
        _sortedDraws.clear();
        _sortedInstances.clear();

        int sortedCount = (int)view.TransparentSortKeys.size();
        const auto* sortKeys = view.TransparentSortKeys.data();
        const auto* polygons = view.TransparentPolygonsToDraw.data();
        const auto* objects = view.TransparentObjectsToDraw.data();

        int i = 0;
        while (i < sortedCount)
        {
            const auto& object = objects[polygons[sortKeys[i].Index].ObjectIndex];

            if (object.ObjectType == RendererObjectType::Sprite)
            {
                auto draw = SortedDraw{};
                draw.IsSprite = true;
                draw.Sprites.Texture = GetSpriteTexture(object.Sprite->Sprite);
                draw.Sprites.BlendMode = object.Sprite->BlendMode;
                draw.Sprites.FirstSprite = (int)(_spriteData.size() / sizeof(GpuSprite));

                while (i < sortedCount && objects[polygons[sortKeys[i].Index].ObjectIndex].GroupIndex == object.GroupIndex)
                {
                    if (draw.Sprites.Texture != nullptr && AddSprite(*objects[polygons[sortKeys[i].Index].ObjectIndex].Sprite, view))
                        draw.Sprites.SpriteCount++;

                    i++;
                }

                if (draw.Sprites.SpriteCount > 0)
                    _sortedDraws.push_back(draw);

                continue;
            }

            const std::vector<int>* sourceIndices = nullptr;
            auto source = MeshSource::Moveables;
            switch (object.ObjectType)
            {
            case RendererObjectType::Room:
                sourceIndices = _legacyGeometry.RoomIndices;
                break;

            case RendererObjectType::Static:
                sourceIndices = _legacyGeometry.StaticIndices;
                source = MeshSource::Statics;
                break;

            case RendererObjectType::Moveable:
            case RendererObjectType::MoveableAsStatic:
            case RendererObjectType::Effect:
                sourceIndices = _legacyGeometry.MoveableIndices;
                break;

            default:
                break;
            }

            // Sprites and hair are ported in a later milestone; skip their whole run.
            if (sourceIndices == nullptr)
            {
                int groupIndex = object.GroupIndex;
                while (i < sortedCount && objects[polygons[sortKeys[i].Index].ObjectIndex].GroupIndex == groupIndex)
                    i++;

                continue;
            }

            int firstIndex = (int)(_sortedIndexData.size() / sizeof(int));
            while (i < sortedCount)
            {
                const auto& polygon = polygons[sortKeys[i].Index];
                if (objects[polygon.ObjectIndex].GroupIndex != object.GroupIndex)
                    break;

                int indexCount = (polygon.Polygon->Shape == 0) ? 6 : 3;
                const auto* bytes = (const unsigned char*)(sourceIndices->data() + polygon.Polygon->BaseIndex);
                _sortedIndexData.insert(_sortedIndexData.end(), bytes, bytes + (indexCount * sizeof(int)));
                i++;
            }

            auto draw = SortedDraw{};
            draw.Source = source;
            draw.IsRoom = (object.ObjectType == RendererObjectType::Room);
            draw.IsWaterRoom = draw.IsRoom && (g_Level.Rooms[object.Room->RoomNumber].flags & ENV_FLAG_WATER);
            draw.IsCausticsRoom = draw.IsRoom && IsCausticsRoom(object.Room->RoomNumber);
            draw.Bucket = object.Bucket;
            draw.BlendMode = object.BlendMode;
            draw.FirstIndex = firstIndex;
            draw.IndexCount = (int)(_sortedIndexData.size() / sizeof(int)) - firstIndex;
            draw.Instance = draw.IsRoom ? 0 : GetSortedObjectInstance(object, interpolationFactor);
            _sortedDraws.push_back(draw);
        }
    }

    void NextRenderer::DrawSortedFaces(ICommandBuffer& commandBuffer, IRenderPass& renderPass)
    {
        if (_sortedDraws.empty())
            return;

        commandBuffer.PushDebugGroup("Sorted transparent faces");

        if (!_sortedIndexData.empty())
            renderPass.BindIndexBuffer(BufferBinding{ _sortedIndexBuffer.Buffer.get(), 0 }, IndexSize::Bits32);

        IBuffer* objectStorageBuffers[4] = { _animatedFramesBuffer.get(), _animatedSetsBuffer.get(), _instanceBuffer.Buffer.get(), _boneBuffer.Buffer.get() };

        const IGraphicsPipeline* lastPipeline = nullptr;
        const ITexture* lastSpriteTexture = nullptr;
        // Vertex source of the last bound geometry: a mesh source, rooms, or nothing after a pipeline change.
        constexpr auto ROOM_SOURCE    = (int)MeshSource::Moveables + 1;
        constexpr auto UNBOUND_SOURCE = INT_MIN;

        int lastSource = UNBOUND_SOURCE;
        auto state = BucketBindState{};

        for (const auto& draw : _sortedDraws)
        {
            // Sprite runs overwrite the sampler and pixel uniform slots used by bucket draws.
            if (draw.IsSprite)
            {
                DrawSpriteRun(commandBuffer, renderPass, draw.Sprites, lastPipeline, lastSpriteTexture);
                lastSource = UNBOUND_SOURCE;
                state = BucketBindState{};
                continue;
            }

            const auto& bucket = *draw.Bucket;
            auto shader = draw.IsRoom ? SceneShader::Rooms : SceneShader::Objects;

            auto* pipeline = GetScenePipeline(shader, draw.BlendMode);
            if (pipeline != lastPipeline)
            {
                renderPass.BindPipeline(pipeline);
                renderPass.BindVertexStorageBuffers(0, objectStorageBuffers, draw.IsRoom ? 2 : 4);
                BindScenePixelStorage(renderPass);
                lastPipeline = pipeline;
                lastSource = UNBOUND_SOURCE;
                lastSpriteTexture = nullptr;
                state = BucketBindState{};
            }

            int source = draw.IsRoom ? ROOM_SOURCE : (int)draw.Source;
            if (source != lastSource)
            {
                auto* vertices = draw.IsRoom ? _roomVertexBuffer.get() :
                    ((draw.Source == MeshSource::Statics) ? _staticGeometry.Vertices.get() : _moveableGeometry.Vertices.get());

                BufferBinding vertexBindings[2] = { { vertices, 0 }, { _instanceIndexBuffer.get(), 0 } };
                renderPass.BindVertexBuffers(0, vertexBindings, draw.IsRoom ? 1 : 2);
                lastSource = source;
            }

            const auto& pages = draw.IsRoom ? _roomTextures : ((draw.Source == MeshSource::Statics) ? _staticTextures : _moveableTextures);
            // Decals are not drawn on sorted faces, as in the legacy renderer.
            auto roomInfo = BucketDrawInfo{ draw.IsWaterRoom ? 1u : 0u, draw.IsCausticsRoom ? 1u : 0u };
            BindBucket(commandBuffer, renderPass, bucket, pages, draw.BlendMode, draw.IsRoom ? (unsigned int)_roomLightCount : 0, roomInfo, state);

            _frameStats.DrawCalls++;
            renderPass.DrawIndexed(draw.IndexCount, 1, draw.FirstIndex, 0, draw.Instance);
        }

        commandBuffer.PopDebugGroup();
    }

    // Same grid as the legacy InitializeSky: 20x20 tiles of 512 units in the XZ plane.
    bool NextRenderer::CreateSkyGeometry()
    {
        auto vertices = std::vector<Vertex>{};
        auto indices = std::vector<int>{};
        vertices.reserve(SKY_VERTICES_COUNT);
        indices.reserve(SKY_INDICES_COUNT);

        auto addVertex = [&](int x, int z)
        {
            auto vertex = Vertex{};
            vertex.Position = Vector3(-SKY_SIZE / 2.0f + x * 512.0f, 0.0f, -SKY_SIZE / 2.0f + z * 512.0f);
            vertex.UV = Vector2(x / 20.0f, z / 20.0f);
            vertex.Color = 0xFFFFFFFF;
            vertices.push_back(vertex);
        };

        for (int x = 0; x < SKY_TILES_COUNT; x++)
        {
            for (int z = 0; z < SKY_TILES_COUNT; z++)
            {
                int base = (int)vertices.size();
                addVertex(x, z + 1);
                addVertex(x + 1, z + 1);
                addVertex(x + 1, z);
                addVertex(x, z);

                int quad[6] = { base, base + 1, base + 2, base, base + 2, base + 3 };
                indices.insert(indices.end(), quad, quad + 6);
            }
        }

        int vertexSize = (int)(vertices.size() * sizeof(Vertex));
        int indexSize = (int)(indices.size() * sizeof(int));
        _skyVertexBuffer = _device->CreateBuffer(BufferDesc{ vertexSize, 0, BufferUsage::Vertex, "Sky.Vertices" });
        _skyIndexBuffer = _device->CreateBuffer(BufferDesc{ indexSize, 0, BufferUsage::Index, "Sky.Indices" });
        if (_skyVertexBuffer == nullptr || _skyIndexBuffer == nullptr)
            return false;

        UploadBuffer(_skyVertexBuffer.get(), vertices.data(), vertexSize);
        UploadBuffer(_skyIndexBuffer.get(), indices.data(), indexSize);
        return true;
    }

    // Classic sky: two scrolling layers (additive) and up to two horizon meshes (alpha blended) around the
    // camera, drawn without depth before the rooms, as in the legacy DrawHorizonAndSky.
    void NextRenderer::DrawSky(ICommandBuffer& commandBuffer, IRenderPass& renderPass, const RenderView& view, float interpolationFactor)
    {
        using namespace TEN::Effects::Environment;

        bool isAnyRoomOutside = false;
        for (const auto* room : view.RoomsToDraw)
        {
            if (g_Level.Rooms[room->RoomNumber].flags & ENV_FLAG_SKYBOX)
            {
                isAnyRoomOutside = true;
                break;
            }
        }

        if (!isAnyRoomOutside)
            return;

        commandBuffer.PushDebugGroup("Sky");

        IBuffer* storageBuffers[2] = { _animatedFramesBuffer.get(), _animatedSetsBuffer.get() };
        auto drawUniforms = DrawVertexUniforms{ NO_VALUE };
        commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 1, &drawUniforms, sizeof(drawUniforms));

        // Layers.
        renderPass.BindPipeline(GetScenePipeline(SceneShader::Sky, BlendMode::Additive));
        renderPass.BindVertexStorageBuffers(0, storageBuffers, 2);

        auto skyVertexBinding = BufferBinding{ _skyVertexBuffer.get(), 0 };
        renderPass.BindVertexBuffers(0, &skyVertexBinding, 1);
        renderPass.BindIndexBuffer(BufferBinding{ _skyIndexBuffer.get(), 0 }, IndexSize::Bits32);

        auto skyTextureBinding = TextureSamplerBinding{ (_skyTexture != nullptr) ? _skyTexture.get() : _whiteTexture.get(), _anisotropicSampler.get() };
        renderPass.BindPixelSamplers(0, &skyTextureBinding, 1);

        auto rotation = Matrix::CreateRotationX(PI);
        for (int layer = 0; layer < 2; layer++)
        {
            auto color = Weather.SkyColor(layer);
            if (Vector3(color) == Vector3::Zero)
                continue;

            commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &color, sizeof(color));

            for (int tile = 0; tile < 2; tile++)
            {
                auto translation = Matrix::CreateTranslation(
                    view.Camera.WorldPosition.x + Weather.SkyPosition(layer) - tile * SKY_SIZE,
                    view.Camera.WorldPosition.y - 1536.0f,
                    view.Camera.WorldPosition.z);

                auto world = rotation * translation;
                // Fog bulbs affect the first layer only, as in the legacy renderer.
                auto skyUniforms = SkyVertexUniforms{ world, (layer == 0) ? 1u : 0u };
                commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 2, &skyUniforms, sizeof(skyUniforms));
                _frameStats.DrawCalls++;
                renderPass.DrawIndexed(SKY_INDICES_COUNT, 1, 0, 0, 0);
            }
        }

        // Horizon meshes.
        auto* level = g_GameFlow->GetLevel(CurrentLevel);
        for (int layer = 0; layer < 2; layer++)
        {
            float alpha = level->GetHorizonTransparency(layer);
            if (!level->GetHorizonEnabled(layer) || alpha <= EPSILON)
                continue;

            const auto& horizonObject = (*_legacyGeometry.MoveableObjects)[level->GetHorizonObjectID(layer)];
            if (!horizonObject.has_value())
                continue;

            auto position = Vector3::Lerp(level->GetHorizonPrevPosition(layer), level->GetHorizonPosition(layer), interpolationFactor);
            auto orientation = EulerAngles::Lerp(level->GetHorizonPrevOrientation(layer), level->GetHorizonOrientation(layer), interpolationFactor);
            auto world = orientation.ToRotationMatrix() * Matrix::CreateTranslation(position) * Matrix::CreateTranslation(view.Camera.WorldPosition);
            auto color = Vector4(1.0f, 1.0f, 1.0f, alpha);

            auto skyUniforms = SkyVertexUniforms{ world, 1u };
            commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 2, &skyUniforms, sizeof(skyUniforms));
            commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &color, sizeof(color));

            auto moveableVertexBinding = BufferBinding{ _moveableGeometry.Vertices.get(), 0 };
            renderPass.BindVertexBuffers(0, &moveableVertexBinding, 1);
            renderPass.BindIndexBuffer(BufferBinding{ _moveableGeometry.Indices.get(), 0 }, IndexSize::Bits32);

            int lastAnimatedSet = INT_MIN;
            for (const auto* mesh : horizonObject->ObjectMeshes)
            {
                for (const auto& bucket : mesh->Buckets)
                {
                    if (bucket.NumIndices == 0)
                        continue;

                    // Horizons always blend; alpha test buckets blend as alpha.
                    auto blendMode = (bucket.BlendMode == BlendMode::AlphaTest || bucket.BlendMode == BlendMode::Opaque) ? BlendMode::AlphaBlend : bucket.BlendMode;
                    renderPass.BindPipeline(GetScenePipeline(SceneShader::Sky, blendMode));
                    renderPass.BindVertexStorageBuffers(0, storageBuffers, 2);

                    int animatedSet = bucket.Animated ? bucket.Texture : NO_VALUE;
                    if (animatedSet != lastAnimatedSet)
                    {
                        auto vertexUniforms = DrawVertexUniforms{ animatedSet };
                        commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 1, &vertexUniforms, sizeof(vertexUniforms));
                        lastAnimatedSet = animatedSet;
                    }

                    const auto& textures = bucket.Animated ? _animatedTextures : _moveableTextures;
                    auto* texture = (bucket.Texture >= 0 && bucket.Texture < (int)textures.size() && textures[bucket.Texture].ColorMap != nullptr) ?
                        textures[bucket.Texture].ColorMap.get() : _whiteTexture.get();

                    auto binding = TextureSamplerBinding{ texture, _anisotropicSampler.get() };
                    renderPass.BindPixelSamplers(0, &binding, 1);
                    _frameStats.DrawCalls++;
                    renderPass.DrawIndexed(bucket.NumIndices, 1, bucket.StartIndex, 0, 0);
                }
            }
        }

        commandBuffer.PopDebugGroup();
    }

    void NextRenderer::LogGpuTimings()
    {
        if (_device == nullptr)
            return;

        auto timings = _device->ConsumeGpuTimings();
        if (timings.empty())
            return;

        auto message = std::string("Next renderer GPU time per pass:");
        for (const auto& timing : timings)
            message += fmt::format(" {} {:.3f} ms,", timing.Name, timing.Milliseconds);

        message.back() = '.';
        TENLog(message + fmt::format(" ({} frames)", timings.front().Samples), LogLevel::Info);
    }

    bool NextRenderer::IsCausticsRoom(int roomNumber) const
    {
        int flags = g_Level.Rooms[roomNumber].flags;
        return (g_Configuration.EnableCaustics && (flags & ENV_FLAG_WATER) && !(flags & ENV_FLAG_NOCAUSTICS));
    }

    // Current frame of the caustics sprite sequence, as in the legacy room pass.
    void NextRenderer::PrepareCaustics(FramePixelUniforms& uniforms)
    {
        _causticsTexture = nullptr;

        const auto& object = Objects[ID_CAUSTIC_TEXTURES];
        if (!object.loaded || object.nmeshes >= 0 || _legacyGeometry.Sprites == nullptr)
            return;

        int spriteIndex = object.meshIndex + (GlobalCounter % -object.nmeshes);
        if (spriteIndex < 0 || spriteIndex >= (int)_legacyGeometry.Sprites->size())
            return;

        const auto& sprite = (*_legacyGeometry.Sprites)[spriteIndex];
        _causticsTexture = GetSpriteTexture(&sprite);
        if (_causticsTexture == nullptr)
            return;

        const auto& pageDesc = _causticsTexture->GetDesc();
        uniforms.CausticsStartUV = sprite.UV[0];
        uniforms.CausticsSize = Vector2((float)sprite.Width / pageDesc.Width, (float)sprite.Height / pageDesc.Height);
    }

    // Same values as the legacy camera constant buffer.
    FogBulbUniforms NextRenderer::GetFogBulbUniforms(const RenderView& view)
    {
        auto uniforms = FogBulbUniforms{};
        uniforms.Count = std::min((int)view.FogBulbsToDraw.size(), FOG_BULBS_MAX);

        for (int i = 0; i < uniforms.Count; i++)
        {
            const auto& bulb = view.FogBulbsToDraw[i];
            auto& gpuBulb = uniforms.FogBulbs[i];
            gpuBulb.Position = bulb.Position;
            gpuBulb.Density = bulb.Density * (CLICK(1) / bulb.Radius);
            gpuBulb.SquaredRadius = SQUARE(bulb.Radius);
            gpuBulb.Color = bulb.Color;
            gpuBulb.SquaredCameraToFogBulbDistance = SQUARE(bulb.Distance);
            gpuBulb.FogBulbToCameraVector = bulb.FogBulbToCameraVector;
        }

        return uniforms;
    }

    const NextFrameStats& NextRenderer::GetFrameStats() const
    {
        return _frameStats;
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

        auto startTime = std::chrono::steady_clock::now();
        _frameStats.DrawCalls = 0;

        auto* commandBuffer = _device->AcquireCommandBuffer();
        if (commandBuffer == nullptr)
            return;

        commandBuffer->PushDebugGroup("Frame");

        // Projection is rebuilt for the preview window aspect ratio; position and orientation follow the game camera.
        auto projection = Matrix::CreatePerspectiveFieldOfView(view.Camera.FOV, (float)width / (float)height, view.Camera.NearPlane, view.Camera.FarPlane);
        _isAmbientOcclusionActive = environment.IsAmbientOcclusionEnabled;
        _isShadowMapActive = environment.IsShadowMapEnabled && (environment.ShadowLight != nullptr);
        _isPlayerShadowOnly = environment.IsPlayerShadowOnly;

        auto frameVertexUniforms = FrameVertexUniforms{};
        frameVertexUniforms.ViewProjection = view.Camera.View * projection;
        frameVertexUniforms.CameraPosition = environment.CameraPosition;
        frameVertexUniforms.FogMinDistance = environment.FogMinDistance;
        frameVertexUniforms.FogMaxDistance = environment.FogMaxDistance;
        frameVertexUniforms.Frame = environment.Frame;
        frameVertexUniforms.InterpolatedFrame = environment.InterpolatedFrame;
        frameVertexUniforms.CameraUnderwater = environment.IsCameraUnderwater ? 1 : 0;
        commandBuffer->PushUniforms(RHI::ShaderStage::Vertex, 0, &frameVertexUniforms, sizeof(frameVertexUniforms));

        auto framePixelUniforms = FramePixelUniforms{};
        framePixelUniforms.FogColor = environment.FogColor;
        framePixelUniforms.CameraPosition = environment.CameraPosition;
        framePixelUniforms.CameraDirection = view.Camera.WorldDirection;
        framePixelUniforms.AmbientOcclusionExponent = GetAmbientOcclusionExponent();
        framePixelUniforms.View = view.Camera.View;
        framePixelUniforms.InverseViewportSize = Vector2(1.0f / width, 1.0f / height);
        PrepareCaustics(framePixelUniforms);
        commandBuffer->PushUniforms(RHI::ShaderStage::Pixel, 0, &framePixelUniforms, sizeof(framePixelUniforms));

        auto fogBulbUniforms = GetFogBulbUniforms(view);
        commandBuffer->PushUniforms(RHI::ShaderStage::Vertex, 3, &fogBulbUniforms, sizeof(fogBulbUniforms));

        CollectMaterials(environment.InterpolationFactor);
        CollectDecals(view);
        CollectObjects(view, environment.InterpolationFactor);
        CollectSprites(view);
        CollectSortedFaces(view, environment.InterpolationFactor);

        UploadFrameData(*commandBuffer);

        PrepareShadows(environment, environment.InterpolationFactor);
        RenderShadowMap(*commandBuffer, frameVertexUniforms);

        if (_isAmbientOcclusionActive)
        {
            RenderDepthPrepass(*commandBuffer, view);
            RenderSSAO(*commandBuffer, projection);
        }

        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = _sceneTarget.get();
        colorTarget.ClearColor[0] = environment.FogColor.x;
        colorTarget.ClearColor[1] = environment.FogColor.y;
        colorTarget.ClearColor[2] = environment.FogColor.z;
        colorTarget.ClearColor[3] = 1.0f;

        auto depthTarget = DepthTargetInfo{};
        depthTarget.Texture = _depthTarget.get();
        depthTarget.Load = _isAmbientOcclusionActive ? LoadOp::Load : LoadOp::Clear;
        depthTarget.Store = StoreOp::DontCare;

        // Slot 2 is shared with the screen space passes, so the shadow uniforms are pushed right before the scene.
        commandBuffer->PushUniforms(RHI::ShaderStage::Pixel, 2, &_shadowUniforms, sizeof(_shadowUniforms));

        commandBuffer->PushDebugGroup("Scene");
        auto* renderPass = commandBuffer->BeginRenderPass(&colorTarget, 1, &depthTarget);
        DrawSky(*commandBuffer, *renderPass, view, environment.InterpolationFactor);
        DrawRooms(*commandBuffer, *renderPass, view, ScenePass::Opaque);
        DrawObjects(*commandBuffer, *renderPass, ScenePass::Opaque);
        DrawRooms(*commandBuffer, *renderPass, view, ScenePass::AlphaEdges);
        DrawObjects(*commandBuffer, *renderPass, ScenePass::AlphaEdges);
        DrawRooms(*commandBuffer, *renderPass, view, ScenePass::Additive);
        DrawObjects(*commandBuffer, *renderPass, ScenePass::Additive);
        DrawSprites(*commandBuffer, *renderPass);
        DrawSortedFaces(*commandBuffer, *renderPass);
        commandBuffer->EndRenderPass();
        commandBuffer->PopDebugGroup();

        RenderPostProcess(*commandBuffer, environment);

        if (!_device->RequiresWindowThreadPresent())
        {
            auto* swapchainTexture = commandBuffer->AcquireSwapchainTexture();
            if (swapchainTexture != nullptr)
                commandBuffer->BlitTexture(_colorTarget.get(), swapchainTexture);
        }

        commandBuffer->PopDebugGroup();

        // Submit is excluded: on DX11 it also presents and may wait for vsync.
        _frameStats.CpuTime = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - startTime).count();
        commandBuffer->Submit();

        if (!_screenshotPath.empty())
        {
            TENLog(fmt::format("Next renderer environment: fog color ({:.2f}, {:.2f}, {:.2f}), fog {:.1f}-{:.1f}, frame {}.",
                environment.FogColor.x, environment.FogColor.y, environment.FogColor.z, environment.FogMinDistance, environment.FogMaxDistance, environment.Frame), LogLevel::Info);

            bool saved = SaveTextureToPng(*_device, _colorTarget.get(), _screenshotPath);
            TENLog("Next renderer screenshot " + _screenshotPath + (saved ? " saved." : " failed."), LogLevel::Info);

            if (_isAmbientOcclusionActive)
            {
                auto occlusionPath = _screenshotPath.substr(0, _screenshotPath.find_last_of('.')) + "_AO.png";
                SaveTextureToPng(*_device, _ssaoTarget.get(), occlusionPath);
            }

            if (_shadowUniforms.CastShadows)
            {
                for (int face = 0; face < 6; face++)
                {
                    auto shadowPath = _screenshotPath.substr(0, _screenshotPath.find_last_of('.')) + "_Shadow" + std::to_string(face) + ".png";
                    SaveTextureToPng(*_device, _shadowMap.get(), shadowPath, face);
                }
            }
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
