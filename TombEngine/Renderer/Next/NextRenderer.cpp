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
#include "Renderer/RendererUtils.h"
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
    constexpr auto UPLOAD_RING_INITIAL_SIZE  = 4 * 1024 * 1024;

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

        _uploadRing.Initialize(*_device, UPLOAD_RING_INITIAL_SIZE);
        _targetPool.Initialize(*_device);
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
        _readback.Poll(*_device, true);
        _roomTextures.clear();
        _animatedTextures.clear();
        _staticTextures.clear();
        _moveableTextures.clear();
        _geometry = {};
        for (auto& section : _geometrySections)
            section = {};

        _instanceBuffer = {};
        _lightBuffer = {};
        _boneBuffer = {};
        _instanceIndexBuffer = nullptr;
        _animatedFramesBuffer = nullptr;
        _animatedSetsBuffer = nullptr;
        LogGpuTimings();

        ClearPipelineCaches();

        for (auto& shader : _noAlphaTestPixelShaders)
            shader = nullptr;

        _sortedIndexBuffer = {};
        _objectsVertexShader = nullptr;
        _objectsPixelShader = nullptr;
        _colorTarget = nullptr;
        _finalTarget = nullptr;
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
        _debrisVertexBuffer = {};
        _debrisInstanceBuffer = {};
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
        _sceneDepthCopy = nullptr;
        _distortionTarget = nullptr;
        _antialiasingSource = nullptr;
        _fxaaPipeline = nullptr;
        _fxaaPixelShader = nullptr;
        _depthCopyPipeline = nullptr;
        _depthCopyPixelShader = nullptr;
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
        _emissiveTarget = nullptr;
        _reflectionTarget = nullptr;
        _videoTexture = nullptr;
        _uploadRing.Release();
        _imGuiFontTexture = nullptr;
        _imGuiVertexBuffer = {};
        _imGuiIndexBuffer = {};
        _skyReflectionTarget = nullptr;
        _atmosphericSkyPipeline = nullptr;
        _overlayPipeline = nullptr;
        _overlayBarPipeline = nullptr;
        _overlayBarPixelShader = nullptr;
        _overlayVertexShader = nullptr;
        _overlayPixelShader = nullptr;
        _overlayVertexBuffer = {};
        _overlayImages.clear();
        _overlayObjectDraws.clear();
        _overlayInstanceData.clear();
        _overlayBoneData.clear();
        _overlayInstanceBuffer = {};
        _overlayBoneBuffer = {};
        _inventoryVertexShader = nullptr;
        _inventoryPixelShader = nullptr;
        _font = {};
        _atmosphericSkyVertexShader = nullptr;
        _atmosphericSkyPixelShader = nullptr;
        _glowTarget = nullptr;
        _glowDownscalePipeline = nullptr;
        _glowBlurPipeline = nullptr;
        _glowVertexShader = nullptr;
        _glowDownscaleShader = nullptr;
        _glowBlurShader = nullptr;
        _glowCombinePipeline = nullptr;
        _glowCombineShader = nullptr;
        for (int i = 0; i < DOF_PASS_COUNT; i++)
        {
            _dofPipelines[i] = nullptr;
            _dofPixelShaders[i] = nullptr;
        }

        for (auto& target : _dofTargets)
            target = nullptr;

        _dofVertexShader = nullptr;
        for (auto& preset : _smaaPresets)
            preset = SmaaPreset{};

        _smaaAreaTexture = nullptr;
        _smaaSearchTexture = nullptr;
        _targetPool.Clear();
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

    // Pipelines created on first use.
    void NextRenderer::ClearPipelineCaches()
    {
        for (auto& shaderPipelines : _scenePipelines)
        {
            for (auto& variants : shaderPipelines)
            {
                variants[0] = nullptr;
                variants[1] = nullptr;
            }
        }

        for (auto& pipeline : _overlayPipelines)
            pipeline = nullptr;
    }

    // Shader sources of a development checkout, searched from the executable directory upwards.
    static std::filesystem::path FindShaderSourceDirectory()
    {
        constexpr auto SEARCH_DEPTH_MAX = 6;

        const char* basePath = SDL_GetBasePath();
        if (basePath == nullptr)
            return {};

        auto directory = std::filesystem::path(basePath);
        for (int i = 0; i < SEARCH_DEPTH_MAX && !directory.empty(); i++)
        {
            auto candidate = directory / "TombEngine" / "Shaders" / "RHI";
            if (std::filesystem::exists(candidate / "CompileShaders.ps1"))
                return candidate;

            directory = directory.parent_path();
        }

        return {};
    }

    // Development builds only: recompiles the changed shader sources with the build script, then recreates every
    // shader and pipeline. The previous shaders stay in use when compilation fails.
    void NextRenderer::ReloadShaders()
    {
        if (!DEBUG_BUILD || _device == nullptr)
            return;

        auto sourceDirectory = FindShaderSourceDirectory();
        if (sourceDirectory.empty())
        {
            TENLog("Next renderer: shader sources not found, reload skipped.", LogLevel::Warning);
            return;
        }

        auto command = "powershell -ExecutionPolicy Bypass -File \"" + (sourceDirectory / "CompileShaders.ps1").string() +
                       "\" -OutputDir \"" + _shaderDirectory + "\" -DebugInfo";
        if (std::system(command.c_str()) != 0)
        {
            TENLog("Next renderer: shader compilation failed, previous shaders kept.", LogLevel::Error);
            return;
        }

        auto lock = std::lock_guard<std::mutex>(_frameMutex);
        _device->WaitIdle();
        ClearPipelineCaches();

        bool isReloaded = CreatePipelines();
        TENLog(isReloaded ? "Next renderer: shaders reloaded." : "Next renderer: shader reload failed.", isReloaded ? LogLevel::Info : LogLevel::Error);
    }

    // Room and object draws without alpha test use pixel shaders without clip, which keep early depth testing.
    bool NextRenderer::LoadNoAlphaTestShaders()
    {
        struct NoAlphaTestShader
        {
            SceneShader Shader     = SceneShader::Rooms;
            const char* File       = nullptr;
            const char* EntryPoint = nullptr;
        };

        constexpr NoAlphaTestShader NO_ALPHA_TEST_SHADERS[] =
        {
            { SceneShader::Rooms, "Rooms", "PS" },
            { SceneShader::RoomsPrepass, "Rooms", "PSDepthNormal" },
            { SceneShader::Objects, "Objects", "PS" },
            { SceneShader::ObjectsMirrored, "Objects", "PS" },
            { SceneShader::ObjectsPrepass, "Objects", "PSDepthNormal" }
        };

        for (const auto& entry : NO_ALPHA_TEST_SHADERS)
        {
            _noAlphaTestPixelShaders[(int)entry.Shader] = LoadShader(*_device, _shaderDirectory, entry.File, entry.EntryPoint, "NO_ALPHA_TEST");
            if (_noAlphaTestPixelShaders[(int)entry.Shader] == nullptr)
                return false;
        }

        return true;
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

        _inventoryVertexShader = LoadShader(*_device, _shaderDirectory, "Objects", "VSInventory");
        _inventoryPixelShader = LoadShader(*_device, _shaderDirectory, "Objects", "PSInventory");
        if (_inventoryVertexShader == nullptr || _inventoryPixelShader == nullptr)
            return false;

        _skyVertexShader = LoadShader(*_device, _shaderDirectory, "Sky", "VS");
        _skyPixelShader = LoadShader(*_device, _shaderDirectory, "Sky", "PS");
        if (_skyVertexShader == nullptr || _skyPixelShader == nullptr || !CreateSkyGeometry())
            return false;

        if (!CreateSpriteResources() || !CreateSSAOResources() || !CreatePostProcessResources() || !CreateShadowResources() || !CreateGlowResources() || !CreateDOFResources() || !CreateSMAAResources() ||
            !CreateSkyReflectionResources() || !CreateAtmosphereResources() || !CreateOverlayResources(TEN::Renderer::Utils::GetAssetPath("Textures/Font.spritefont")))
            return false;

        if (!LoadNoAlphaTestShaders())
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
    BlendState GetBlendState(BlendMode blendMode)
    {
        auto state = BlendState{};
        state.EnableBlend = true;

        switch (blendMode)
        {
        case BlendMode::Additive:
            state.SrcColor = state.SrcAlpha = BlendFactor::SrcAlpha;
            state.DstColor = state.DstAlpha = BlendFactor::One;
            break;

        case BlendMode::Distortion:
            state.SrcColor = state.SrcAlpha = BlendFactor::One;
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

    // Attributes 0-4, read by every level geometry shader.
    template <typename TVertex>
    static void SetCommonVertexLayout(VertexLayout& layout, VertexElementFormat uvFormat)
    {
        layout.Buffers[0] = VertexBufferLayout{ 0, (int)sizeof(TVertex), VertexInputRate::Vertex };
        layout.Attributes[0] = VertexAttribute{ 0, 0, VertexElementFormat::Float3, (int)offsetof(TVertex, Position) };
        layout.Attributes[1] = VertexAttribute{ 1, 0, uvFormat, (int)offsetof(TVertex, UV) };
        layout.Attributes[2] = VertexAttribute{ 2, 0, VertexElementFormat::UByte4Norm, (int)offsetof(TVertex, Color) };
        layout.Attributes[3] = VertexAttribute{ 3, 0, VertexElementFormat::UInt, (int)offsetof(TVertex, Effects) };
        layout.Attributes[4] = VertexAttribute{ 4, 0, VertexElementFormat::UInt, (int)offsetof(TVertex, AnimationFrameOffsetIndexHash) };
        layout.NumAttributes = 5;
    }

    // Object shaders add the instance index stream and the skinning data.
    template <typename TVertex>
    static void SetObjectVertexLayout(VertexLayout& layout, VertexElementFormat uvFormat)
    {
        SetCommonVertexLayout<TVertex>(layout, uvFormat);
        layout.Attributes[5] = VertexAttribute{ 5, 1, VertexElementFormat::UInt, 0 };
        layout.Attributes[6] = VertexAttribute{ 6, 0, VertexElementFormat::Byte4Norm, (int)offsetof(TVertex, Normal) };
        layout.Attributes[7] = VertexAttribute{ 7, 0, VertexElementFormat::UByte4, (int)offsetof(TVertex, BoneIndex) };
        layout.Attributes[8] = VertexAttribute{ 8, 0, VertexElementFormat::UByte4Norm, (int)offsetof(TVertex, BoneWeight) };
        layout.Attributes[9] = VertexAttribute{ 9, 0, VertexElementFormat::Byte4Norm, (int)offsetof(TVertex, Tangent) };
        layout.Attributes[10] = VertexAttribute{ 10, 0, VertexElementFormat::Byte4Norm, (int)offsetof(TVertex, FaceNormal) };
        layout.NumAttributes = 11;
    }

    // Pipelines are created on first use per shader, blend mode and alpha test; opaque and alpha test blending share one.
    // Shaders without a NO_ALPHA_TEST permutation always use their alpha tested pixel shader.
    IGraphicsPipeline* NextRenderer::GetScenePipeline(SceneShader shader, BlendMode blendMode, bool isAlphaTested)
    {
        int modeIndex = (blendMode == BlendMode::AlphaTest) ? (int)BlendMode::Opaque : std::clamp((int)blendMode, 0, 15);
        int variant = (isAlphaTested || _noAlphaTestPixelShaders[(int)shader] == nullptr) ? 1 : 0;
        auto& pipeline = _scenePipelines[(int)shader][modeIndex][variant];
        if (pipeline != nullptr)
            return pipeline.get();

        bool isObjects = (shader == SceneShader::Objects || shader == SceneShader::ObjectsPrepass);
        bool isParaboloid = (shader == SceneShader::SkyParaboloid);
        bool isSky = (shader == SceneShader::Sky) || isParaboloid;
        bool isSprites = (shader == SceneShader::Sprites);
        bool isPrepass = (shader == SceneShader::RoomsPrepass || shader == SceneShader::ObjectsPrepass);
        bool isShadow = (shader == SceneShader::ObjectsShadow);
        bool isDebris = (shader == SceneShader::ObjectsDebris);
        bool isMirrored = (shader == SceneShader::ObjectsMirrored);
        bool isInventory = (shader == SceneShader::Inventory);
        isObjects = isObjects || isShadow || isDebris || isMirrored || isInventory;

        IShader* vertexShaders[(int)SceneShader::Count] =
        {
            _roomsVertexShader.get(), _objectsVertexShader.get(), _skyVertexShader.get(), _spritesVertexShader.get(),
            _roomsVertexShader.get(), _objectsVertexShader.get(), _objectsVertexShader.get(), _objectsVertexShader.get(), _objectsVertexShader.get(),
            _skyVertexShader.get(), _inventoryVertexShader.get()
        };
        IShader* pixelShaders[(int)SceneShader::Count] =
        {
            _roomsPixelShader.get(), _objectsPixelShader.get(), _skyPixelShader.get(), _spritesPixelShader.get(),
            _roomsPrepassPixelShader.get(), _objectsPrepassPixelShader.get(), _objectsShadowPixelShader.get(), _objectsPixelShader.get(), _objectsPixelShader.get(),
            _skyPixelShader.get(), _inventoryPixelShader.get()
        };
        const char* names[(int)SceneShader::Count] =
        {
            "Rooms", "Objects", "Sky", "Sprites", "RoomsPrepass", "ObjectsPrepass", "ObjectsShadow", "ObjectsDebris", "ObjectsMirrored", "SkyParaboloid", "Inventory"
        };

        auto desc = GraphicsPipelineDesc{};
        desc.VertexShader = vertexShaders[(int)shader];
        desc.PixelShader = (variant == 0) ? _noAlphaTestPixelShaders[(int)shader].get() : pixelShaders[(int)shader];
        desc.Layout.NumBuffers = (isObjects || isSprites) ? 2 : 1;
        // Debris has one instance per triangle, so its instance index comes from a per vertex stream.
        desc.Layout.Buffers[1] = VertexBufferLayout{ 1, (int)sizeof(unsigned int), isDebris ? VertexInputRate::Vertex : VertexInputRate::Instance };
        if (isDebris)
            desc.Rasterizer.Cull = RHI::CullMode::None;
        else if (isMirrored)
            desc.Rasterizer.Cull = RHI::CullMode::Front;
        else if (isParaboloid)
            desc.Rasterizer.Cull = RHI::CullMode::None;

        // Debris is built every frame from legacy vertices; level geometry and the sky use the packed vertices.
        if (isSprites)
        {
            desc.Layout.Buffers[0] = VertexBufferLayout{ 0, (int)sizeof(unsigned int), VertexInputRate::Vertex };
            desc.Layout.Attributes[0] = VertexAttribute{ 0, 0, VertexElementFormat::UInt, 0 };
            desc.Layout.Attributes[1] = VertexAttribute{ 1, 1, VertexElementFormat::UInt, 0 };
            desc.Layout.NumAttributes = 2;
            desc.Rasterizer.Cull = RHI::CullMode::None;
        }
        else if (isDebris)
        {
            SetObjectVertexLayout<Vertex>(desc.Layout, VertexElementFormat::Float2);
        }
        else if (isObjects)
        {
            SetObjectVertexLayout<GpuObjectVertex>(desc.Layout, VertexElementFormat::UShort2Norm);
        }
        else if (isSky)
        {
            SetCommonVertexLayout<GpuObjectVertex>(desc.Layout, VertexElementFormat::UShort2Norm);
        }
        else
        {
            SetCommonVertexLayout<GpuRoomVertex>(desc.Layout, VertexElementFormat::UShort2Norm);
            desc.Layout.Attributes[5] = VertexAttribute{ 5, 0, VertexElementFormat::Byte4Norm, (int)offsetof(GpuRoomVertex, Normal) };
            desc.Layout.Attributes[6] = VertexAttribute{ 6, 0, VertexElementFormat::Byte4Norm, (int)offsetof(GpuRoomVertex, Tangent) };
            desc.Layout.Attributes[7] = VertexAttribute{ 7, 0, VertexElementFormat::Byte4Norm, (int)offsetof(GpuRoomVertex, FaceNormal) };
            desc.Layout.NumAttributes = 8;
        }

        // The prepass writes view normals and emissive, the inventory color and emissive.
        desc.NumColorTargets = (isPrepass || isInventory) ? 2 : 1;
        desc.ColorTargets[0].Format = (isPrepass || isParaboloid) ? PixelFormat::RGBA8Unorm : (isShadow ? PixelFormat::R32Float : _device->GetSwapchainFormat());
        desc.ColorTargets[0].Blend = GetBlendState((isPrepass || isShadow) ? BlendMode::Opaque : (BlendMode)modeIndex);
        desc.ColorTargets[1].Format = PixelFormat::RGBA8Unorm;
        desc.ColorTargets[1].Blend = GetBlendState(BlendMode::Opaque);
        // Distortion accumulates in a half resolution target without depth; the post-process rejects occluded texels.
        bool isDistortion = (modeIndex == (int)BlendMode::Distortion);
        desc.DepthStencil.EnableDepthWrite = ((modeIndex == (int)BlendMode::Opaque) && !isSky) || isInventory;
        desc.DepthStencil.EnableDepthTest = !isSky && !isDistortion;
        desc.DepthFormat = (isDistortion || isParaboloid) ? PixelFormat::Unknown : PixelFormat::D32Float;
        desc.Name = std::string(names[(int)shader]) + ".BlendMode" + std::to_string(modeIndex) + ((variant == 0) ? ".NoAlphaTest" : "");

        pipeline = _device->CreateGraphicsPipeline(desc);
        if (pipeline == nullptr)
            TENLog("Next renderer: cannot create pipeline " + desc.Name, LogLevel::Error);

        return pipeline.get();
    }

    // Targets sampled by later passes are cleared once at creation: some are written only when their feature is
    // active, and sampling a texture never written leaves it in an undefined layout on Vulkan.
    void NextRenderer::ClearSampledTargets()
    {
        ITexture* targets[] =
        {
            _distortionTarget.get(), _emissiveTarget.get(), _glowTarget.get(), _reflectionTarget.get(), _dofTargets[0].get(), _dofTargets[1].get(),
            _antialiasingSource.get(), _colorTarget.get(), _skyReflectionTarget.get()
        };

        auto* commandBuffer = _device->AcquireCommandBuffer();
        if (commandBuffer == nullptr)
            return;

        for (auto* target : targets)
        {
            if (target == nullptr)
                continue;

            int layerCount = (target->GetDesc().Type == TextureType::Texture2DArray) ? target->GetDesc().DepthOrLayers : 1;
            for (int layer = 0; layer < layerCount; layer++)
            {
                auto colorTarget = ColorTargetInfo{};
                colorTarget.Texture = target;
                colorTarget.Layer = layer;
                commandBuffer->BeginRenderPass(&colorTarget, 1, nullptr);
                commandBuffer->EndRenderPass();
            }
        }

        commandBuffer->Submit();
    }

    bool NextRenderer::EnsureRenderTargets(int width, int height)
    {
        if (_colorTarget != nullptr && _colorTarget->GetDesc().Width == width && _colorTarget->GetDesc().Height == height)
            return true;

        _targetPool.Clear();

        auto colorDesc = TextureDesc{};
        colorDesc.Width = width;
        colorDesc.Height = height;
        colorDesc.Format = _device->GetSwapchainFormat();
        colorDesc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        colorDesc.Name = "Frame.Color";
        _colorTarget = _device->CreateTexture(colorDesc);
        _hasScene = false;

        colorDesc.Name = "Frame.Final";
        _finalTarget = _device->CreateTexture(colorDesc);

        colorDesc.Name = "Scene.Color";
        _sceneTarget = _device->CreateTexture(colorDesc);

        // Distortion is low frequency: half resolution and no depth, as the legacy distortion target.
        auto distortionDesc = colorDesc;
        distortionDesc.Width = std::max(width / 2, 1);
        distortionDesc.Height = std::max(height / 2, 1);
        distortionDesc.Name = "Scene.Distortion";
        _distortionTarget = _device->CreateTexture(distortionDesc);

        colorDesc.Name = "PostProcess.Result";
        _antialiasingSource = _device->CreateTexture(colorDesc);

        colorDesc.Format = PixelFormat::R32Float;
        colorDesc.Name = "Scene.DepthCopy";
        _sceneDepthCopy = _device->CreateTexture(colorDesc);

        auto depthDesc = TextureDesc{};
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.Format = PixelFormat::D32Float;
        depthDesc.Usage = TextureUsage::DepthStencilTarget | TextureUsage::Sampler;
        depthDesc.Name = "Scene.Depth";
        _depthTarget = _device->CreateTexture(depthDesc);

        bool isCreated = (_colorTarget != nullptr && _finalTarget != nullptr && _sceneTarget != nullptr && _sceneDepthCopy != nullptr && _distortionTarget != nullptr && _depthTarget != nullptr && EnsureSSAOTargets(width, height) && EnsureGlowTargets(width, height) && EnsureDOFTargets(width, height));
        if (isCreated)
            ClearSampledTargets();

        return isCreated;
    }

    void NextRenderer::UploadBuffer(IBuffer* buffer, const void* data, int size, int offset)
    {
        auto transferBuffer = _device->CreateTransferBuffer(TransferBufferUsage::Upload, size);
        memcpy(_device->MapTransferBuffer(transferBuffer.get(), false), data, size);
        _device->UnmapTransferBuffer(transferBuffer.get());

        auto* commandBuffer = _device->AcquireCommandBuffer();
        auto* copyPass = commandBuffer->BeginCopyPass();
        copyPass->UploadToBuffer(transferBuffer.get(), 0, buffer, offset, size, false);
        commandBuffer->EndCopyPass();
        commandBuffer->Submit();
        _device->WaitIdle();
    }

    std::unique_ptr<ITexture> NextRenderer::CreateLevelTexture(const std::vector<unsigned char>& encodedData, const std::string& name, bool allowMips)
    {
        auto image = DecodedImage{};
        if (encodedData.empty() || !DecodeImage(encodedData, image))
            return nullptr;

        // Single-level images get their mip chain generated on the GPU, which needs a color target.
        bool generateMips = (image.MipLevels == 1 && allowMips);

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

        if (!CreateLevelGeometry(geometry))
            return;

        _hasEmissiveMaps = false;
        LoadTextureSet(g_Level.RoomTextures, _roomTextures, "RoomAtlas");
        LoadTextureSet(g_Level.StaticsTextures, _staticTextures, "StaticAtlas");
        LoadTextureSet(g_Level.MoveablesTextures, _moveableTextures, "MoveableAtlas");
        LoadTextureSet(g_Level.SpritesTextures, _spriteTextures, "SpriteAtlas");
        LoadAnimatedTextures(geometry);
        _skyTexture = CreateLevelTexture(g_Level.SkyTexture.colorMapData, "Sky");

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
        TENLog(fmt::format("Next renderer: level data uploaded in {:.1f} ms ({} room vertices, {} room textures, emissive maps: {}).",
            elapsed, geometry.RoomVertices->size(), _roomTextures.size(), _hasEmissiveMaps ? "yes" : "no"), LogLevel::Info);
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
            {
                texture.EmissiveMap = CreateLevelTexture(source[i].emissiveMapData, pageName + ".Emissive");
                _hasEmissiveMaps = true;
            }

            destination.push_back(std::move(texture));
        }
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
                frames.push_back(GpuAnimatedFrame{ texture.UV[0], texture.UV[1], texture.UV[2], texture.UV[3] });
        }

        AddWaterfallSets(geometry, frames, sets);
        AddVideoSets(geometry, frames, sets);

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
            frames.push_back(GpuAnimatedFrame{ Vector2(0.0f, minV), Vector2(1.0f, minV), Vector2(1.0f, maxV), Vector2(0.0f, maxV) });
        }
    }

    // Material properties can be animated by scripts, so the whole table is rebuilt every frame.
    void NextRenderer::CollectMaterials(float interpolationFactor)
    {
        _materialData.resize(g_Level.Materials.size() * sizeof(GpuMaterial));
        auto* materials = (GpuMaterial*)_materialData.data();
        _hasReflectiveMaterials = false;
        _hasSkyReflectiveMaterials = false;

        for (int i = 0; i < (int)g_Level.Materials.size(); i++)
        {
            const auto& material = g_Level.Materials[i];
            auto properties = material.GetInterpolatedProperties(interpolationFactor);

            auto& gpuMaterial = materials[i];
            gpuMaterial = GpuMaterial{};
            for (int j = 0; j < MaterialData::PropertyCount; j++)
                gpuMaterial.Properties[j] = properties[j];

            // Scripts can change material types at runtime.
            if (material.Type == TextureMaterialType::Reflective)
                _hasReflectiveMaterials = true;
            else if (material.Type == TextureMaterialType::SkyboxReflective)
                _hasSkyReflectiveMaterials = true;

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

    void NextRenderer::BindTexturePage(IRenderPass& renderPass, const RoomTexture* page, ITexture* colorOverride, ITexture* emissiveOverride)
    {
        auto getMap = [&](const std::unique_ptr<ITexture>* map, ITexture* fallback)
        {
            return (page != nullptr && *map != nullptr) ? map->get() : fallback;
        };

        auto* sampler = _anisotropicSampler.get();
        auto* ambientOcclusion = _isAmbientOcclusionActive ? _ssaoTarget.get() : _whiteTexture.get();
        auto* shadowMap = _isRenderingShadowMap ? _dummyShadowMap.get() : _shadowMap.get();
        auto* caustics = (_causticsTexture != nullptr) ? _causticsTexture : _whiteTexture.get();
        auto* reflections = _hasReflectiveMaterials ? _reflectionTarget.get() : _blackTexture.get();
        auto* skyReflections = _hasSkyReflectiveMaterials ? _skyReflectionTarget.get() : _dummyShadowMap.get();
        TextureSamplerBinding bindings[9] =
        {
            { (colorOverride != nullptr) ? colorOverride : getMap(page ? &page->ColorMap : nullptr, _whiteTexture.get()), sampler },
            { getMap(page ? &page->NormalMap : nullptr, _flatNormalTexture.get()), sampler },
            { getMap(page ? &page->ORSHMap : nullptr, _defaultORSHTexture.get()), sampler },
            { (emissiveOverride != nullptr) ? emissiveOverride : getMap(page ? &page->EmissiveMap : nullptr, _blackTexture.get()), sampler },
            { ambientOcclusion, _linearClampSampler.get() },
            { shadowMap, _pointClampSampler.get() },
            { caustics, sampler },
            { reflections, _linearClampSampler.get() },
            { skyReflections, _linearClampSampler.get() }
        };
        renderPass.BindPixelSamplers(0, bindings, 9);
    }

    // Positive for alpha test, negative for the soft edge pass (texels below the threshold), zero for none.
    float NextRenderer::GetAlphaThreshold(const RendererBucket& bucket, const BucketDrawInfo& info)
    {
        if (info.IsAlphaEdgePass)
            return -ALPHA_TEST_THRESHOLD;

        if (info.IsForcedAlphaTest)
            return ALPHA_TEST_THRESHOLD;

        if (info.IsAlphaTestDisabled)
            return 0.0f;

        return (bucket.BlendMode == BlendMode::AlphaTest) ? ALPHA_TEST_THRESHOLD : 0.0f;
    }

    void NextRenderer::BindBucket(ICommandBuffer& commandBuffer, IRenderPass& renderPass, const RendererBucket& bucket, const std::vector<RoomTexture>& pages,
                                  BlendMode blendMode, unsigned int lightCount, const BucketDrawInfo& room, BucketBindState& state)
    {
        auto pixelUniforms = DrawPixelUniforms{};
        pixelUniforms.AlphaThreshold = GetAlphaThreshold(bucket, room);
        pixelUniforms.BlendMode = (unsigned int)(room.IsAlphaEdgePass ? BlendMode::AlphaBlend : blendMode);
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

        // While a background video plays, video sets show the whole frame, also as emissive if the material has one.
        int videoSet = GetVideoSet(bucket);
        int animatedSet = (room.AnimatedSetOverride != NO_VALUE) ? room.AnimatedSetOverride : (bucket.Animated ? bucket.Texture : NO_VALUE);
        if (videoSet != NO_VALUE)
            animatedSet = videoSet;

        auto vertexUniforms = DrawVertexUniforms{ animatedSet, room.IsWaterRoom };
        if (vertexUniforms.AnimatedSetIndex != state.VertexUniforms.AnimatedSetIndex || vertexUniforms.IsWaterRoom != state.VertexUniforms.IsWaterRoom)
        {
            commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 1, &vertexUniforms, sizeof(vertexUniforms));
            state.VertexUniforms = vertexUniforms;
        }

        const auto& sourcePages = bucket.Animated ? _animatedTextures : pages;
        const auto* page = (bucket.Texture >= 0 && bucket.Texture < (int)sourcePages.size()) ? &sourcePages[bucket.Texture] : nullptr;
        if (videoSet != NO_VALUE)
        {
            bool hasEmissive = (pixelUniforms.MaterialIndex < g_Level.Materials.size()) && g_Level.Materials[pixelUniforms.MaterialIndex].HasEmissiveMap;
            BindTexturePage(renderPass, page, _videoTexture.get(), hasEmissive ? _videoTexture.get() : nullptr);
            state.IsPageBound = false;
        }
        else if (!state.IsPageBound || page != state.Page)
        {
            BindTexturePage(renderPass, page);
            state.Page = page;
            state.IsPageBound = true;
        }
    }

    BlendMode NextRenderer::GetPassBlendMode(ScenePass pass)
    {
        switch (pass)
        {
        case ScenePass::AlphaEdges:
            return BlendMode::AlphaBlend;

        case ScenePass::Additive:
            return BlendMode::Additive;

        case ScenePass::Distortion:
            return BlendMode::Distortion;

        default:
            return BlendMode::Opaque;
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

        case ScenePass::Distortion:
            return (bucket.BlendMode == BlendMode::Distortion);

        default:
            return false;
        }
    }

    // Prepass and opaque passes hold buckets with and without alpha test, the alpha edge pass only alpha tested ones,
    // the others none.
    bool NextRenderer::IsAlphaTestVariantInPass(ScenePass pass, bool isAlphaTested)
    {
        if (pass == ScenePass::Prepass || pass == ScenePass::Opaque)
            return true;

        return (isAlphaTested == (pass == ScenePass::AlphaEdges));
    }

    // Buckets without alpha test are drawn first, with the pixel shader without clip.
    void NextRenderer::DrawRooms(ICommandBuffer& commandBuffer, IRenderPass& renderPass, const RenderView& view, ScenePass pass)
    {
        constexpr const char* GROUP_NAMES[] = { "Rooms prepass", "Rooms opaque", "Rooms alpha edges", "Rooms additive", "Rooms distortion" };

        commandBuffer.PushDebugGroup(GROUP_NAMES[(int)pass]);

        for (int variant = 0; variant < 2; variant++)
        {
            if (IsAlphaTestVariantInPass(pass, variant == 1))
                DrawRoomBuckets(commandBuffer, renderPass, view, pass, variant == 1);
        }

        commandBuffer.PopDebugGroup();
    }

    void NextRenderer::DrawRoomBuckets(ICommandBuffer& commandBuffer, IRenderPass& renderPass, const RenderView& view, ScenePass pass, bool isAlphaTested)
    {
        bool additive = (pass == ScenePass::Additive);
        bool isAlphaEdges = (pass == ScenePass::AlphaEdges);

        auto shader = (pass == ScenePass::Prepass) ? SceneShader::RoomsPrepass : SceneShader::Rooms;
        renderPass.BindPipeline(GetScenePipeline(shader, GetPassBlendMode(pass), isAlphaTested));

        BindLevelGeometry(renderPass, MeshSource::Rooms, false);

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

                if (!IsBucketInPass(bucket, pass) || (GetAlphaThreshold(bucket, roomInfo) != 0.0f) != isAlphaTested)
                    continue;

                BindBucket(commandBuffer, renderPass, bucket, _roomTextures, bucket.BlendMode, (unsigned int)_roomLightCount, roomInfo, state);
                DrawBucket(renderPass, MeshSource::Rooms, bucket, 1, 0);
            }
        }
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
        _instanceOrigins.clear();
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
            int instanceCount = 0;
            for (const auto* staticMesh : statics)
            {
                // Fading statics are drawn with the sorted faces only, as in the legacy DrawStatics.
                if (staticMesh->Color.w < ALPHA_BLEND_THRESHOLD)
                    continue;

                _currentOrigin = InstanceOrigin{ staticMesh->RoomNumber, InstanceCategory::Static };
                instanceCount++;

                int lightOffset = 0;
                int lightCount = AddLights(staticMesh->LightsToDraw, interpolationFactor, NO_VALUE, NO_VALUE, 1.0f, lightOffset);
                AddInstance(staticMesh->World, staticMesh->Color, staticMesh->AmbientLight, mesh->LightMode, lightOffset, lightCount, 0, SKINNING_NONE);
            }

            if (instanceCount == 0)
                continue;

            for (const auto& bucket : mesh->Buckets)
            {
                if (bucket.NumIndices > 0)
                    _objectDraws.push_back(ObjectDraw{ MeshSource::Statics, &bucket, firstInstance, instanceCount });
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

                // Legacy mirrors skip items with cleared bodies.
                bool isReflectable = !(nativeItem.Flags & IFLAG_CLEAR_BODY);
                _currentOrigin = InstanceOrigin{ item->RoomNumber, isReflectable ? InstanceCategory::Moveable : InstanceCategory::None };

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

                RemoveFadedDraws(firstDraw, item->Color.w);
            }
        }

        if (LaraItem != nullptr)
            _currentOrigin = InstanceOrigin{ LaraItem->RoomNumber, InstanceCategory::Player };

        CollectPlayer(interpolationFactor);
        _currentOrigin = InstanceOrigin{};

        CollectEffectMeshes(view, interpolationFactor);
        CollectMirrors(view);
        CollectDebris(interpolationFactor);
        CollectGunFlashes(view);
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
        _instanceOrigins.push_back(_currentOrigin);
        return index;
    }

    int NextRenderer::AddBones(const Matrix* bones, int count)
    {
        int offset = (int)(_boneData.size() / sizeof(Matrix));
        const auto* bytes = (const unsigned char*)bones;
        _boneData.insert(_boneData.end(), bytes, bytes + (count * sizeof(Matrix)));
        return offset;
    }

    // Legacy GetBlendModeFromAlpha: opaque buckets of a fading object turn alpha blended and are drawn with the
    // sorted faces only. Shadow casters are marked before, as the legacy shadow map draws every bucket.
    void NextRenderer::RemoveFadedDraws(int firstDraw, float alpha)
    {
        if (alpha >= ALPHA_BLEND_THRESHOLD)
            return;

        auto isFaded = [](const ObjectDraw& draw)
        {
            auto blendMode = draw.Bucket->BlendMode;
            return (blendMode == BlendMode::Opaque || blendMode == BlendMode::AlphaTest || blendMode == BlendMode::FastAlphaBlend);
        };

        _objectDraws.erase(std::remove_if(_objectDraws.begin() + firstDraw, _objectDraws.end(), isFaded), _objectDraws.end());
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
    // bind pose corrected bones. Hair units follow the player.
    void NextRenderer::CollectPlayer(float interpolationFactor)
    {
        _hairInstances.fill(NO_VALUE);

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

        RemoveFadedDraws(firstDraw, item->Color.w);
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

            if (unitIndex < (int)_hairInstances.size())
                _hairInstances[unitIndex] = instance;

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

    void NextRenderer::UploadFrameStorage(FrameStorageBuffer& storage, const std::vector<unsigned char>& data, int stride, unsigned int usage, const char* name)
    {
        // Storage buffers cannot be empty, so the smallest buffer holds one element.
        int dataSize = std::max((int)data.size(), stride);
        if (dataSize > storage.Capacity || storage.Buffer == nullptr)
        {
            storage.Capacity = std::max(dataSize * 2, stride * INITIAL_INSTANCE_CAPACITY);
            storage.Buffer = _device->CreateBuffer(BufferDesc{ storage.Capacity, stride, usage, name });
        }

        _uploadRing.UploadToBuffer(storage.Buffer.get(), 0, data.data(), (int)data.size());
    }

    // All per-frame data goes to the GPU in a single copy pass before the scene pass.
    void NextRenderer::UploadFrameData(ICommandBuffer& commandBuffer, const FrameEnvironment& environment)
    {
        commandBuffer.PushDebugGroup("Upload frame data");
        _uploadRing.Begin(*commandBuffer.BeginCopyPass());

        UploadFrameStorage(_instanceBuffer, _instanceData, (int)sizeof(GpuObjectInstance), BufferUsage::GraphicsStorageRead, "Frame.Instances");
        UploadFrameStorage(_lightBuffer, _lightData, (int)sizeof(GpuLight), BufferUsage::GraphicsStorageRead, "Frame.Lights");
        UploadFrameStorage(_boneBuffer, _boneData, (int)sizeof(Matrix), BufferUsage::GraphicsStorageRead, "Frame.Bones");
        UploadFrameStorage(_sortedIndexBuffer, _sortedIndexData, (int)sizeof(int), BufferUsage::Index, "Frame.SortedIndices");
        UploadFrameStorage(_spriteBuffer, _spriteData, (int)sizeof(GpuSprite), BufferUsage::GraphicsStorageRead, "Frame.Sprites");
        UploadFrameStorage(_materialBuffer, _materialData, (int)sizeof(GpuMaterial), BufferUsage::GraphicsStorageRead, "Frame.Materials");
        UploadFrameStorage(_decalBuffer, _decalData, (int)sizeof(GpuDecal), BufferUsage::GraphicsStorageRead, "Frame.Decals");
        UploadFrameStorage(_debrisVertexBuffer, _debrisVertexData, (int)sizeof(Vertex), BufferUsage::Vertex, "Frame.DebrisVertices");
        UploadFrameStorage(_debrisInstanceBuffer, _debrisInstanceData, (int)sizeof(unsigned int), BufferUsage::Vertex, "Frame.DebrisInstances");
        UploadVideoFrame(environment.VideoFrame, environment.VideoWidth, environment.VideoHeight);

        _uploadRing.End();
        commandBuffer.EndCopyPass();
        commandBuffer.PopDebugGroup();
    }

    void NextRenderer::DrawObjects(ICommandBuffer& commandBuffer, IRenderPass& renderPass, ScenePass pass, bool isMirrored)
    {
        constexpr const char* GROUP_NAMES[] = { "Objects prepass", "Objects opaque", "Objects alpha edges", "Objects additive", "Objects distortion" };

        const auto& draws = isMirrored ? _mirrorDraws : _objectDraws;
        if (draws.empty() || _instanceBuffer.Buffer == nullptr)
            return;

        commandBuffer.PushDebugGroup(isMirrored ? "Mirrored objects" : GROUP_NAMES[(int)pass]);

        for (int variant = 0; variant < 2; variant++)
        {
            if (IsAlphaTestVariantInPass(pass, variant == 1))
                DrawObjectBuckets(commandBuffer, renderPass, draws, pass, isMirrored, variant == 1);
        }

        commandBuffer.PopDebugGroup();
    }

    void NextRenderer::DrawObjectBuckets(ICommandBuffer& commandBuffer, IRenderPass& renderPass, const std::vector<ObjectDraw>& draws, ScenePass pass,
                                         bool isMirrored, bool isAlphaTested)
    {
        bool isAlphaEdges = (pass == ScenePass::AlphaEdges);

        auto shader = (pass == ScenePass::Prepass) ? SceneShader::ObjectsPrepass : (isMirrored ? SceneShader::ObjectsMirrored : SceneShader::Objects);
        renderPass.BindPipeline(GetScenePipeline(shader, GetPassBlendMode(pass), isAlphaTested));

        IBuffer* storageBuffers[4] = { _animatedFramesBuffer.get(), _animatedSetsBuffer.get(), _instanceBuffer.Buffer.get(), _boneBuffer.Buffer.get() };
        renderPass.BindVertexStorageBuffers(0, storageBuffers, 4);

        BindScenePixelStorage(renderPass);
        BindLevelGeometry(renderPass, MeshSource::Moveables, true);

        auto state = BucketBindState{};

        for (const auto& draw : draws)
        {
            const auto& bucket = *draw.Bucket;
            auto drawInfo = BucketDrawInfo{};
            drawInfo.AnimatedSetOverride = draw.AnimatedSetOverride;
            drawInfo.IsAlphaEdgePass = isAlphaEdges;
            if (!IsBucketInPass(bucket, pass) || (GetAlphaThreshold(bucket, drawInfo) != 0.0f) != isAlphaTested)
                continue;

            const auto& pages = (draw.Source == MeshSource::Statics) ? _staticTextures : _moveableTextures;
            BindBucket(commandBuffer, renderPass, bucket, pages, bucket.BlendMode, 0, drawInfo, state);
            DrawBucket(renderPass, draw.Source, bucket, draw.InstanceCount, draw.FirstInstance);
        }
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
                draw.Sprites.IsSoftParticle = object.Sprite->SoftParticle;
                draw.Sprites.RenderType = (int)object.Sprite->renderType;
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
            int hairInstance = NO_VALUE;
            switch (object.ObjectType)
            {
            case RendererObjectType::Room:
                source = MeshSource::Rooms;
                sourceIndices = _geometrySections[(int)source].Indices;
                break;

            case RendererObjectType::Static:
                source = MeshSource::Statics;
                sourceIndices = _geometrySections[(int)source].Indices;
                break;

            case RendererObjectType::Moveable:
            case RendererObjectType::MoveableAsStatic:
            case RendererObjectType::Effect:
                sourceIndices = _geometrySections[(int)source].Indices;
                break;

            // Hair faces reuse the hair unit instance built with the player.
            case RendererObjectType::HairPrimary:
            case RendererObjectType::HairSecondary:
                hairInstance = _hairInstances[(object.ObjectType == RendererObjectType::HairPrimary) ? 0 : 1];
                if (hairInstance != NO_VALUE)
                    sourceIndices = _geometrySections[(int)source].Indices;

                break;

            default:
                break;
            }

            // Skip runs without geometry to draw.
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
            draw.IsForcedAlphaTest = (object.ObjectType == RendererObjectType::MoveableAsStatic);
            draw.Bucket = object.Bucket;
            draw.BlendMode = object.BlendMode;
            draw.FirstIndex = firstIndex;
            draw.IndexCount = (int)(_sortedIndexData.size() / sizeof(int)) - firstIndex;
            if (hairInstance != NO_VALUE)
                draw.Instance = hairInstance;
            else
                draw.Instance = draw.IsRoom ? 0 : GetSortedObjectInstance(object, interpolationFactor);
            _sortedDraws.push_back(draw);
        }
    }

    void NextRenderer::DrawSortedFaces(ICommandBuffer& commandBuffer, IRenderPass& renderPass, bool isMirrored)
    {
        const auto& draws = isMirrored ? _mirrorSortedDraws : _sortedDraws;
        if (draws.empty())
            return;

        commandBuffer.PushDebugGroup(isMirrored ? "Mirrored sorted faces" : "Sorted transparent faces");

        if (!_sortedIndexData.empty())
            renderPass.BindIndexBuffer(BufferBinding{ _sortedIndexBuffer.Buffer.get(), 0 }, IndexSize::Bits32);

        IBuffer* objectStorageBuffers[4] = { _animatedFramesBuffer.get(), _animatedSetsBuffer.get(), _instanceBuffer.Buffer.get(), _boneBuffer.Buffer.get() };

        const IGraphicsPipeline* lastPipeline = nullptr;
        const ITexture* lastSpriteTexture = nullptr;
        // Vertex streams bound: the room vertex region, or the object region plus the instance indices; none after a pipeline change.
        int lastStreamCount = 0;
        auto state = BucketBindState{};

        for (const auto& draw : draws)
        {
            // Sprite runs overwrite the sampler and pixel uniform slots used by bucket draws.
            if (draw.IsSprite)
            {
                DrawSpriteRun(commandBuffer, renderPass, draw.Sprites, lastPipeline, lastSpriteTexture);
                lastStreamCount = 0;
                state = BucketBindState{};
                continue;
            }

            const auto& bucket = *draw.Bucket;
            auto shader = draw.IsRoom ? SceneShader::Rooms : (isMirrored ? SceneShader::ObjectsMirrored : SceneShader::Objects);

            auto* pipeline = GetScenePipeline(shader, draw.BlendMode);
            if (pipeline != lastPipeline)
            {
                renderPass.BindPipeline(pipeline);
                renderPass.BindVertexStorageBuffers(0, objectStorageBuffers, draw.IsRoom ? 2 : 4);
                BindScenePixelStorage(renderPass);
                lastPipeline = pipeline;
                lastStreamCount = 0;
                lastSpriteTexture = nullptr;
                state = BucketBindState{};
            }

            int streamCount = draw.IsRoom ? 1 : 2;
            if (streamCount != lastStreamCount)
            {
                BufferBinding vertexBindings[2] = { { _geometry.Vertices.get(), _geometrySections[(int)draw.Source].RegionOffset }, { _instanceIndexBuffer.get(), 0 } };
                renderPass.BindVertexBuffers(0, vertexBindings, streamCount);
                lastStreamCount = streamCount;
            }

            const auto& pages = draw.IsRoom ? _roomTextures : ((draw.Source == MeshSource::Statics) ? _staticTextures : _moveableTextures);
            // Decals are not drawn on sorted faces, as in the legacy renderer.
            auto roomInfo = BucketDrawInfo{ draw.IsWaterRoom ? 1u : 0u, draw.IsCausticsRoom ? 1u : 0u };
            roomInfo.IsForcedAlphaTest = draw.IsForcedAlphaTest;
            BindBucket(commandBuffer, renderPass, bucket, pages, draw.BlendMode, draw.IsRoom ? (unsigned int)_roomLightCount : 0, roomInfo, state);

            _frameStats.DrawCalls++;
            renderPass.DrawIndexed(draw.IndexCount, 1, draw.FirstIndex, _geometrySections[(int)draw.Source].BaseVertex, draw.Instance);
        }

        commandBuffer.PopDebugGroup();
    }

    // Same grid as the legacy InitializeSky: 20x20 tiles of 512 units in the XZ plane.
    bool NextRenderer::CreateSkyGeometry()
    {
        auto vertices = std::vector<GpuObjectVertex>{};
        auto indices = std::vector<int>{};
        vertices.reserve(SKY_VERTICES_COUNT);
        indices.reserve(SKY_INDICES_COUNT);

        auto addVertex = [&](int x, int z)
        {
            auto vertex = GpuObjectVertex{};
            vertex.Position = Vector3(-SKY_SIZE / 2.0f + x * 512.0f, 0.0f, -SKY_SIZE / 2.0f + z * 512.0f);
            vertex.UV[0] = (unsigned short)((x * USHRT_MAX) / SKY_TILES_COUNT);
            vertex.UV[1] = (unsigned short)((z * USHRT_MAX) / SKY_TILES_COUNT);
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

        int vertexSize = (int)(vertices.size() * sizeof(GpuObjectVertex));
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
    // Hemisphere 0 draws the sky of the view; -1 and 1 draw a hemisphere of the dual paraboloid sky reflections,
    // without fog bulbs and regardless of outside rooms, as the legacy reflection pass.
    void NextRenderer::DrawSky(ICommandBuffer& commandBuffer, IRenderPass& renderPass, const RenderView& view, float interpolationFactor, int hemisphere)
    {
        using namespace TEN::Effects::Environment;

        constexpr auto PARABOLOID_NEAR = 32.0f;

        bool isParaboloid = (hemisphere != 0);
        auto shader = isParaboloid ? SceneShader::SkyParaboloid : SceneShader::Sky;
        auto paraboloidView = Matrix::CreateLookAt(view.Camera.WorldPosition, view.Camera.WorldPosition + Vector3(0.0f, -BLOCK(1), 0.0f), Vector3::UnitX);
        auto getUniforms = [&](const Matrix& world, bool isFogBulbsEnabled)
        {
            auto uniforms = SkyVertexUniforms{ world, (isFogBulbsEnabled && !isParaboloid) ? 1u : 0u };
            uniforms.IsParaboloid = isParaboloid ? 1u : 0u;
            uniforms.Hemisphere = (float)hemisphere;
            uniforms.ParaboloidView = paraboloidView;
            uniforms.ParaboloidNear = PARABOLOID_NEAR;
            uniforms.ParaboloidFar = DEFAULT_FAR_VIEW;
            return uniforms;
        };

        bool isAnyRoomOutside = isParaboloid;
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

        // Atmospheric dome first, as an opaque background; the reflection map keeps the classic sky only.
        if (!isParaboloid)
            DrawAtmosphericSky(commandBuffer, renderPass);

        IBuffer* storageBuffers[2] = { _animatedFramesBuffer.get(), _animatedSetsBuffer.get() };
        auto drawUniforms = DrawVertexUniforms{ NO_VALUE };
        commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 1, &drawUniforms, sizeof(drawUniforms));

        // Layers.
        renderPass.BindPipeline(GetScenePipeline(shader, BlendMode::Additive));
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
                auto skyUniforms = getUniforms(world, layer == 0);
                commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 2, &skyUniforms, sizeof(skyUniforms));
                _frameStats.DrawCalls++;
                renderPass.DrawIndexed(SKY_INDICES_COUNT, 1, 0, 0, 0);
            }
        }

        // Stars, meteors and sun sprite, between the layers and the horizons as in the legacy renderer.
        if (!isParaboloid && !_skySpriteDraws.empty())
        {
            const IGraphicsPipeline* lastPipeline = nullptr;
            const ITexture* lastTexture = nullptr;
            for (const auto& draw : _skySpriteDraws)
                DrawSpriteRun(commandBuffer, renderPass, draw, lastPipeline, lastTexture);

            auto noAnimation = DrawVertexUniforms{ NO_VALUE };
            commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 1, &noAnimation, sizeof(noAnimation));
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

            auto skyUniforms = getUniforms(world, true);
            commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 2, &skyUniforms, sizeof(skyUniforms));
            commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &color, sizeof(color));

            BindLevelGeometry(renderPass, MeshSource::Moveables, false);

            int lastAnimatedSet = INT_MIN;
            for (const auto* mesh : horizonObject->ObjectMeshes)
            {
                for (const auto& bucket : mesh->Buckets)
                {
                    if (bucket.NumIndices == 0)
                        continue;

                    // Horizons always blend; alpha test buckets blend as alpha.
                    auto blendMode = (bucket.BlendMode == BlendMode::AlphaTest || bucket.BlendMode == BlendMode::Opaque) ? BlendMode::AlphaBlend : bucket.BlendMode;
                    renderPass.BindPipeline(GetScenePipeline(shader, blendMode));
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
                    DrawBucket(renderPass, MeshSource::Moveables, bucket, 1, 0);
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
        uniforms.FogBulbCount = std::min((int)view.FogBulbsToDraw.size(), FOG_BULBS_MAX);

        for (int i = 0; i < uniforms.FogBulbCount; i++)
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

    // The prepass always runs: the scene pass reuses its depth, and soft particles read a copy of it.
    // Reflective materials sample the scene of the previous frame, as in the legacy renderer.
    const NextRenderer::FramePass NextRenderer::FRAME_PASSES[] =
    {
        { "Shadow map", nullptr, &NextRenderer::RenderShadowMap },
        { "Depth and normals prepass", nullptr, &NextRenderer::RenderDepthPrepass },
        { "Depth copy", nullptr, &NextRenderer::CopySceneDepth },
        { "SSAO", &NextRenderer::_isAmbientOcclusionActive, &NextRenderer::RenderSSAO },
        { "Sky reflections", &NextRenderer::_hasSkyReflectiveMaterials, &NextRenderer::RenderSkyReflections },
        { "Distortion", &NextRenderer::_hasDistortion, &NextRenderer::RenderDistortion },
        { "Scene", nullptr, &NextRenderer::RenderScenePass },
        { "DOF", &NextRenderer::_isDofActive, &NextRenderer::RenderDOF },
        { "Reflections", &NextRenderer::_hasReflectiveMaterials, &NextRenderer::CopyReflectionSource },
        { "Glow", &NextRenderer::_hasEmissiveMaps, &NextRenderer::RenderGlow },
        { "Post-process", nullptr, &NextRenderer::RenderPostProcess }
    };

    void NextRenderer::RenderScenePass(ICommandBuffer& commandBuffer)
    {
        const auto& view = *_frameView;
        const auto& environment = *_frameEnvironment;

        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = _sceneTarget.get();
        colorTarget.ClearColor[0] = environment.FogColor.x;
        colorTarget.ClearColor[1] = environment.FogColor.y;
        colorTarget.ClearColor[2] = environment.FogColor.z;
        colorTarget.ClearColor[3] = 1.0f;

        auto depthTarget = DepthTargetInfo{};
        depthTarget.Texture = _depthTarget.get();
        depthTarget.Load = LoadOp::Load;
        depthTarget.Store = StoreOp::DontCare;

        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, &depthTarget);
        DrawSky(commandBuffer, *renderPass, view, environment.InterpolationFactor);

        // The sky color uses pixel slot 2 too: restore the shadow uniforms read by rooms and objects.
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &_shadowUniforms, sizeof(_shadowUniforms));
        DrawRooms(commandBuffer, *renderPass, view, ScenePass::Opaque);
        DrawObjects(commandBuffer, *renderPass, ScenePass::Opaque);
        DrawObjects(commandBuffer, *renderPass, ScenePass::Opaque, true);
        DrawDebris(commandBuffer, *renderPass, DebrisPass::Opaque);
        DrawRooms(commandBuffer, *renderPass, view, ScenePass::AlphaEdges);
        DrawObjects(commandBuffer, *renderPass, ScenePass::AlphaEdges);
        DrawObjects(commandBuffer, *renderPass, ScenePass::AlphaEdges, true);
        DrawSprites(commandBuffer, *renderPass, ScenePass::Opaque);
        DrawSprites(commandBuffer, *renderPass, ScenePass::Opaque, true);
        DrawRooms(commandBuffer, *renderPass, view, ScenePass::Additive);
        DrawObjects(commandBuffer, *renderPass, ScenePass::Additive);
        DrawObjects(commandBuffer, *renderPass, ScenePass::Additive, true);
        DrawDebris(commandBuffer, *renderPass, DebrisPass::Additive);
        DrawSprites(commandBuffer, *renderPass, ScenePass::Additive);
        DrawSprites(commandBuffer, *renderPass, ScenePass::Additive, true);
        DrawSortedFaces(commandBuffer, *renderPass);
        DrawSortedFaces(commandBuffer, *renderPass, true);
        DrawDebris(commandBuffer, *renderPass, DebrisPass::Blended);
        DrawGunFlashes(commandBuffer, *renderPass);
        commandBuffer.EndRenderPass();
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
        _nearPlane = view.Camera.NearPlane;
        _farPlane = view.Camera.FarPlane;
        _time = (float)environment.Frame;
        _isShadowMapActive = environment.IsShadowMapEnabled && (environment.ShadowLight != nullptr);
        _isPlayerShadowOnly = environment.IsPlayerShadowOnly;
        _videoSprite = environment.VideoSprite;

        auto frameVertexUniforms = FrameVertexUniforms{};
        frameVertexUniforms.ViewProjection = view.Camera.View * projection;
        _viewProjection = frameVertexUniforms.ViewProjection;
        _view = view.Camera.View;
        _projection = projection;
        _atmosphericSkyUniforms = environment.AtmosphericSkyUniforms;
        _atmosphericSkyUniformsSize = environment.AtmosphericSkyUniformsSize;
        frameVertexUniforms.CameraPosition = environment.CameraPosition;
        frameVertexUniforms.FogMinDistance = environment.FogMinDistance;
        frameVertexUniforms.FogMaxDistance = environment.FogMaxDistance;
        frameVertexUniforms.Frame = environment.Frame;
        frameVertexUniforms.InterpolatedFrame = environment.InterpolatedFrame;
        frameVertexUniforms.CameraUnderwater = environment.IsCameraUnderwater ? 1 : 0;
        commandBuffer->PushUniforms(RHI::ShaderStage::Vertex, 0, &frameVertexUniforms, sizeof(frameVertexUniforms));

        auto framePixelUniforms = FramePixelUniforms{};
        framePixelUniforms.FogColor = environment.FogColor;
        framePixelUniforms.PixelCameraPosition = environment.CameraPosition;
        framePixelUniforms.CameraDirection = view.Camera.WorldDirection;
        framePixelUniforms.AmbientOcclusionExponent = GetAmbientOcclusionExponent();
        framePixelUniforms.View = view.Camera.View;
        framePixelUniforms.InverseViewportSize = Vector2(1.0f / width, 1.0f / height);
        PrepareCaustics(framePixelUniforms);
        commandBuffer->PushUniforms(RHI::ShaderStage::Pixel, 0, &framePixelUniforms, sizeof(framePixelUniforms));

        auto fogBulbUniforms = GetFogBulbUniforms(view);
        commandBuffer->PushUniforms(RHI::ShaderStage::Vertex, 3, &fogBulbUniforms, sizeof(fogBulbUniforms));

        // Every uniform slot a shader declares must hold data of at least its size before the first draw of the
        // command buffer: SDL GPU on Vulkan otherwise binds a stale or undersized buffer, which can lose the device.
        auto defaultDrawVertexUniforms = DrawVertexUniforms{ NO_VALUE };
        auto defaultSkyVertexUniforms = SkyVertexUniforms{};
        auto defaultDrawPixelUniforms = DrawPixelUniforms{};
        commandBuffer->PushUniforms(RHI::ShaderStage::Vertex, 1, &defaultDrawVertexUniforms, sizeof(defaultDrawVertexUniforms));
        commandBuffer->PushUniforms(RHI::ShaderStage::Vertex, 2, &defaultSkyVertexUniforms, sizeof(defaultSkyVertexUniforms));
        commandBuffer->PushUniforms(RHI::ShaderStage::Pixel, 1, &defaultDrawPixelUniforms, sizeof(defaultDrawPixelUniforms));

        CollectMaterials(environment.InterpolationFactor);
        CollectDecals(view);
        CollectObjects(view, environment.InterpolationFactor);
        CollectSprites(view);
        CollectSkySprites(view, environment);
        CollectSortedFaces(view, environment.InterpolationFactor);
        CollectMirroredSorted(view);

        UploadFrameData(*commandBuffer, environment);

        PrepareShadows(environment, environment.InterpolationFactor);
        _hasDistortion = HasDistortionDraws(view);
        _isDofActive = IsDOFActive(environment);

        // The shadow uniforms are the largest block of pixel slot 2, so they also cover the passes before the scene.
        commandBuffer->PushUniforms(RHI::ShaderStage::Pixel, 2, &_shadowUniforms, sizeof(_shadowUniforms));

        _frameView = &view;
        _frameEnvironment = &environment;
        _frameVertexUniforms = frameVertexUniforms;

        for (const auto& pass : FRAME_PASSES)
        {
            if (pass.IsEnabled != nullptr && !(this->*pass.IsEnabled))
                continue;

            commandBuffer->PushDebugGroup(pass.Name);
            (this->*pass.Execute)(*commandBuffer);
            commandBuffer->PopDebugGroup();
        }

        _frameView = nullptr;
        _frameEnvironment = nullptr;
        commandBuffer->PopDebugGroup();
        _frameStats.CpuTime = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - startTime).count();
        commandBuffer->Submit();
        _hasScene = true;
        _isFramePending = true;

        if (!_screenshotPath.empty())
        {
            TENLog(fmt::format("Next renderer environment: fog color ({:.2f}, {:.2f}, {:.2f}), fog {:.1f}-{:.1f}, frame {}.",
                environment.FogColor.x, environment.FogColor.y, environment.FogColor.z, environment.FogMinDistance, environment.FogMaxDistance, environment.Frame), LogLevel::Info);

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
        }
    }

    // Composes the presented frame: the last scene frame faded by the UI, then the 2D layer recorded by the legacy
    // renderer. Menu frames that render no scene reuse the last one as their background.
    void NextRenderer::FinishFrame(const UiFrame& ui)
    {
        if (_device == nullptr)
            return;

        int width = _windowWidth;
        int height = _windowHeight;
        if (width <= 0 || height <= 0)
            return;

        auto lock = std::lock_guard<std::mutex>(_frameMutex);
        if (!EnsureRenderTargets(width, height))
            return;

        // Image files load before the frame command buffer: their upload waits for the device.
        LoadOverlayImages(ui);

        auto startTime = std::chrono::steady_clock::now();
        auto* commandBuffer = _device->AcquireCommandBuffer();
        if (commandBuffer == nullptr)
            return;

        bool hasScene = _isFramePending;
        if (!hasScene)
            _frameStats = NextFrameStats{};

        _isFramePending = false;

        commandBuffer->PushDebugGroup("Finish frame");
        _uploadRing.Begin(*commandBuffer->BeginCopyPass());

        // Frames without a scene, such as exclusive video playback, upload the video frame before the overlay uses it.
        if (!hasScene)
            UploadVideoFrame(ui.VideoFrame, ui.VideoWidth, ui.VideoHeight);

        CollectOverlay(ui);
        UploadOverlayData(hasScene);
        UploadImGuiData(ui);
        _uploadRing.End();
        commandBuffer->EndCopyPass();

        RenderOverlay(*commandBuffer, ui);
        RenderMenuPostProcess(*commandBuffer, ui);
        DrawImGui(*commandBuffer, ui);

        if (!_device->RequiresWindowThreadPresent())
        {
            auto* swapchainTexture = commandBuffer->AcquireSwapchainTexture();
            if (swapchainTexture != nullptr)
                commandBuffer->BlitTexture(_finalTarget.get(), swapchainTexture);
        }

        if (!_screenshotPath.empty())
        {
            _readback.Record(*_device, *commandBuffer->BeginCopyPass(), _finalTarget.get(), _screenshotPath);
            commandBuffer->EndCopyPass();
            _screenshotPath.clear();
        }

        commandBuffer->PopDebugGroup();

        // Submit is excluded: on DX11 it also presents and may wait for vsync.
        _frameStats.CpuTime += std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - startTime).count();
        _readback.Submit(*commandBuffer);
        _readback.Poll(*_device, false);

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

        // Acquisition may wait for vsync, so the frame lock is taken after it. Submission stays inside the lock:
        // a frame submitted by the game thread between this blit's recording and submission would execute out of
        // the recorded order, with stale texture layouts, which loses the Vulkan device.
        commandBuffer->PushDebugGroup("Present");
        auto* swapchainTexture = commandBuffer->AcquireSwapchainTexture();

        auto lock = std::lock_guard<std::mutex>(_frameMutex);
        if (swapchainTexture != nullptr)
        {
            if (_finalTarget != nullptr)
                commandBuffer->BlitTexture(_finalTarget.get(), swapchainTexture);

            _isFrameReady = false;
        }

        commandBuffer->PopDebugGroup();
        commandBuffer->Submit();
    }
}
