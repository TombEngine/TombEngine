#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

#include "Game/Animation/Animation.h"
#include "Game/control/control.h"
#include "Game/effects/hair.h"
#include "Game/effects/Decal.h"
#include "Game/effects/weather.h"
#include "Game/Lara/lara.h"
#include "Game/Setup.h"
#include "Objects/game_object_ids.h"
#include "Renderer/Backend/GpuTypes.h"
#include "Renderer/Backend/TextureLoader.h"
#include "Renderer/RendererUtils.h"
#include "Renderer/Scene/RenderView.h"
#include "Renderer/RHI/MemoryTracker.h"
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
#include "Specific/Benchmark.h"
#include "Specific/configuration.h"
#include "Specific/level.h"
#include "Specific/Structures/MaterialData.h"

using namespace TEN::Renderer::RHI;
using namespace TEN::Renderer::Structures;
using TEN::Renderer::Structures::Vertex;

namespace TEN::Renderer::Backend
{
    constexpr auto INITIAL_INSTANCE_CAPACITY = 4096;
    constexpr auto UPLOAD_RING_INITIAL_SIZE  = 4 * 1024 * 1024;

    SceneRenderer g_SceneRenderer;

    // While a background video plays, video sets show the frame also as emissive if the material has an emissive map.
    static bool HasEmissiveMaterial(int materialIndex)
    {
        return (materialIndex >= 0 && materialIndex < (int)g_Level.Materials.size() && g_Level.Materials[materialIndex].HasEmissiveMap);
    }

    bool SceneRenderer::Initialize(SDL_Window* window, GraphicsApi backend, const std::string& gameDirectory, bool preferLowPowerGpu)
    {
        _device = CreateDevice(backend, window, DEBUG_BUILD, preferLowPowerGpu);
        if (_device == nullptr)
            return false;

        _window = window;
        _shaderDirectory = gameDirectory + "Shaders/Bin";
        _textureCacheDirectory = gameDirectory + "Cache/Textures";

        int width = 0;
        int height = 0;
        SDL_GetWindowSizeInPixels(window, &width, &height);
        _windowWidth = width;
        _windowHeight = height;

        SetVsyncEnabled(_isVsyncEnabled);
        _uploadRing.Initialize(*_device, UPLOAD_RING_INITIAL_SIZE);
        _targetPool.Initialize(*_device);
        if (!CreatePipelines())
        {
            TENLog("Scene renderer: pipeline creation failed.", LogLevel::Error);
            Shutdown();
            return false;
        }

        TENLog(fmt::format("Scene renderer initialized on {}, window {}x{}.", _device->GetDeviceName(), width, height), LogLevel::Info);
        return true;
    }

    void SceneRenderer::Shutdown()
    {
        if (_device == nullptr)
            return;

        _device->WaitIdle();
        _readback.Poll(*_device, true);
        _readback.Clear();
        _frameFences = {};
        LogMemoryUsage();
        _roomTextures.clear();
        _animatedTextures.clear();
        _staticTextures.clear();
        _moveableTextures.clear();
        _geometry = {};
        for (auto& section : _geometrySections)
            section = {};

        _instanceBuffer = {};
        _lightBuffer = {};
        _lightClusterBuffer = {};
        _boneBuffer = {};
        _instanceIndexBuffer = nullptr;
        _animatedFramesBuffer = nullptr;
        _animatedSetsBuffer = nullptr;
        LogGpuTimings();

        ClearPipelineCaches();

        for (auto& variants : _pixelShaderVariants)
        {
            for (auto& shader : variants)
                shader = nullptr;
        }

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
        _debugVertexBuffer = {};
        _dustStormPipeline = nullptr;
        _dustStormVertexShader = nullptr;
        _dustStormPixelShader = nullptr;
        _godRayPipeline = nullptr;
        _godRayCompositePipeline = nullptr;
        _godRayVertexShader = nullptr;
        _godRayPixelShader = nullptr;
        _godRayCompositePixelShader = nullptr;
        _cloudRaymarchPipeline = nullptr;
        _cloudCompositePipeline = nullptr;
        _cloudOcclusionPipeline = nullptr;
        _cloudVertexShader = nullptr;
        _cloudRaymarchPixelShader = nullptr;
        _cloudCompositePixelShader = nullptr;
        _cloudOcclusionPixelShader = nullptr;
        _linearWrapSampler = nullptr;
        _cloudNoise3D = nullptr;
        _cloudWorley2D = nullptr;
        for (int layer = 0; layer < CLOUD_LAYER_COUNT; layer++)
        {
            _cloudTargets[layer] = nullptr;
            _cloudHistoryTargets[layer] = nullptr;
            _cloudOcclusionTargets[layer] = nullptr;
        }

        _debugLinePipeline = nullptr;
        _debugTrianglePipeline = nullptr;
        _debugVertexShader = nullptr;
        _debugPixelShader = nullptr;
        _skyPage = {};
        _textureArrayGroups.clear();
        for (auto& map : _defaultMapArrays)
            map = nullptr;

        _videoArrayTexture = nullptr;
        _skyVertexBuffer = nullptr;
        _skyIndexBuffer = nullptr;
        _skyVertexShader = nullptr;
        _skyPixelShader = nullptr;
        _horizonMaskPixelShader = nullptr;
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
        _upscaleSource = nullptr;
        _easuPipeline = nullptr;
        _rcasPipeline = nullptr;
        _easuPixelShader = nullptr;
        _rcasPixelShader = nullptr;
        _fxaaPipeline = nullptr;
        _fxaaPixelShader = nullptr;
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
        _skyDiscPipeline = nullptr;
        _auroraPipeline = nullptr;
        _auroraHalfPipeline = nullptr;
        _auroraUpscalePipeline = nullptr;
        _skyDiscPixelShader = nullptr;
        _auroraPixelShader = nullptr;
        _skyCopyPixelShader = nullptr;
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
        _skySpritesPixelShader = nullptr;
        _spriteCornerBuffer = nullptr;
        _linearClampSampler = nullptr;
        _spriteBuffer = {};
        _spriteTextures.clear();
        _device = nullptr;
    }

    bool SceneRenderer::IsActive() const
    {
        return (_device != nullptr);
    }

    // With vertical sync, Vulkan presents in mailbox mode where the driver allows it: in a window, the immediate mode of
    // some drivers (NVIDIA) adds a copy of the image to each present, which mailbox avoids without tearing, and the game
    // thread is paced to the refresh rate (Renderer::PaceFrame). Without vertical sync the immediate mode is used: in a
    // window the compositor takes one mailbox image per refresh, which capped a 60 Hz display at about 120 fps.
    // DX11 tears without vertical sync: its flip model is capped at the refresh rate otherwise.
    void SceneRenderer::SetVsyncEnabled(bool enabled)
    {
        _isVsyncEnabled = enabled;
        if (_device == nullptr)
            return;

        auto mode = enabled ? PresentMode::Vsync : PresentMode::Immediate;
        if (enabled && _device->RequiresWindowThreadPresent() && _device->SupportsPresentMode(PresentMode::Mailbox))
            mode = PresentMode::Mailbox;

        _device->SetPresentMode(mode);
    }

    bool SceneRenderer::IsVsyncEnabled() const
    {
        return _isVsyncEnabled;
    }

    bool SceneRenderer::IsPresentedOnWindowThread() const
    {
        return (_device != nullptr && _device->RequiresWindowThreadPresent());
    }

    // Called by a thread that stops rendering, so the OpenGL context can become current on the next one.
    void SceneRenderer::ReleaseThread()
    {
        if (_device != nullptr)
            _device->ReleaseThread();
    }

    // Anisotropic sampler of the material maps. With a render scale below 1, its mip bias keeps the texture detail
    // of the window resolution.
    bool SceneRenderer::CreateMaterialSampler()
    {
        auto samplerDesc = SamplerDesc{};
        samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = AddressMode::ClampToEdge;
        samplerDesc.MaxAnisotropy = 16;
        samplerDesc.MipLodBias = _materialLodBias;
        samplerDesc.Name = "AnisotropicClamp";
        _anisotropicSampler = _device->CreateSampler(samplerDesc);
        return (_anisotropicSampler != nullptr);
    }

    // Pipelines created on first use.
    void SceneRenderer::ClearPipelineCaches()
    {
        for (auto& shaderPipelines : _scenePipelines)
        {
            for (auto& variants : shaderPipelines)
            {
                for (auto& pipeline : variants)
                    pipeline = nullptr;
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
            auto candidate = directory / "TombEngine" / "Shaders";
            if (std::filesystem::exists(candidate / "CompileShaders.ps1"))
                return candidate;

            directory = directory.parent_path();
        }

        return {};
    }

    // Development builds only: recompiles the changed shader sources with the build script, then recreates every
    // shader and pipeline. The previous shaders stay in use when compilation fails.
    void SceneRenderer::ReloadShaders()
    {
        if (!DEBUG_BUILD || _device == nullptr)
            return;

        auto sourceDirectory = FindShaderSourceDirectory();
        if (sourceDirectory.empty())
        {
            TENLog("Scene renderer: shader sources not found, reload skipped.", LogLevel::Warning);
            return;
        }

        auto command = "powershell -ExecutionPolicy Bypass -File \"" + (sourceDirectory / "CompileShaders.ps1").string() +
                       "\" -OutputDir \"" + _shaderDirectory + "\" -DebugInfo";
        if (std::system(command.c_str()) != 0)
        {
            TENLog("Scene renderer: shader compilation failed, previous shaders kept.", LogLevel::Error);
            return;
        }

        auto lock = std::lock_guard<std::mutex>(_frameMutex);
        _device->WaitIdle();
        ClearPipelineCaches();

        bool isReloaded = CreatePipelines();
        TENLog(isReloaded ? "Scene renderer: shaders reloaded." : "Scene renderer: shader reload failed.", isReloaded ? LogLevel::Info : LogLevel::Error);
    }

    // Room and object draws without alpha test use pixel shaders without clip, which keep early depth testing. Texture
    // groups without some material maps use variants where those maps are constants, without samples or branches.
    bool SceneRenderer::LoadPixelShaderVariants()
    {
        struct VariantShader
        {
            SceneShader Shader     = SceneShader::Rooms;
            const char* File       = nullptr;
            const char* EntryPoint = nullptr;
        };

        constexpr VariantShader VARIANT_SHADERS[] =
        {
            { SceneShader::Rooms, "Rooms", "PS" },
            { SceneShader::RoomsPrepass, "Rooms", "PSDepthNormal" },
            { SceneShader::Objects, "Objects", "PS" },
            { SceneShader::ObjectsMirrored, "Objects", "PS" },
            { SceneShader::ObjectsPrepass, "Objects", "PSDepthNormal" }
        };

        // Same order as the options of the TEN_SHADER lines, which name the compiled variants.
        constexpr const char* OPTION_NAMES[] = { "NO_ALPHA_TEST", "NO_NORMAL_MAP", "NO_ORSH_MAP", "NO_EMISSIVE_MAP" };

        for (const auto& entry : VARIANT_SHADERS)
        {
            for (int variant = 1; variant < PIXEL_VARIANT_COUNT; variant++)
            {
                auto name = std::string();
                for (int option = 0; option < (int)std::size(OPTION_NAMES); option++)
                {
                    if (variant & (1 << option))
                        name += (name.empty() ? "" : ".") + std::string(OPTION_NAMES[option]);
                }

                _pixelShaderVariants[(int)entry.Shader][variant] = LoadShader(*_device, _shaderDirectory, entry.File, entry.EntryPoint, name);
                if (_pixelShaderVariants[(int)entry.Shader][variant] == nullptr)
                    return false;
            }
        }

        return true;
    }

    bool SceneRenderer::CreatePipelines()
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
        _horizonMaskPixelShader = LoadShader(*_device, _shaderDirectory, "Sky", "PSHorizonMask");
        if (_skyVertexShader == nullptr || _skyPixelShader == nullptr || _horizonMaskPixelShader == nullptr || !CreateSkyGeometry())
            return false;

        if (!CreateSpriteResources() || !CreateSSAOResources() || !CreatePostProcessResources() || !CreateShadowResources() || !CreateGlowResources() || !CreateDOFResources() || !CreateSMAAResources() || !CreateUpscaleResources() ||
            !CreateSkyReflectionResources() || !CreateAtmosphereResources() || !CreateOverlayResources(TEN::Renderer::Utils::GetAssetPath("Textures/Font.spritefont")) ||
            !CreateDebugPrimitiveResources() || !CreateCloudResources() || !CreateGodRayResources() || !CreateDustStormResources())
            return false;

        if (!LoadPixelShaderVariants())
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

        CreateMaterialSampler();

        // Defaults of the frontend for atlas pages without material maps.
        constexpr unsigned char WHITE_PIXEL[4]       = { 255, 255, 255, 255 };
        constexpr unsigned char FLAT_NORMAL_PIXEL[4] = { 128, 128, 255, 255 };
        constexpr unsigned char ORSH_PIXEL[4]        = { 255, 255, 0, 255 };
        constexpr unsigned char BLACK_PIXEL[4]       = { 0, 0, 0, 0 };

        _whiteTexture = CreateSolidTexture(WHITE_PIXEL, "White");
        _flatNormalTexture = CreateSolidTexture(FLAT_NORMAL_PIXEL, "FlatNormal");
        _defaultORSHTexture = CreateSolidTexture(ORSH_PIXEL, "DefaultORSH");
        _blackTexture = CreateSolidTexture(BLACK_PIXEL, "Black");

        // The same defaults as one layer arrays, for the material map slots of level pages.
        const unsigned char* DEFAULT_MAP_PIXELS[(int)MaterialMap::Count] = { WHITE_PIXEL, FLAT_NORMAL_PIXEL, ORSH_PIXEL, BLACK_PIXEL };
        for (int map = 0; map < (int)MaterialMap::Count; map++)
        {
            _defaultMapArrays[map] = CreateSolidTexture(DEFAULT_MAP_PIXELS[map], "DefaultMap[" + std::to_string(map) + "]", TextureType::Texture2DArray);
            if (_defaultMapArrays[map] == nullptr)
                return false;
        }

        return (_anisotropicSampler != nullptr && _whiteTexture != nullptr && _flatNormalTexture != nullptr &&
                _defaultORSHTexture != nullptr && _blackTexture != nullptr);
    }

    std::unique_ptr<ITexture> SceneRenderer::CreateSolidTexture(const unsigned char color[4], const std::string& name, TextureType type)
    {
        auto desc = TextureDesc{};
        desc.Type = type;
        desc.Name = name;
        auto texture = _device->CreateTexture(desc);
        if (texture != nullptr)
            UploadTexture(texture.get(), color, 4);

        return texture;
    }

    // Synchronous upload of the first mip level, for small textures created at initialization.
    void SceneRenderer::UploadTexture(ITexture* texture, const void* data, int size)
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
        region.Depth = (texture->GetDesc().Type == TextureType::Texture3D) ? texture->GetDesc().DepthOrLayers : 1;
        copyPass->UploadToTexture(TextureTransferInfo{ transferBuffer.get(), 0, 0, 0 }, region, false);
        commandBuffer->EndCopyPass();
        commandBuffer->Submit();
        _device->WaitIdle();
    }

    // Blend state of each blend mode.
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

    // Pipelines are created on first use per shader, blend mode and pixel shader variant; opaque and alpha test blending
    // share one. Shaders without variants always use their alpha tested pixel shader with every map.
    IGraphicsPipeline* SceneRenderer::GetScenePipeline(SceneShader shader, BlendMode blendMode, bool isAlphaTested, int missingMaps, bool isAlphaEdge)
    {
        // Alpha edges blend like alpha blending, in a slot of their own: the slot of no blend mode.
        constexpr auto ALPHA_EDGE_MODE_INDEX = 7;

        int modeIndex = (blendMode == BlendMode::AlphaTest) ? (int)BlendMode::Opaque : std::clamp((int)blendMode, 0, 15);
        if (isAlphaEdge)
            modeIndex = ALPHA_EDGE_MODE_INDEX;

        int variant = (isAlphaTested ? 0 : PIXEL_VARIANT_NO_ALPHA_TEST) | (missingMaps << PIXEL_VARIANT_MAP_SHIFT);
        if (variant >= PIXEL_VARIANT_COUNT || _pixelShaderVariants[(int)shader][variant] == nullptr)
            variant = 0;

        auto& pipeline = _scenePipelines[(int)shader][modeIndex][variant];
        if (pipeline != nullptr)
            return pipeline.get();

        bool isObjects = (shader == SceneShader::Objects || shader == SceneShader::ObjectsPrepass);
        bool isHorizonMask = (shader == SceneShader::SkyHorizonMask);
        bool isParaboloid = (shader == SceneShader::SkyParaboloid);
        bool isSky = (shader == SceneShader::Sky) || isParaboloid || isHorizonMask;
        bool isSprites = (shader == SceneShader::Sprites || shader == SceneShader::SkySprites);
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
            _skyVertexShader.get(), _inventoryVertexShader.get(), _skyVertexShader.get(), _spritesVertexShader.get()
        };
        IShader* pixelShaders[(int)SceneShader::Count] =
        {
            _roomsPixelShader.get(), _objectsPixelShader.get(), _skyPixelShader.get(), _spritesPixelShader.get(),
            _roomsPrepassPixelShader.get(), _objectsPrepassPixelShader.get(), _objectsShadowPixelShader.get(), _objectsPixelShader.get(), _objectsPixelShader.get(),
            _skyPixelShader.get(), _inventoryPixelShader.get(), _horizonMaskPixelShader.get(), _skySpritesPixelShader.get()
        };
        const char* names[(int)SceneShader::Count] =
        {
            "Rooms", "Objects", "Sky", "Sprites", "RoomsPrepass", "ObjectsPrepass", "ObjectsShadow", "ObjectsDebris", "ObjectsMirrored", "SkyParaboloid", "Inventory", "SkyHorizonMask", "SkySprites"
        };

        auto desc = GraphicsPipelineDesc{};
        desc.VertexShader = vertexShaders[(int)shader];
        desc.PixelShader = (variant == 0) ? pixelShaders[(int)shader] : _pixelShaderVariants[(int)shader][variant].get();
        desc.Layout.NumBuffers = (isObjects || isSprites) ? 2 : 1;
        // Debris has one instance per triangle, so its instance index comes from a per vertex stream.
        desc.Layout.Buffers[1] = VertexBufferLayout{ 1, (int)sizeof(unsigned int), isDebris ? VertexInputRate::Vertex : VertexInputRate::Instance };
        if (isDebris)
            desc.Rasterizer.Cull = RHI::CullMode::None;
        else if (isMirrored)
            desc.Rasterizer.Cull = RHI::CullMode::Front;
        else if (isParaboloid)
            desc.Rasterizer.Cull = RHI::CullMode::None;

        // Debris is built every frame from unpacked vertices; level geometry and the sky use the packed vertices.
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

        // The prepass writes view normals, emissive and a copy of the depth, the inventory color and emissive.
        desc.NumColorTargets = isPrepass ? 3 : (isInventory ? 2 : 1);
        desc.ColorTargets[0].Format = (isPrepass || isParaboloid) ? PixelFormat::RGBA8Unorm :
            (isShadow ? PixelFormat::R32Float : (isHorizonMask ? PixelFormat::R11G11B10Float : _device->GetSwapchainFormat()));
        desc.ColorTargets[0].Blend = GetBlendState((isPrepass || isShadow) ? BlendMode::Opaque : (isAlphaEdge ? BlendMode::AlphaBlend : (BlendMode)modeIndex));
        desc.ColorTargets[1].Format = PixelFormat::RGBA8Unorm;
        desc.ColorTargets[1].Blend = GetBlendState(BlendMode::Opaque);
        desc.ColorTargets[2].Format = PixelFormat::R32Float;
        desc.ColorTargets[2].Blend = GetBlendState(BlendMode::Opaque);
        // Distortion accumulates in a half resolution target without depth; the post-process rejects occluded texels.
        bool isDistortion = (modeIndex == (int)BlendMode::Distortion);
        desc.DepthStencil.EnableDepthWrite = ((modeIndex == (int)BlendMode::Opaque) && !isSky) || isInventory;
        // The sky is on the far plane and tested against the prepass depth; its reflection targets have no depth.
        desc.DepthStencil.EnableDepthTest = (!isSky || shader == SceneShader::Sky) && !isDistortion;

        // Opaque rooms and objects of the scene pass are the same draws as the prepass, so they shade only the texels
        // whose depth the prepass kept.
        if ((shader == SceneShader::Rooms || shader == SceneShader::Objects) && modeIndex == (int)BlendMode::Opaque)
        {
            desc.DepthStencil.EnableDepthWrite = false;
            desc.DepthStencil.DepthCompare = CompareOp::Equal;
        }

        // Texels the prepass kept are already drawn by the opaque pass: early depth rejects them and only the edges shade.
        if (isAlphaEdge)
            desc.DepthStencil.DepthCompare = CompareOp::Less;

        desc.DepthFormat = (isDistortion || isParaboloid || isHorizonMask) ? PixelFormat::Unknown : PixelFormat::D32Float;
        desc.Name = std::string(names[(int)shader]) + ".BlendMode" + std::to_string(modeIndex) + ((variant == 0) ? "" : ".Variant" + std::to_string(variant));

        pipeline = _device->CreateGraphicsPipeline(desc);
        if (pipeline == nullptr)
            TENLog("Scene renderer: cannot create pipeline " + desc.Name, LogLevel::Error);

        return pipeline.get();
    }

    // Targets sampled by later passes are cleared once at creation: some are written only when their feature is
    // active, and sampling a texture never written leaves it in an undefined layout on Vulkan. The frame target is cleared
    // only when it is new: it holds the last scene frame, which menus show as their background.
    void SceneRenderer::ClearSampledTargets(bool includeFrameTargets)
    {
        ITexture* targets[] =
        {
            _distortionTarget.get(), _emissiveTarget.get(), _glowTarget.get(), _reflectionTarget.get(), _dofTargets[0].get(), _dofTargets[1].get(),
            _antialiasingSource.get(), includeFrameTargets ? _colorTarget.get() : nullptr, _skyReflectionTarget.get()
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

    // Frame targets have the window resolution; scene targets have the render resolution, which the render scale lowers.
    // A render scale change keeps the frame targets, so the menu background of the last scene frame survives it.
    bool SceneRenderer::EnsureRenderTargets(int width, int height)
    {
        int renderWidth = GetRenderSize(width);
        int renderHeight = GetRenderSize(height);
        bool isFrameCurrent = (_colorTarget != nullptr && _colorTarget->GetDesc().Width == width && _colorTarget->GetDesc().Height == height);
        bool isSceneCurrent = (_sceneTarget != nullptr && _sceneTarget->GetDesc().Width == renderWidth && _sceneTarget->GetDesc().Height == renderHeight);
        if (isFrameCurrent && isSceneCurrent)
            return true;

        _targetPool.Clear();

        bool isCreated = (isFrameCurrent || CreateFrameTargets(width, height)) && (isSceneCurrent || CreateSceneTargets(renderWidth, renderHeight, width));
        if (isCreated)
        {
            ClearSampledTargets(!isFrameCurrent);
            ReserveTransientTargets();
        }

        return isCreated;
    }

    // Pooled targets created on first use, after a level filled the video memory, may be placed in system memory
    // on Vulkan, which has no residency management; reading them there costs several milliseconds per pass.
    // The full screen ones are therefore created with the frame targets.
    void SceneRenderer::ReserveTransientTargets()
    {
        auto edgesDesc = TextureDesc{};
        auto weightsDesc = TextureDesc{};
        auto stencilDesc = TextureDesc{};
        GetSMAATargetDescs((_isUpscaleActive ? _upscaleSource : _colorTarget)->GetDesc(), edgesDesc, weightsDesc, stencilDesc);
        _targetPool.Reserve(edgesDesc);
        _targetPool.Reserve(weightsDesc);
        if (_smaaStencilFormat != PixelFormat::Unknown)
            _targetPool.Reserve(stencilDesc);

        _targetPool.Reserve(_sceneTarget->GetDesc());
        _targetPool.Reserve(_dofTargets[0]->GetDesc());
        _targetPool.Reserve(_ssaoTarget->GetDesc());
        _targetPool.Reserve(_glowTarget->GetDesc());
    }

    bool SceneRenderer::CreateFrameTargets(int width, int height)
    {
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
        return (_colorTarget != nullptr && _finalTarget != nullptr);
    }

    bool SceneRenderer::CreateSceneTargets(int width, int height, int windowWidth)
    {
        auto colorDesc = TextureDesc{};
        colorDesc.Width = width;
        colorDesc.Height = height;
        colorDesc.Format = _device->GetSwapchainFormat();
        colorDesc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        colorDesc.Name = "Scene.Color";
        _sceneTarget = _device->CreateTexture(colorDesc);

        // Distortion is low frequency: half resolution and no depth.
        auto distortionDesc = colorDesc;
        distortionDesc.Width = std::max(width / 2, 1);
        distortionDesc.Height = std::max(height / 2, 1);
        distortionDesc.Name = "Scene.Distortion";
        _distortionTarget = _device->CreateTexture(distortionDesc);

        colorDesc.Name = "PostProcess.Result";
        _antialiasingSource = _device->CreateTexture(colorDesc);

        // Below the window resolution, the post-processed and antialiased scene waits here for the upscale pass.
        _isUpscaleActive = (width != windowWidth);
        colorDesc.Name = "FSR.Source";
        _upscaleSource = _isUpscaleActive ? _device->CreateTexture(colorDesc) : nullptr;

        colorDesc.Format = PixelFormat::R32Float;
        colorDesc.Name = "Scene.DepthCopy";
        _sceneDepthCopy = _device->CreateTexture(colorDesc);

        auto depthDesc = TextureDesc{};
        depthDesc.Width = width;
        depthDesc.Height = height;
        depthDesc.Format = PixelFormat::D32Float;
        // Never sampled: a depth buffer read as a texture is decompressed at each transition on some GPUs (Intel).
        depthDesc.Usage = TextureUsage::DepthStencilTarget;
        depthDesc.Name = "Scene.Depth";
        _depthTarget = _device->CreateTexture(depthDesc);

        if (_sceneTarget == nullptr || _sceneDepthCopy == nullptr || _distortionTarget == nullptr || _antialiasingSource == nullptr ||
            _depthTarget == nullptr || (_isUpscaleActive && _upscaleSource == nullptr))
        {
            return false;
        }

        if (!EnsureSSAOTargets(width, height) || !EnsureGlowTargets(width, height) || !EnsureDOFTargets(width, height))
            return false;

        // Texture mips are biased by the scale, so the scene keeps the texture detail of the window resolution.
        float lodBias = _isUpscaleActive ? std::log2((float)width / (float)windowWidth) : 0.0f;
        if (lodBias == _materialLodBias)
            return true;

        _materialLodBias = lodBias;
        return CreateMaterialSampler();
    }

    void SceneRenderer::UploadBuffer(IBuffer* buffer, const void* data, int size, int offset)
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

    void SceneRenderer::LoadLevel(const LevelGeometry& geometry)
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

        // The previous level textures are released first, so the geometry gets the video memory they held.
        _textureArrayGroups.clear();
        _spriteTextures.clear();

        if (!CreateLevelGeometry(geometry))
            return;

        _hasEmissiveMaps = false;
        LoadLevelTextures();
        LoadAnimatedTextures(geometry);
        PrecompileLevelPipelines(geometry);

        _levelGeometry = geometry;

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

        TENLog("Scene renderer: room buckets by blend mode:" + bucketSummary, LogLevel::Info);

        float elapsed = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - startTime).count();
        TENLog(fmt::format("Scene renderer: level data uploaded in {:.1f} ms ({} room vertices, {} room textures, emissive maps: {}).",
            elapsed, geometry.RoomVertices->size(), _roomTextures.size(), _hasEmissiveMaps ? "yes" : "no"), LogLevel::Info);
    }

    // All animated texture sets live in two static storage buffers, so drawing an animated bucket only
    // pushes its set index; frames are selected in the vertex shader.
    void SceneRenderer::LoadAnimatedTextures(const LevelGeometry& geometry)
    {
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

    // Waterfall objects scroll vertically within the V range of their first triangle at a fixed speed. That range
    // only depends on the object, so each waterfall object gets a set with a single frame.
    void SceneRenderer::AddWaterfallSets(const LevelGeometry& geometry, std::vector<GpuAnimatedFrame>& frames, std::vector<GpuAnimatedSet>& sets)
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
    void SceneRenderer::CollectMaterials(float interpolationFactor)
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

    // The material table is uploaded at load and again only when scripts change material properties or types.
    void SceneRenderer::UploadMaterials()
    {
        if (_materialBuffer.Buffer != nullptr && _materialData == _uploadedMaterialData)
            return;

        UploadFrameStorage(_materialBuffer, _materialData, (int)sizeof(GpuMaterial), BufferUsage::GraphicsStorageRead, "Materials");
        _frameStats.MaterialUploads++;
        _uploadedMaterialData = _materialData;
    }

    void SceneRenderer::BindScenePixelStorage(IRenderPass& renderPass)
    {
        IBuffer* buffers[4] = { _lightBuffer.Buffer.get(), _materialBuffer.Buffer.get(), _decalBuffer.Buffer.get(), _lightClusterBuffer.Buffer.get() };
        renderPass.BindPixelStorageBuffers(0, buffers, 4);
    }

    SceneRenderer::BucketDrawInfo SceneRenderer::GetBucketDrawInfo(int roomNumber, bool hasDecals)
    {
        auto info = BucketDrawInfo{};
        info.IsWaterRoom = (g_Level.Rooms[roomNumber].flags & ENV_FLAG_WATER) ? 1 : 0;
        info.IsCausticsRoom = IsCausticsRoom(roomNumber) ? 1 : 0;
        info.IsOutdoorRoom = (g_Level.Rooms[roomNumber].flags & ENV_FLAG_SKYBOX) ? 1 : 0;

        auto it = _roomDecalSlices.find(roomNumber);
        if (hasDecals && it != _roomDecalSlices.end())
        {
            info.DecalOffset = (unsigned int)it->second.first;
            info.DecalCount = (unsigned int)it->second.second;
        }

        return info;
    }

    // Decals of the visible rooms.
    void SceneRenderer::CollectDecals(const RenderView& view)
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

    // Color map array of a page and its layer, for the sky shader.
    void SceneRenderer::BindPageColorMap(ICommandBuffer& commandBuffer, IRenderPass& renderPass, const TexturePage* page)
    {
        const auto* group = (page != nullptr) ? page->Group : nullptr;
        auto* colorMap = (group != nullptr && group->Maps[(int)MaterialMap::Color] != nullptr) ? group->Maps[(int)MaterialMap::Color].get() : _defaultMapArrays[(int)MaterialMap::Color].get();
        auto binding = TextureSamplerBinding{ colorMap, _anisotropicSampler.get() };
        renderPass.BindPixelSamplers(0, &binding, 1);

        auto pixelUniforms = DrawPixelUniforms{};
        pixelUniforms.TextureLayer = (page != nullptr) ? (unsigned int)page->Layer : 0;
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 1, &pixelUniforms, sizeof(pixelUniforms));
    }

    // Binds the texture arrays of the page group; the layer is selected by the draw uniforms.
    void SceneRenderer::BindTexturePage(IRenderPass& renderPass, const TexturePage* page, ITexture* colorOverride, ITexture* emissiveOverride)
    {
        auto getMap = [&](MaterialMap map)
        {
            const auto* group = (page != nullptr) ? page->Group : nullptr;
            return (group != nullptr && group->Maps[(int)map] != nullptr) ? group->Maps[(int)map].get() : _defaultMapArrays[(int)map].get();
        };

        auto* sampler = _anisotropicSampler.get();
        auto* ambientOcclusion = _isAmbientOcclusionActive ? _ssaoTarget.get() : _whiteTexture.get();
        auto* shadowMap = _isRenderingShadowMap ? _dummyShadowMap.get() : _shadowMap.get();
        auto* caustics = (_causticsTexture != nullptr) ? _causticsTexture : _whiteTexture.get();
        auto* reflections = _hasReflectiveMaterials ? _reflectionTarget.get() : _blackTexture.get();
        auto* skyReflections = _hasSkyReflectiveMaterials ? _skyReflectionTarget.get() : _dummyShadowMap.get();
        TextureSamplerBinding bindings[9] =
        {
            { (colorOverride != nullptr) ? colorOverride : getMap(MaterialMap::Color), sampler },
            { getMap(MaterialMap::Normal), sampler },
            { getMap(MaterialMap::ORSH), sampler },
            { (emissiveOverride != nullptr) ? emissiveOverride : getMap(MaterialMap::Emissive), sampler },
            { ambientOcclusion, _linearClampSampler.get() },
            { shadowMap, _pointClampSampler.get() },
            { caustics, sampler },
            { reflections, _linearClampSampler.get() },
            { skyReflections, _linearClampSampler.get() }
        };
        renderPass.BindPixelSamplers(0, bindings, 9);
    }

    // Positive for alpha test, negative for the soft edge pass (texels below the threshold), zero for none.
    float SceneRenderer::GetAlphaThreshold(const RendererBucket& bucket, const BucketDrawInfo& info)
    {
        if (info.IsAlphaEdgePass)
            return -ALPHA_TEST_THRESHOLD;

        if (info.IsForcedAlphaTest)
            return ALPHA_TEST_THRESHOLD;

        if (info.IsAlphaTestDisabled)
            return 0.0f;

        return (bucket.BlendMode == BlendMode::AlphaTest) ? ALPHA_TEST_THRESHOLD : 0.0f;
    }

    void SceneRenderer::BindBucket(ICommandBuffer& commandBuffer, IRenderPass& renderPass, const RendererBucket& bucket, const std::vector<TexturePage>& pages,
                                  BlendMode blendMode, unsigned int lightCount, const BucketDrawInfo& room, BucketBindState& state)
    {
        const auto& sourcePages = bucket.Animated ? _animatedTextures : pages;
        const auto* page = (bucket.Texture >= 0 && bucket.Texture < (int)sourcePages.size()) ? &sourcePages[bucket.Texture] : nullptr;

        auto pixelUniforms = DrawPixelUniforms{};
        pixelUniforms.AlphaThreshold = GetAlphaThreshold(bucket, room);
        pixelUniforms.BlendMode = (unsigned int)(room.IsAlphaEdgePass ? BlendMode::AlphaBlend : blendMode);
        pixelUniforms.LightCount = lightCount;
        pixelUniforms.MaterialIndex = (bucket.MaterialIndex >= 0 && bucket.MaterialIndex < (int)g_Level.Materials.size()) ? (unsigned int)bucket.MaterialIndex : 0;
        pixelUniforms.IsCausticsRoom = room.IsCausticsRoom;
        pixelUniforms.IsOutdoorRoom = room.IsOutdoorRoom;
        pixelUniforms.DecalOffset = room.DecalOffset;
        pixelUniforms.DecalCount = bucket.Animated ? 0 : room.DecalCount;
        pixelUniforms.TextureLayer = (page != nullptr) ? (unsigned int)page->Layer : 0;
        if (memcmp(&pixelUniforms, &state.PixelUniforms, sizeof(pixelUniforms)) != 0)
        {
            commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 1, &pixelUniforms, sizeof(pixelUniforms));
            state.PixelUniforms = pixelUniforms;
        }

        // The glow pass runs only when a draw of the frame can write emissive light.
        if (HasEmissiveMaterial(bucket.MaterialIndex))
            _hasVisibleEmissive = true;

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

        // Pages of the same group share their texture arrays: only the layer uniform changes between them.
        const auto* group = (page != nullptr) ? page->Group : nullptr;
        if (videoSet != NO_VALUE)
        {
            bool hasEmissive = HasEmissiveMaterial((int)pixelUniforms.MaterialIndex);
            BindTexturePage(renderPass, page, _videoArrayTexture.get(), hasEmissive ? _videoArrayTexture.get() : nullptr);
            state.IsGroupBound = false;
        }
        else if (!state.IsGroupBound || group != state.Group)
        {
            BindTexturePage(renderPass, page);
            state.Group = group;
            state.IsGroupBound = true;
        }
    }

    // Maps the texture group of a bucket lacks, as pixel shader variant bits. Pages of a group share their maps.
    int SceneRenderer::GetMissingMaps(const RendererBucket& bucket, const std::vector<TexturePage>& pages) const
    {
        const auto& sourcePages = bucket.Animated ? _animatedTextures : pages;
        const auto* page = (bucket.Texture >= 0 && bucket.Texture < (int)sourcePages.size()) ? &sourcePages[bucket.Texture] : nullptr;
        const auto* group = (page != nullptr) ? page->Group : nullptr;

        int missingMaps = 0;
        for (int map = (int)MaterialMap::Normal; map < (int)MaterialMap::Count; map++)
        {
            if (group == nullptr || group->Maps[map] == nullptr)
                missingMaps |= 1 << (map - (int)MaterialMap::Normal);
        }

        int materialIndex = (bucket.MaterialIndex >= 0 && bucket.MaterialIndex < (int)g_Level.Materials.size()) ? bucket.MaterialIndex : 0;
        if (GetVideoSet(bucket) != NO_VALUE && HasEmissiveMaterial(materialIndex))
            missingMaps &= ~(1 << ((int)MaterialMap::Emissive - (int)MaterialMap::Normal));

        return missingMaps;
    }

    // Draws are sorted by texture group, so the pipeline changes at most once per group in the state sorted passes.
    // Without ambient occlusion only the outdoor mask of the prepass normals is read, so the prepass skips normal maps.
    void SceneRenderer::BindPixelVariant(IRenderPass& renderPass, SceneShader shader, ScenePass pass, bool isAlphaTested, int missingMaps, int& boundMissingMaps)
    {
        if (pass == ScenePass::Prepass && !_isAmbientOcclusionActive)
            missingMaps |= 1; // NO_NORMAL_MAP.

        if (missingMaps == boundMissingMaps)
            return;

        renderPass.BindPipeline(GetScenePipeline(shader, GetPassBlendMode(pass), isAlphaTested, missingMaps, pass == ScenePass::AlphaEdges));
        boundMissingMaps = missingMaps;
    }

    BlendMode SceneRenderer::GetPassBlendMode(ScenePass pass)
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

    bool SceneRenderer::IsBucketInPass(const RendererBucket& bucket, ScenePass pass)
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
    bool SceneRenderer::IsAlphaTestVariantInPass(ScenePass pass, bool isAlphaTested)
    {
        if (pass == ScenePass::Prepass || pass == ScenePass::Opaque)
            return true;

        return (isAlphaTested == (pass == ScenePass::AlphaEdges));
    }

    const std::vector<SceneRenderer::TexturePage>& SceneRenderer::GetSourcePages(MeshSource source) const
    {
        switch (source)
        {
        case MeshSource::Rooms:
            return _roomTextures;

        case MeshSource::Statics:
            return _staticTextures;

        default:
            return _moveableTextures;
        }
    }

    // Draw state of a bucket, most expensive change first: alpha test variant, texture group, layer, material, source.
    unsigned long long SceneRenderer::GetBucketSortKey(MeshSource source, const RendererBucket& bucket) const
    {
        const auto& pages = bucket.Animated ? _animatedTextures : GetSourcePages(source);
        const auto* page = (bucket.Texture >= 0 && bucket.Texture < (int)pages.size()) ? &pages[bucket.Texture] : nullptr;

        auto isAlphaTested = (unsigned long long)(bucket.BlendMode == BlendMode::AlphaTest);
        auto group = (unsigned long long)((page != nullptr && page->Group != nullptr) ? (page->Group->Index + 1) : 0);
        auto layer = (unsigned long long)((page != nullptr) ? page->Layer : 0);
        auto material = (unsigned long long)std::clamp(bucket.MaterialIndex, 0, (int)USHRT_MAX);
        return (isAlphaTested << 60) | ((group & 0xFFF) << 44) | ((layer & 0xFFF) << 32) | (material << 16) | (unsigned long long)source;
    }

    void SceneRenderer::SortObjectDraws()
    {
        _objectDrawOrder.clear();
        for (int i = 0; i < (int)_objectDraws.size(); i++)
            _objectDrawOrder.push_back(DrawSortKey{ GetBucketSortKey(_objectDraws[i].Source, *_objectDraws[i].Bucket), i });

        SortDrawKeys(_objectDrawOrder, _drawSortScratch);
    }

    // Visible room buckets are listed once per frame in portal order, front to back, and sorted by draw state for
    // the passes whose order does not matter.
    void SceneRenderer::CollectRoomDraws(const RenderView& view)
    {
        _roomDraws.clear();
        _roomDrawOrder.clear();

        const auto& targetDesc = _sceneTarget->GetDesc();
        for (const auto* room : view.RoomsToDraw)
        {
            auto scissor = GetRoomScissor(*room, targetDesc.Width, targetDesc.Height);
            if (scissor.Width == 0 || scissor.Height == 0)
                continue;

            for (const auto& bucket : room->Buckets)
            {
                if (bucket.NumIndices == 0 || !IsRoomBucketVisible(bucket) || !IsRoomBucketInPortals(bucket, *room, view.Camera.ViewProjection))
                    continue;

                _roomDrawOrder.push_back(DrawSortKey{ GetBucketSortKey(MeshSource::Rooms, bucket), (int)_roomDraws.size() });
                _roomDraws.push_back(RoomDraw{ room, &bucket });
            }
        }

        SortDrawKeys(_roomDrawOrder, _drawSortScratch);
    }

    // Opaque, additive and distortion draws do not depend on their order: they are drawn sorted by state.
    bool SceneRenderer::IsStateSortedPass(ScenePass pass)
    {
        return (pass == ScenePass::Opaque || pass == ScenePass::Additive || pass == ScenePass::Distortion);
    }

    // Buckets without alpha test are drawn first, with the pixel shader without clip.
    void SceneRenderer::DrawRooms(ICommandBuffer& commandBuffer, IRenderPass& renderPass, ScenePass pass)
    {
        constexpr const char* GROUP_NAMES[] = { "Rooms prepass", "Rooms opaque", "Rooms alpha edges", "Rooms additive", "Rooms distortion" };

        commandBuffer.PushDebugGroup(GROUP_NAMES[(int)pass]);

        for (int variant = 0; variant < 2; variant++)
        {
            if (IsAlphaTestVariantInPass(pass, variant == 1))
                DrawRoomBuckets(commandBuffer, renderPass, pass, variant == 1);
        }

        commandBuffer.PopDebugGroup();
    }

    void SceneRenderer::DrawRoomBuckets(ICommandBuffer& commandBuffer, IRenderPass& renderPass, ScenePass pass, bool isAlphaTested)
    {
        auto shader = (pass == ScenePass::Prepass) ? SceneShader::RoomsPrepass : SceneShader::Rooms;
        renderPass.BindPipeline(GetScenePipeline(shader, GetPassBlendMode(pass), isAlphaTested, 0, pass == ScenePass::AlphaEdges));

        BindLevelGeometry(renderPass, MeshSource::Rooms, false);

        IBuffer* storageBuffers[2] = { _animatedFramesBuffer.get(), _animatedSetsBuffer.get() };
        renderPass.BindVertexStorageBuffers(0, storageBuffers, 2);

        BindScenePixelStorage(renderPass);
        auto state = BucketBindState{};

        // Rooms are clipped to their portal rectangle. Distortion uses a half resolution target.
        const auto& targetDesc = ((pass == ScenePass::Distortion) ? _distortionTarget : _sceneTarget)->GetDesc();
        bool isSorted = IsStateSortedPass(pass);

        int boundMissingMaps = 0;
        const RendererRoom* lastRoom = nullptr;
        auto roomInfo = BucketDrawInfo{};
        for (int i = 0; i < (int)_roomDraws.size(); i++)
        {
            const auto& draw = _roomDraws[isSorted ? _roomDrawOrder[i].Index : i];
            const auto& bucket = *draw.Bucket;
            if (!IsBucketInPass(bucket, pass))
                continue;

            if (draw.Room != lastRoom)
            {
                roomInfo = GetBucketDrawInfo(draw.Room->RoomNumber, pass != ScenePass::Prepass);
                roomInfo.IsAlphaEdgePass = (pass == ScenePass::AlphaEdges);
                renderPass.SetScissor(GetRoomScissor(*draw.Room, targetDesc.Width, targetDesc.Height));
                lastRoom = draw.Room;
            }

            if ((GetAlphaThreshold(bucket, roomInfo) != 0.0f) != isAlphaTested)
                continue;

            BindPixelVariant(renderPass, shader, pass, isAlphaTested, GetMissingMaps(bucket, _roomTextures), boundMissingMaps);
            BindBucket(commandBuffer, renderPass, bucket, _roomTextures, bucket.BlendMode, (unsigned int)_roomLightCount, roomInfo, state);
            DrawBucket(renderPass, MeshSource::Rooms, bucket, 1, 0);
        }

        renderPass.SetScissor(ScissorRect{ 0, 0, targetDesc.Width, targetDesc.Height });
    }

    // Same conversion: interpolated positions and spot ranges as cosines.
    GpuLight SceneRenderer::ToGpuLight(const RendererLight& light, float interpolationFactor, float fade)
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
    int SceneRenderer::AddLights(const std::vector<RendererLight*>& lights, float interpolationFactor, int roomNumber, int prevRoomNumber, float fade, int& lightOffset)
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
    void SceneRenderer::CollectObjects(const RenderView& view, float interpolationFactor)
    {
        _instanceData.clear();
        _instanceOrigins.clear();
        _lightData.clear();
        _objectDraws.clear();
        _shadowDraws.clear();

        // Rooms share the view light list.
        int roomLightOffset = 0;
        _roomLightCount = AddLights(view.LightsToDraw, interpolationFactor, NO_VALUE, NO_VALUE, 1.0f, roomLightOffset);
        BuildLightClusters(roomLightOffset, _roomLightCount & LIGHT_COUNT_MASK);

        _boneData.clear();

        for (const auto& [objectNumber, statics] : view.SortedStaticsToDraw)
        {
            if (statics.empty())
                continue;

            int objectIndex = Statics.GetIndex(objectNumber);
            if (objectIndex < 0 || objectIndex >= (int)_levelGeometry.StaticObjects->size())
                continue;

            const auto& object = (*_levelGeometry.StaticObjects)[objectIndex];
            if (!object.has_value() || object->ObjectMeshes.empty())
                continue;

            const auto* mesh = object->ObjectMeshes[0];
            int firstInstance = (int)(_instanceData.size() / sizeof(GpuObjectInstance));
            int instanceCount = 0;
            for (const auto* staticMesh : statics)
            {
                // Fading statics are drawn with the sorted faces only.
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

                const auto& object = (*_levelGeometry.MoveableObjects)[item->ObjectID];
                if (!object.has_value() || object->Hidden)
                    continue;

                const auto& nativeItem = g_Level.Items[item->ItemNumber];

                // Mirrors skip items with cleared bodies.
                bool isReflectable = !(nativeItem.Flags & IFLAG_CLEAR_BODY);
                _currentOrigin = InstanceOrigin{ item->RoomNumber, isReflectable ? InstanceCategory::Moveable : InstanceCategory::None };

                int lightOffset = 0;
                int lightCount = AddLights(item->LightsToDraw, interpolationFactor, item->RoomNumber, item->PrevRoomNumber, item->LightFade, lightOffset);
                lightCount |= GetShadowReceiverMask(item->ObjectID);

                int firstDraw = (int)_objectDraws.size();

                bool isFullSkin = (item->SkinIndex != NO_VALUE);
                auto waterfallSet = _waterfallSets.find(item->ObjectID);
                bool isShadowCaster = IsShadowCaster(item->ObjectID);

                // Rigid meshes of plain items are grouped by mesh with those of identical items. Skinned, scrolling, fading and
                // shadow casting items keep their own draws, which per item steps below adjust.
                bool isGrouped = !isFullSkin && waterfallSet == _waterfallSets.end() && item->Color.w >= ALPHA_BLEND_THRESHOLD && !isShadowCaster;

                // Full skin: the skin mesh with bind pose corrected bones, and only the rigid meshes not hidden by it.
                if (isFullSkin)
                {
                    Matrix skinBones[BONE_COUNT_MAX];
                    int boneCount = std::min((int)object->AnimationTransforms.size(), (int)BONE_COUNT_MAX);
                    for (int i = 0; i < boneCount; i++)
                        skinBones[i] = object->BindPoseTransforms[i] * item->InterpolatedAnimationTransforms[i];

                    const auto* skinMesh = (*_levelGeometry.Meshes)[item->SkinIndex];
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

                    const auto* mesh = (*_levelGeometry.Meshes)[item->MeshIndex[meshIndex]];
                    auto world = item->InterpolatedAnimationTransforms[meshIndex] * item->InterpolatedWorld;
                    if (isGrouped)
                    {
                        QueueMeshInstance(*mesh, world, item->Color, item->AmbientLight, mesh->LightMode, lightOffset, lightCount, item->RoomNumber, isReflectable);
                        continue;
                    }

                    int instance = AddInstance(world, item->Color, item->AmbientLight, mesh->LightMode, lightOffset, lightCount, 0, SKINNING_NONE);
                    AddMeshDraws(*mesh, instance);
                }

                if (waterfallSet != _waterfallSets.end())
                {
                    for (int i = firstDraw; i < (int)_objectDraws.size(); i++)
                        _objectDraws[i].AnimatedSetOverride = waterfallSet->second;
                }

                if (isShadowCaster)
                    MarkShadowCasterDraws(firstDraw, GetItemBounds(*item, 0.0f));

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

    int SceneRenderer::AddInstance(const Matrix& world, const Vector4& color, const Vector4& ambientLight, LightMode lightMode, int lightOffset, int lightCount, int boneOffset, int skinningMode)
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
        AppendInstance(instance, _currentOrigin);
        return index;
    }

    // Instances of outdoor rooms are flagged for the outdoor mask of the prepass.
    void SceneRenderer::AppendInstance(const GpuObjectInstance& instance, const InstanceOrigin& origin)
    {
        auto flaggedInstance = instance;
        if (origin.RoomNumber != NO_VALUE && (g_Level.Rooms[origin.RoomNumber].flags & ENV_FLAG_SKYBOX))
            flaggedInstance.LightMode |= INSTANCE_OUTDOOR_FLAG;

        const auto* bytes = (const unsigned char*)&flaggedInstance;
        _instanceData.insert(_instanceData.end(), bytes, bytes + sizeof(flaggedInstance));
        _instanceOrigins.push_back(origin);
    }

    int SceneRenderer::AddBones(const Matrix* bones, int count)
    {
        int offset = (int)(_boneData.size() / sizeof(Matrix));
        const auto* bytes = (const unsigned char*)bones;
        _boneData.insert(_boneData.end(), bytes, bytes + (count * sizeof(Matrix)));
        return offset;
    }

    // Opaque buckets of a fading object turn alpha blended, as in the frontend GetBlendModeFromAlpha, and are drawn
    // with the sorted faces only. Shadow casters are marked before.
    void SceneRenderer::RemoveFadedDraws(int firstDraw, float alpha)
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

    void SceneRenderer::AddMeshDraws(const RendererMesh& mesh, int firstInstance, int instanceCount)
    {
        for (const auto& bucket : mesh.Buckets)
        {
            if (bucket.NumIndices > 0)
                _objectDraws.push_back(ObjectDraw{ MeshSource::Moveables, &bucket, firstInstance, instanceCount });
        }
    }

    // Player meshes are skinned in the vertex shader against the animation bone palette: rigid parts and
    // classic joints blend per-vertex bone indices, the full skin uses bind pose corrected bones.
    // Hair units follow the player.
    void SceneRenderer::CollectPlayer(float interpolationFactor)
    {
        _hairInstances.fill(NO_VALUE);

        if (LaraItem == nullptr || Lara.Control.Look.OpticRange != 0)
            return;

        const auto& nativeItem = g_Level.Items[LaraItem->Index];
        if (nativeItem.Flags & IFLAG_INVISIBLE)
            return;

        const auto& laraObject = (*_levelGeometry.MoveableObjects)[ID_LARA];
        const auto* item = _levelGeometry.Items != nullptr ? &(*_levelGeometry.Items)[LaraItem->Index] : nullptr;
        if (!laraObject.has_value() || item == nullptr)
            return;

        int lightOffset = 0;
        int lightCount = AddLights(item->LightsToDraw, interpolationFactor, item->RoomNumber, item->PrevRoomNumber, item->LightFade, lightOffset);
        lightCount |= GetShadowReceiverMask(ID_LARA);
        int firstDraw = (int)_objectDraws.size();

        int boneCount = (int)laraObject->AnimationTransforms.size();
        int animationBones = AddBones(item->InterpolatedAnimationTransforms, boneCount);
        auto lightMode = (*_levelGeometry.Meshes)[item->MeshIndex[0]]->LightMode;

        bool isFullSkin = (item->SkinIndex != NO_VALUE);
        if (isFullSkin)
        {
            Matrix skinBones[BONE_COUNT_MAX];
            for (int i = 0; i < boneCount; i++)
                skinBones[i] = laraObject->BindPoseTransforms[i] * item->InterpolatedAnimationTransforms[i];

            int skinBoneOffset = AddBones(skinBones, boneCount);
            int skinInstance = AddInstance(item->InterpolatedWorld, item->Color, item->AmbientLight, lightMode, lightOffset, lightCount, skinBoneOffset, SKINNING_FULL);
            AddMeshDraws(*(*_levelGeometry.Meshes)[item->SkinIndex], skinInstance);
        }

        int instance = AddInstance(item->InterpolatedWorld, item->Color, item->AmbientLight, lightMode, lightOffset, lightCount, animationBones, SKINNING_CLASSIC);

        for (int meshIndex = 0; meshIndex < (int)item->MeshIndex.size(); meshIndex++)
        {
            if (!nativeItem.MeshBits.Test(meshIndex))
                continue;

            if (isFullSkin && g_Level.Meshes[nativeItem.Model.MeshIndex[meshIndex]].hidden)
                continue;

            AddMeshDraws(*(*_levelGeometry.Meshes)[item->MeshIndex[meshIndex]], instance);
        }

        if (!isFullSkin)
        {
            const auto& joints = (*_levelGeometry.MoveableObjects)[Lara.Skin.SkinJoints];
            if (joints.has_value())
            {
                for (int meshIndex = 1; meshIndex < (int)joints->ObjectMeshes.size(); meshIndex++)
                    AddMeshDraws(*joints->ObjectMeshes[meshIndex], instance);
            }
        }

        // Left and right holsters always draw their slot object (the empty slot is the empty holster mesh),
        // the back holster only when occupied.
        auto addHolster = [&](HolsterSlot slot, int boneIndex, bool skipEmpty)
        {
            if (skipEmpty && slot == HolsterSlot::Empty)
                return;

            const auto& holster = (*_levelGeometry.MoveableObjects)[(int)slot];
            if (holster.has_value() && boneIndex < (int)holster->ObjectMeshes.size())
                AddMeshDraws(*holster->ObjectMeshes[boneIndex], instance);
        };

        addHolster(Lara.Control.Weapon.HolsterInfo.LeftHolster, LM_LTHIGH, false);
        addHolster(Lara.Control.Weapon.HolsterInfo.RightHolster, LM_RTHIGH, false);
        addHolster(Lara.Control.Weapon.HolsterInfo.BackHolster, LM_TORSO, true);

        CollectPlayerHair(*item, item->AmbientLight, lightOffset, lightCount, interpolationFactor);

        // Hair hangs outside the meshes of the player.
        if (IsShadowCaster(ID_LARA))
            MarkShadowCasterDraws(firstDraw, GetItemBounds(*item, CLICK(2)));

        RemoveFadedDraws(firstDraw, item->Color.w);
    }

    // Hair units use their own bone palette: the head attachment followed by one matrix per simulated
    // segment. The frontend updates segment interpolation data, so the matrices are only read here.
    void SceneRenderer::CollectPlayerHair(const RendererItem& item, const Vector4& ambientLight, int lightOffset, int lightCount, float interpolationFactor)
    {
        using namespace TEN::Effects::Hair;

        for (int unitIndex = 0; unitIndex < (int)HairEffect.Units.size(); unitIndex++)
        {
            const auto& unit = HairEffect.Units[unitIndex];
            if (!unit.IsEnabled || !Objects[unit.ObjectID].loaded)
                continue;

            const auto& hairObject = (*_levelGeometry.MoveableObjects)[unit.ObjectID];
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
                AddMeshDraws(*(*_levelGeometry.Meshes)[Objects[unit.ObjectID].skinIndex], instance);
            }
            else
            {
                for (const auto* mesh : hairObject->ObjectMeshes)
                    AddMeshDraws(*mesh, instance);
            }
        }
    }

    void SceneRenderer::UploadFrameStorage(FrameStorageBuffer& storage, const std::vector<unsigned char>& data, int stride, unsigned int usage, const char* name)
    {
        // Storage buffers cannot be empty, so the smallest buffer holds one element.
        int dataSize = std::max((int)data.size(), stride);
        if (dataSize > storage.Capacity || storage.Buffer == nullptr)
        {
            storage.Capacity = std::max(dataSize * 2, stride * INITIAL_INSTANCE_CAPACITY);
            storage.Buffer = _device->CreateBuffer(BufferDesc{ storage.Capacity, stride, usage, name });
        }

        _uploadRing.UploadToBuffer(storage.Buffer.get(), 0, data.data(), (int)data.size());
        _frameStats.UploadBytes += (int)data.size();
    }

    // All per-frame data goes to the GPU in a single copy pass before the scene pass.
    void SceneRenderer::UploadFrameData(ICommandBuffer& commandBuffer, const FrameEnvironment& environment)
    {
        commandBuffer.PushDebugGroup("Upload frame data");
        _uploadRing.Begin(*commandBuffer.BeginCopyPass());

        UploadFrameStorage(_instanceBuffer, _instanceData, (int)sizeof(GpuObjectInstance), BufferUsage::GraphicsStorageRead, "Frame.Instances");
        UploadFrameStorage(_lightBuffer, _lightData, (int)sizeof(GpuLight), BufferUsage::GraphicsStorageRead, "Frame.Lights");
        UploadFrameStorage(_lightClusterBuffer, _lightClusterData, (int)sizeof(unsigned int) * 2, BufferUsage::GraphicsStorageRead, "Frame.LightClusters");
        UploadFrameStorage(_boneBuffer, _boneData, (int)sizeof(Matrix), BufferUsage::GraphicsStorageRead, "Frame.Bones");
        UploadFrameStorage(_sortedIndexBuffer, _sortedIndexData, (int)sizeof(int), BufferUsage::Index, "Frame.SortedIndices");
        UploadFrameStorage(_spriteBuffer, _spriteData, (int)sizeof(GpuSprite), BufferUsage::GraphicsStorageRead, "Frame.Sprites");
        UploadMaterials();
        UploadFrameStorage(_decalBuffer, _decalData, (int)sizeof(GpuDecal), BufferUsage::GraphicsStorageRead, "Frame.Decals");
        UploadFrameStorage(_debrisVertexBuffer, _debrisVertexData, (int)sizeof(Vertex), BufferUsage::Vertex, "Frame.DebrisVertices");
        UploadFrameStorage(_debrisInstanceBuffer, _debrisInstanceData, (int)sizeof(unsigned int), BufferUsage::Vertex, "Frame.DebrisInstances");
        UploadFrameStorage(_debugVertexBuffer, _debugVertexData, (int)sizeof(DebugVertex), BufferUsage::Vertex, "Frame.DebugVertices");
        UploadVideoFrame(environment.VideoFrame, environment.VideoWidth, environment.VideoHeight);

        _uploadRing.End();
        commandBuffer.EndCopyPass();
        commandBuffer.PopDebugGroup();
    }

    void SceneRenderer::DrawObjects(ICommandBuffer& commandBuffer, IRenderPass& renderPass, ScenePass pass, bool isMirrored)
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

    void SceneRenderer::DrawObjectBuckets(ICommandBuffer& commandBuffer, IRenderPass& renderPass, const std::vector<ObjectDraw>& draws, ScenePass pass,
                                         bool isMirrored, bool isAlphaTested)
    {
        bool isAlphaEdges = (pass == ScenePass::AlphaEdges);

        auto shader = (pass == ScenePass::Prepass) ? SceneShader::ObjectsPrepass : (isMirrored ? SceneShader::ObjectsMirrored : SceneShader::Objects);
        renderPass.BindPipeline(GetScenePipeline(shader, GetPassBlendMode(pass), isAlphaTested, 0, pass == ScenePass::AlphaEdges));

        IBuffer* storageBuffers[4] = { _animatedFramesBuffer.get(), _animatedSetsBuffer.get(), _instanceBuffer.Buffer.get(), _boneBuffer.Buffer.get() };
        renderPass.BindVertexStorageBuffers(0, storageBuffers, 4);

        BindScenePixelStorage(renderPass);
        BindLevelGeometry(renderPass, MeshSource::Moveables, true);

        auto state = BucketBindState{};
        int boundMissingMaps = 0;

        bool isSorted = !isMirrored && IsStateSortedPass(pass);
        for (int i = 0; i < (int)draws.size(); i++)
        {
            const auto& draw = draws[isSorted ? _objectDrawOrder[i].Index : i];
            const auto& bucket = *draw.Bucket;
            auto drawInfo = BucketDrawInfo{};
            drawInfo.AnimatedSetOverride = draw.AnimatedSetOverride;
            drawInfo.IsAlphaEdgePass = isAlphaEdges;
            if (!IsBucketInPass(bucket, pass) || (GetAlphaThreshold(bucket, drawInfo) != 0.0f) != isAlphaTested)
                continue;

            const auto& pages = (draw.Source == MeshSource::Statics) ? _staticTextures : _moveableTextures;
            BindPixelVariant(renderPass, shader, pass, isAlphaTested, GetMissingMaps(bucket, pages), boundMissingMaps);
            BindBucket(commandBuffer, renderPass, bucket, pages, bucket.BlendMode, 0, drawInfo, state);
            DrawBucket(renderPass, draw.Source, bucket, draw.InstanceCount, draw.FirstInstance);
        }
    }

    // Instance of a sorted object, created once per frame even when interleaved clusters split the object
    // into several runs. Its state depends on the sorted object type.
    int SceneRenderer::GetSortedObjectInstance(const RendererSortableObject& object, float interpolationFactor)
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
            const auto& moveableObject = (*_levelGeometry.MoveableObjects)[item->ObjectID];
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

    // Walks the transparent polygons in the order computed by the frontend SortTransparentFaces and turns each
    // group run into one draw. All indices go into a single frame index buffer, so a run costs one draw call
    // and object state comes from the instance buffer instead of per-run constant buffer uploads.
    void SceneRenderer::CollectSortedFaces(const RenderView& view, float interpolationFactor)
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
            draw.Room = draw.IsRoom ? object.Room : nullptr;
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

            if (!_sortedDraws.empty() && CanMergeSortedRoomRuns(_sortedDraws.back(), draw))
                _sortedDraws.back().IndexCount += draw.IndexCount;
            else
                _sortedDraws.push_back(draw);
        }
    }

    // Consecutive room runs with the same draw state and portal rectangle draw as one: their indices are contiguous,
    // so the order of the faces is kept.
    bool SceneRenderer::CanMergeSortedRoomRuns(const SortedDraw& previous, const SortedDraw& next) const
    {
        if (!previous.IsRoom || !next.IsRoom || previous.IsSprite || previous.FirstIndex + previous.IndexCount != next.FirstIndex)
            return false;

        const auto& previousBucket = *previous.Bucket;
        const auto& nextBucket = *next.Bucket;
        if (previous.BlendMode != next.BlendMode || previousBucket.BlendMode != nextBucket.BlendMode || previousBucket.Texture != nextBucket.Texture ||
            previousBucket.Animated != nextBucket.Animated || previousBucket.MaterialIndex != nextBucket.MaterialIndex ||
            previous.IsWaterRoom != next.IsWaterRoom || previous.IsCausticsRoom != next.IsCausticsRoom)
        {
            return false;
        }

        if (previous.Room == next.Room)
            return true;

        const auto& targetDesc = _sceneTarget->GetDesc();
        auto previousScissor = GetRoomScissor(*previous.Room, targetDesc.Width, targetDesc.Height);
        auto nextScissor = GetRoomScissor(*next.Room, targetDesc.Width, targetDesc.Height);
        return (previousScissor.X == nextScissor.X && previousScissor.Y == nextScissor.Y &&
                previousScissor.Width == nextScissor.Width && previousScissor.Height == nextScissor.Height);
    }

    void SceneRenderer::DrawSortedFaces(ICommandBuffer& commandBuffer, IRenderPass& renderPass, bool isMirrored)
    {
        const auto& draws = isMirrored ? _mirrorSortedDraws : _sortedDraws;
        if (draws.empty())
            return;

        auto startTime = std::chrono::steady_clock::now();
        commandBuffer.PushDebugGroup(isMirrored ? "Mirrored sorted faces" : "Sorted transparent faces");

        if (!_sortedIndexData.empty())
            renderPass.BindIndexBuffer(BufferBinding{ _sortedIndexBuffer.Buffer.get(), 0 }, IndexSize::Bits32);

        IBuffer* objectStorageBuffers[4] = { _animatedFramesBuffer.get(), _animatedSetsBuffer.get(), _instanceBuffer.Buffer.get(), _boneBuffer.Buffer.get() };

        const IGraphicsPipeline* lastPipeline = nullptr;
        const ITexture* lastSpriteTexture = nullptr;
        // Vertex streams bound: the room vertex region, or the object region plus the instance indices; none after a pipeline change.
        int lastStreamCount = 0;
        auto state = BucketBindState{};

        // Room faces are clipped to their portal rectangle; other faces use the whole target.
        const auto& targetDesc = _sceneTarget->GetDesc();
        auto fullScissor = ScissorRect{ 0, 0, targetDesc.Width, targetDesc.Height };
        const RendererRoom* scissorRoom = nullptr;

        for (const auto& draw : draws)
        {
            // Sprite runs overwrite the sampler and pixel uniform slots used by bucket draws.
            if (draw.IsSprite)
            {
                if (scissorRoom != nullptr)
                {
                    renderPass.SetScissor(fullScissor);
                    scissorRoom = nullptr;
                }

                DrawSpriteRun(commandBuffer, renderPass, draw.Sprites, lastPipeline, lastSpriteTexture, false, DrawCategory::SortedSprites);
                lastStreamCount = 0;
                state = BucketBindState{};
                continue;
            }

            if (draw.Room != scissorRoom)
            {
                auto scissor = (draw.Room != nullptr) ? GetRoomScissor(*draw.Room, targetDesc.Width, targetDesc.Height) : fullScissor;
                if (scissor.Width == 0 || scissor.Height == 0)
                    continue;

                renderPass.SetScissor(scissor);
                scissorRoom = draw.Room;
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
            // Decals are not drawn on sorted faces.
            auto roomInfo = BucketDrawInfo{ draw.IsWaterRoom ? 1u : 0u, draw.IsCausticsRoom ? 1u : 0u };
            roomInfo.IsForcedAlphaTest = draw.IsForcedAlphaTest;
            BindBucket(commandBuffer, renderPass, bucket, pages, draw.BlendMode, draw.IsRoom ? (unsigned int)_roomLightCount : 0, roomInfo, state);

            CountDraw(draw.IsRoom ? DrawCategory::SortedRooms : ((draw.Source == MeshSource::Statics) ? DrawCategory::SortedStatics : DrawCategory::SortedMoveables), draw.IndexCount / 3);
            renderPass.DrawIndexed(draw.IndexCount, 1, draw.FirstIndex, _geometrySections[(int)draw.Source].BaseVertex, draw.Instance);
        }

        if (scissorRoom != nullptr)
            renderPass.SetScissor(fullScissor);

        _frameStats.SortedRecordTime += GetMilliseconds(startTime, std::chrono::steady_clock::now());
        commandBuffer.PopDebugGroup();
    }

    // Same grid: 20x20 tiles of 512 units in the XZ plane.
    bool SceneRenderer::CreateSkyGeometry()
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
    // camera, drawn without depth before the rooms.
    // Hemisphere 0 draws the sky of the view; -1 and 1 draw a hemisphere of the dual paraboloid sky reflections,
    // without fog bulbs and regardless of outside rooms.
    void SceneRenderer::DrawSky(ICommandBuffer& commandBuffer, IRenderPass& renderPass, const RenderView& view, float interpolationFactor, int hemisphere)
    {
        if (hemisphere == 0 && !IsSkyVisible(view))
            return;

        commandBuffer.PushDebugGroup("Sky");
        DrawSkyBackground(commandBuffer, renderPass, view, hemisphere);
        DrawSkyHorizons(commandBuffer, renderPass, view, interpolationFactor, hemisphere);
        commandBuffer.PopDebugGroup();
    }

    bool SceneRenderer::IsSkyVisible(const RenderView& view) const
    {
        for (const auto* room : view.RoomsToDraw)
        {
            if (g_Level.Rooms[room->RoomNumber].flags & ENV_FLAG_SKYBOX)
                return true;
        }

        return false;
    }

    SkyVertexUniforms SceneRenderer::GetSkyVertexUniforms(const Matrix& world, bool isFogBulbsEnabled, const RenderView& view, int hemisphere) const
    {
        constexpr auto PARABOLOID_NEAR = 32.0f;

        bool isParaboloid = (hemisphere != 0);
        auto uniforms = SkyVertexUniforms{ world, (isFogBulbsEnabled && !isParaboloid) ? 1u : 0u };
        uniforms.IsParaboloid = isParaboloid ? 1u : 0u;
        uniforms.Hemisphere = (float)hemisphere;
        uniforms.ParaboloidView = Matrix::CreateLookAt(view.Camera.WorldPosition, view.Camera.WorldPosition + Vector3(0.0f, -BLOCK(1), 0.0f), Vector3::UnitX);
        uniforms.ParaboloidNear = PARABOLOID_NEAR;
        uniforms.ParaboloidFar = DEFAULT_FAR_VIEW;
        return uniforms;
    }

    // Atmospheric dome, classic layers, stars and meteors: what the frontend draws before the clouds.
    void SceneRenderer::DrawSkyBackground(ICommandBuffer& commandBuffer, IRenderPass& renderPass, const RenderView& view, int hemisphere)
    {
        using namespace TEN::Effects::Environment;

        bool isParaboloid = (hemisphere != 0);
        auto shader = isParaboloid ? SceneShader::SkyParaboloid : SceneShader::Sky;

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

        BindPageColorMap(commandBuffer, renderPass, &_skyPage);

        auto rotation = Matrix::CreateRotationX(PI);
        for (int layer = 0; layer < 2; layer++)
        {
            auto color = Weather.SkyColor(layer);
            if (Vector3(color) == Vector3::Zero)
                continue;

            auto pixelUniforms = SkyPixelUniforms{};
            pixelUniforms.SkyColor = color;
            commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &pixelUniforms, sizeof(pixelUniforms));

            for (int tile = 0; tile < 2; tile++)
            {
                auto translation = Matrix::CreateTranslation(
                    view.Camera.WorldPosition.x + Weather.SkyPosition(layer) - tile * SKY_SIZE,
                    view.Camera.WorldPosition.y - 1536.0f,
                    view.Camera.WorldPosition.z);

                auto world = rotation * translation;
                // Fog bulbs affect the first layer only.
                auto skyUniforms = GetSkyVertexUniforms(world, layer == 0, view, hemisphere);
                commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 2, &skyUniforms, sizeof(skyUniforms));
                CountDraw(DrawCategory::Sky, SKY_INDICES_COUNT / 3);
                renderPass.DrawIndexed(SKY_INDICES_COUNT, 1, 0, 0, 0);
            }
        }

        // Stars and meteors, after the layers.
        if (!isParaboloid)
            DrawSkySprites(commandBuffer, renderPass, _skySpriteDraws, true);
    }

    // Stars and meteors are occluded by the moon disc and the clouds; the sun sprite is not.
    void SceneRenderer::DrawSkySprites(ICommandBuffer& commandBuffer, IRenderPass& renderPass, const std::vector<SpriteDraw>& draws, bool isOccluded)
    {
        if (draws.empty())
            return;

        const IGraphicsPipeline* lastPipeline = nullptr;
        const ITexture* lastTexture = nullptr;
        for (const auto& draw : draws)
            DrawSpriteRun(commandBuffer, renderPass, draw, lastPipeline, lastTexture, isOccluded, DrawCategory::Sky);

        auto noAnimation = DrawVertexUniforms{ NO_VALUE };
        commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 1, &noAnimation, sizeof(noAnimation));
    }

    // Sun sprite and horizon meshes: what the frontend draws after the clouds, sun and moon discs and aurora.
    void SceneRenderer::DrawSkyHorizons(ICommandBuffer& commandBuffer, IRenderPass& renderPass, const RenderView& view, float interpolationFactor, int hemisphere)
    {
        if (hemisphere == 0)
            DrawSkySprites(commandBuffer, renderPass, _skySunDraws, false);

        DrawHorizonMeshes(commandBuffer, renderPass, view, interpolationFactor, hemisphere, false);
    }

    // Horizon blend mode: alpha test buckets blend as alpha, and opaque buckets blend once the horizon fades, as in
    // GetBlendModeFromAlpha. The cloud presets cap the alpha below 1 so that their gradients blend.
    static BlendMode GetHorizonBlendMode(BlendMode blendMode, float alpha)
    {
        if (blendMode == BlendMode::AlphaTest)
            return BlendMode::AlphaBlend;

        if (alpha < ALPHA_BLEND_THRESHOLD && (blendMode == BlendMode::Opaque || blendMode == BlendMode::FastAlphaBlend))
            return BlendMode::AlphaBlend;

        return blendMode;
    }

    // The view uses the alpha and gradients of the frontend layer; reflections and the god ray mask keep the plain mesh.
    SkyPixelUniforms SceneRenderer::GetHorizonPixelUniforms(int layer, float alpha, int hemisphere, bool isMask) const
    {
        auto uniforms = SkyPixelUniforms{};
        uniforms.SkyColor = Vector4(1.0f, 1.0f, 1.0f, isMask ? 1.0f : alpha);

        const auto& legacyLayer = _frameEnvironment->HorizonLayers[layer];
        if (hemisphere != 0 || isMask || !legacyLayer.IsDrawn)
            return uniforms;

        uniforms.SkyColor.w = legacyLayer.Alpha;
        uniforms.HorizonGradientFade = legacyLayer.GradientFade;
        uniforms.HorizonGradientRise = legacyLayer.GradientRise;
        uniforms.MeshWorldYMin = legacyLayer.MeshWorldYMin;
        uniforms.MeshWorldYRange = legacyLayer.MeshWorldYRange;
        return uniforms;
    }

    // Horizon meshes of the level. The god ray mask draws them opaque and without fog bulbs, as a silhouette.
    void SceneRenderer::DrawHorizonMeshes(ICommandBuffer& commandBuffer, IRenderPass& renderPass, const RenderView& view, float interpolationFactor, int hemisphere, bool isMask)
    {
        auto shader = isMask ? SceneShader::SkyHorizonMask : ((hemisphere != 0) ? SceneShader::SkyParaboloid : SceneShader::Sky);
        IBuffer* storageBuffers[2] = { _animatedFramesBuffer.get(), _animatedSetsBuffer.get() };

        auto* level = g_GameFlow->GetLevel(CurrentLevel);
        for (int layer = 0; layer < 2; layer++)
        {
            float alpha = level->GetHorizonTransparency(layer);
            if (!level->GetHorizonEnabled(layer) || alpha <= EPSILON)
                continue;

            const auto& horizonObject = (*_levelGeometry.MoveableObjects)[level->GetHorizonObjectID(layer)];
            if (!horizonObject.has_value())
                continue;

            auto position = Vector3::Lerp(level->GetHorizonPrevPosition(layer), level->GetHorizonPosition(layer), interpolationFactor);
            auto orientation = EulerAngles::Lerp(level->GetHorizonPrevOrientation(layer), level->GetHorizonOrientation(layer), interpolationFactor);
            auto world = orientation.ToRotationMatrix() * Matrix::CreateTranslation(position) * Matrix::CreateTranslation(view.Camera.WorldPosition);
            auto pixelUniforms = GetHorizonPixelUniforms(layer, alpha, hemisphere, isMask);
            alpha = pixelUniforms.SkyColor.w;

            auto skyUniforms = GetSkyVertexUniforms(world, !isMask, view, hemisphere);
            commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 2, &skyUniforms, sizeof(skyUniforms));
            commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &pixelUniforms, sizeof(pixelUniforms));

            BindLevelGeometry(renderPass, MeshSource::Moveables, false);

            int lastAnimatedSet = INT_MIN;
            for (const auto* mesh : horizonObject->ObjectMeshes)
            {
                for (const auto& bucket : mesh->Buckets)
                {
                    if (bucket.NumIndices == 0)
                        continue;

                    renderPass.BindPipeline(GetScenePipeline(shader, isMask ? BlendMode::Opaque : GetHorizonBlendMode(bucket.BlendMode, alpha)));
                    renderPass.BindVertexStorageBuffers(0, storageBuffers, 2);

                    int animatedSet = bucket.Animated ? bucket.Texture : NO_VALUE;
                    if (animatedSet != lastAnimatedSet)
                    {
                        auto vertexUniforms = DrawVertexUniforms{ animatedSet };
                        commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 1, &vertexUniforms, sizeof(vertexUniforms));
                        lastAnimatedSet = animatedSet;
                    }

                    const auto& pages = bucket.Animated ? _animatedTextures : _moveableTextures;
                    BindPageColorMap(commandBuffer, renderPass, (bucket.Texture >= 0 && bucket.Texture < (int)pages.size()) ? &pages[bucket.Texture] : nullptr);
                    DrawBucket(renderPass, MeshSource::Moveables, bucket, 1, 0);
                }
            }
        }
    }

    void SceneRenderer::LogGpuTimings()
    {
        if (_device == nullptr)
            return;

        auto timings = _device->ConsumeGpuTimings();
        if (timings.empty())
            return;

        auto message = std::string("Scene renderer GPU time per pass:");
        for (const auto& timing : timings)
            message += fmt::format(" {} {:.3f} ms,", timing.Name, timing.Milliseconds);

        message.back() = '.';
        TENLog(message + fmt::format(" ({} frames)", timings.front().Samples), LogLevel::Info);
    }

    void SceneRenderer::WaitForFrameInFlight()
    {
        auto& fence = _frameFences[_frameFenceIndex];
        if (fence == nullptr)
            return;

        _device->WaitForFence(fence.get());
        fence = nullptr;
    }

    // Live GPU resources by category and the video memory the operating system reports for the process.
    void SceneRenderer::LogMemoryUsage()
    {
        constexpr auto MEGABYTE = 1024.0 * 1024.0;

        auto stats = RHI::GetMemoryStats(_device->GetDeviceName());
        TENLog(fmt::format(
            "Scene renderer memory: textures {:.1f} MB ({}), render targets {:.1f} MB ({}), buffers {:.1f} MB ({}), transfer {:.1f} MB ({}); "
            "process video memory {:.1f} MB of {:.1f} MB budget, dedicated {:.1f} MB ({:.1f} MB used by all processes), shared {:.1f} MB.",
            stats.Bytes[(int)RHI::MemoryCategory::Texture] / MEGABYTE, stats.Counts[(int)RHI::MemoryCategory::Texture],
            stats.Bytes[(int)RHI::MemoryCategory::RenderTarget] / MEGABYTE, stats.Counts[(int)RHI::MemoryCategory::RenderTarget],
            stats.Bytes[(int)RHI::MemoryCategory::Buffer] / MEGABYTE, stats.Counts[(int)RHI::MemoryCategory::Buffer],
            stats.Bytes[(int)RHI::MemoryCategory::TransferBuffer] / MEGABYTE, stats.Counts[(int)RHI::MemoryCategory::TransferBuffer],
            stats.Adapter.ProcessUsage / MEGABYTE, stats.Adapter.ProcessBudget / MEGABYTE,
            stats.Adapter.DedicatedVideoMemory / MEGABYTE, stats.Adapter.AdapterUsage / MEGABYTE, stats.Adapter.SharedSystemMemory / MEGABYTE), LogLevel::Info);
    }

    bool SceneRenderer::IsCausticsRoom(int roomNumber) const
    {
        int flags = g_Level.Rooms[roomNumber].flags;
        return (g_Configuration.EnableCaustics && (flags & ENV_FLAG_WATER) && !(flags & ENV_FLAG_NOCAUSTICS));
    }

    // Current frame of the caustics sprite sequence.
    void SceneRenderer::PrepareCaustics(FramePixelUniforms& uniforms)
    {
        _causticsTexture = nullptr;

        const auto& object = Objects[ID_CAUSTIC_TEXTURES];
        if (!object.loaded || object.nmeshes >= 0 || _levelGeometry.Sprites == nullptr)
            return;

        int spriteIndex = object.meshIndex + (GlobalCounter % -object.nmeshes);
        if (spriteIndex < 0 || spriteIndex >= (int)_levelGeometry.Sprites->size())
            return;

        const auto& sprite = (*_levelGeometry.Sprites)[spriteIndex];
        _causticsTexture = GetSpriteTexture(&sprite);
        if (_causticsTexture == nullptr)
            return;

        const auto& pageDesc = _causticsTexture->GetDesc();
        uniforms.CausticsStartUV = sprite.UV[0];
        uniforms.CausticsSize = Vector2((float)sprite.Width / pageDesc.Width, (float)sprite.Height / pageDesc.Height);
    }

    // Same values.
    FogBulbUniforms SceneRenderer::GetFogBulbUniforms(const RenderView& view)
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

    const SceneFrameStats& SceneRenderer::GetFrameStats() const
    {
        return _frameStats;
    }

    // The prepass always runs: the scene pass reuses its depth, and the later passes read the copy it writes beside it.
    // Reflective materials sample the scene of the previous frame.
    const SceneRenderer::FramePass SceneRenderer::FRAME_PASSES[] =
    {
        { "Shadow map", nullptr, &SceneRenderer::RenderShadowMap },
        { "Depth and normals prepass", nullptr, &SceneRenderer::RenderDepthPrepass },
        { "SSAO", &SceneRenderer::_isAmbientOcclusionActive, &SceneRenderer::RenderSSAO },
        { "Sky reflections", &SceneRenderer::_hasSkyReflectiveMaterials, &SceneRenderer::RenderSkyReflections },
        { "Distortion", &SceneRenderer::_hasDistortion, &SceneRenderer::RenderDistortion },
        { "Sky", nullptr, &SceneRenderer::RenderSkyPass },
        { "Scene", nullptr, &SceneRenderer::RenderScenePass },
        { "DOF", &SceneRenderer::_isDofActive, &SceneRenderer::RenderDOF },
        { "Reflections", &SceneRenderer::_hasReflectiveMaterials, &SceneRenderer::CopyReflectionSource },
        { "Glow", &SceneRenderer::_hasVisibleEmissive, &SceneRenderer::RenderGlow },
        { "Post-process", nullptr, &SceneRenderer::RenderPostProcess },
        { "Upscale", &SceneRenderer::_isUpscaleActive, &SceneRenderer::RenderUpscale }
    };

    void SceneRenderer::RenderScenePass(ICommandBuffer& commandBuffer)
    {
        const auto& view = *_frameView;
        const auto& environment = *_frameEnvironment;

        // The sky pass cleared the target and drew the sky.
        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = _sceneTarget.get();
        colorTarget.Load = LoadOp::Load;

        auto depthTarget = DepthTargetInfo{};
        depthTarget.Texture = _depthTarget.get();
        depthTarget.Load = LoadOp::Load;
        depthTarget.Store = StoreOp::DontCare;

        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, &depthTarget);

        // The sky passes use pixel slot 2 too: restore the shadow uniforms read by rooms and objects.
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &_shadowUniforms, sizeof(_shadowUniforms));
        DrawRooms(commandBuffer, *renderPass, ScenePass::Opaque);
        DrawObjects(commandBuffer, *renderPass, ScenePass::Opaque);
        DrawObjects(commandBuffer, *renderPass, ScenePass::Opaque, true);
        DrawDebris(commandBuffer, *renderPass, DebrisPass::Opaque);
        DrawRooms(commandBuffer, *renderPass, ScenePass::AlphaEdges);
        DrawObjects(commandBuffer, *renderPass, ScenePass::AlphaEdges);
        DrawObjects(commandBuffer, *renderPass, ScenePass::AlphaEdges, true);
        DrawSprites(commandBuffer, *renderPass, ScenePass::Opaque);
        DrawSprites(commandBuffer, *renderPass, ScenePass::Opaque, true);
        DrawRooms(commandBuffer, *renderPass, ScenePass::Additive);
        DrawObjects(commandBuffer, *renderPass, ScenePass::Additive);
        DrawObjects(commandBuffer, *renderPass, ScenePass::Additive, true);
        DrawDebris(commandBuffer, *renderPass, DebrisPass::Additive);
        DrawSprites(commandBuffer, *renderPass, ScenePass::Additive);
        DrawSprites(commandBuffer, *renderPass, ScenePass::Additive, true);
        DrawSortedFaces(commandBuffer, *renderPass);
        DrawSortedFaces(commandBuffer, *renderPass, true);
        DrawDebris(commandBuffer, *renderPass, DebrisPass::Blended);
        DrawGunFlashes(commandBuffer, *renderPass);
        DrawDustStorm(commandBuffer, *renderPass);
        DrawDebugPrimitives(commandBuffer, *renderPass);
        commandBuffer.EndRenderPass();
    }

    void SceneRenderer::RenderFrame(const RenderView& view, const FrameEnvironment& environment)
    {
        if (_device == nullptr || !_isLevelLoaded)
            return;

        int width = _windowWidth;
        int height = _windowHeight;
        if (width <= 0 || height <= 0)
            return;

        WaitForFrameInFlight();

        auto lock = std::lock_guard<std::mutex>(_frameMutex);
        _renderScale = environment.RenderScale;
        _targetPool.CollectUnused();
        if (!EnsureRenderTargets(width, height))
            return;

        EnsureCloudNoise(environment.CloudNoise);

        auto startTime = std::chrono::steady_clock::now();
        ResetFrameStats();

        auto* commandBuffer = _device->AcquireCommandBuffer();
        if (commandBuffer == nullptr)
            return;

        commandBuffer->PushDebugGroup("Frame");

        // Projection is rebuilt for the target aspect ratio; position and orientation follow the game camera.
        auto projection = Matrix::CreatePerspectiveFieldOfView(view.Camera.FOV, (float)width / (float)height, view.Camera.NearPlane, view.Camera.FarPlane);
        _isAmbientOcclusionActive = environment.IsAmbientOcclusionEnabled;
        _nearPlane = view.Camera.NearPlane;
        _farPlane = view.Camera.FarPlane;
        _time = (float)environment.Frame;
        _isShadowMapActive = environment.IsShadowMapEnabled && (environment.ShadowLight != nullptr);
        _isPlayerShadowOnly = environment.IsPlayerShadowOnly;
        _videoSprite = environment.VideoSprite;
        _whiteSprite = environment.WhiteSprite;

        auto frameVertexUniforms = FrameVertexUniforms{};
        frameVertexUniforms.ViewProjection = view.Camera.View * projection;
        _viewProjection = frameVertexUniforms.ViewProjection;
        _view = view.Camera.View;
        _projection = projection;
        _frustum.Update(view.Camera.View, projection);
        _roomClipScaleX = (view.Camera.Projection._11 != 0.0f) ? (projection._11 / view.Camera.Projection._11) : 1.0f;
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
        framePixelUniforms.InverseViewportSize = Vector2(1.0f / _sceneTarget->GetDesc().Width, 1.0f / _sceneTarget->GetDesc().Height);
        PrepareCaustics(framePixelUniforms);
        SetLightClusterUniforms(framePixelUniforms);
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

        auto collectStartTime = std::chrono::steady_clock::now();
        CollectMaterials(environment.InterpolationFactor);
        CollectDecals(view);
        CollectObjects(view, environment.InterpolationFactor);
        SortObjectDraws();
        CollectRoomDraws(view);
        CollectSprites(view);
        CollectSkySprites(view, environment);
        auto sortedStartTime = std::chrono::steady_clock::now();
        CollectSortedFaces(view, environment.InterpolationFactor);
        _frameStats.SortedCollectTime = GetMilliseconds(sortedStartTime, std::chrono::steady_clock::now());
        CollectMirroredSorted(view);
        CollectDebugPrimitives(environment);
        auto uploadStartTime = std::chrono::steady_clock::now();
        _frameStats.CollectTime = GetMilliseconds(collectStartTime, uploadStartTime);

        UploadFrameData(*commandBuffer, environment);
        _frameStats.UploadTime = GetMilliseconds(uploadStartTime, std::chrono::steady_clock::now());

        PrepareShadows(environment, environment.InterpolationFactor);
        _hasDistortion = HasDistortionDraws(view);
        _isDofActive = IsDOFActive(environment);
        _hasVisibleEmissive = false;

        // The shadow uniforms are the largest block of pixel slot 2, so they also cover the passes before the scene.
        commandBuffer->PushUniforms(RHI::ShaderStage::Pixel, 2, &_shadowUniforms, sizeof(_shadowUniforms));

        _frameView = &view;
        _frameEnvironment = &environment;
        _frameVertexUniforms = frameVertexUniforms;

        for (const auto& pass : FRAME_PASSES)
        {
            if (pass.IsEnabled != nullptr && !(this->*pass.IsEnabled))
                continue;

            auto passStartTime = std::chrono::steady_clock::now();
            int passDrawCalls = _frameStats.DrawCalls;
            commandBuffer->PushDebugGroup(pass.Name);
            (this->*pass.Execute)(*commandBuffer);
            commandBuffer->PopDebugGroup();
            _frameStats.Passes.push_back(ScenePassStats{ pass.Name, GetMilliseconds(passStartTime, std::chrono::steady_clock::now()), _frameStats.DrawCalls - passDrawCalls });
        }

        CollectFrameStats(view, environment);

        _frameView = nullptr;
        _frameEnvironment = nullptr;
        commandBuffer->PopDebugGroup();
        _frameStats.CpuTime = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - startTime).count();
        _frameStats.Api = commandBuffer->GetCounters();
        commandBuffer->Submit();
        _hasScene = true;
        _isFramePending = true;

        if (!_screenshotPath.empty())
        {
            TENLog(fmt::format("Scene renderer environment: fog color ({:.2f}, {:.2f}, {:.2f}), fog {:.1f}-{:.1f}, frame {}.",
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

    // Composes the presented frame: the last scene frame faded by the UI, then the 2D layer recorded by the frontend.
    // Menu frames that render no scene reuse the last one as their background.
    void SceneRenderer::FinishFrame(const UiFrame& ui)
    {
        if (_device == nullptr)
            return;

        int width = _windowWidth;
        int height = _windowHeight;
        if (width <= 0 || height <= 0)
            return;

        // A rendered scene already waited for its frame slot.
        if (!_isFramePending)
            WaitForFrameInFlight();

        // GPU timings are measured while the debug window shows them or a benchmark runs.
        _device->SetGpuTimingEnabled(_isGpuTimingRequested || (_isBenchmarkGpuTimingEnabled && TEN::Benchmark::IsBenchmarkActive()));
        _isGpuTimingRequested = false;

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
            _frameStats = SceneFrameStats{};

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

        AcquireOverlayTargets();
        RenderOverlay(*commandBuffer, ui);
        RenderMenuPostProcess(*commandBuffer, ui);
        ReleaseOverlayTargets();
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
        _frameFences[_frameFenceIndex] = _readback.SubmitWithFence(*commandBuffer);
        _frameFenceIndex = (_frameFenceIndex + 1) % FRAMES_IN_FLIGHT_MAX;

        _readback.Poll(*_device, false);

        if (_device->RequiresWindowThreadPresent())
            _isFrameReady = true;
    }

    void SceneRenderer::RequestScreenshot(const std::string& path)
    {
        _screenshotPath = path;
    }

    void SceneRenderer::PresentOnWindowThread()
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
