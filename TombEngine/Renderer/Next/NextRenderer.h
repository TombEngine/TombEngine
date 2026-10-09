#pragma once

#include <mutex>

#include "Objects/game_object_ids.h"
#include "Renderer/Graphics/Vertices/Vertex.h"
#include "Renderer/Next/GpuTypes.h"
#include "Renderer/Next/OverlayTypes.h"
#include "Renderer/RHI/RHIDevice.h"
#include "Renderer/RHI/TextureCapture.h"
#include "Renderer/RHI/UploadRing.h"

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
    struct RendererMirror;
    struct RendererSortableObject;
    struct RendererSprite;
    struct RendererSpriteToDraw;
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
        const std::vector<Structures::RendererSprite>*                 Sprites          = nullptr;
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
        bool         IsCameraUnderwater  = false;
        bool         IsAmbientOcclusionEnabled = false;

        // Post-process values of the legacy renderer.
        Vector3      Tint                = Vector3(0.5f);
        float        Gamma               = 1.0f;
        int          ColorScheme         = 0;
        float        ColorSchemeStrength = 1.0f;
        float        ScreenFade          = 1.0f;
        float        CinematicBarsHeight = 0.0f;
        int          AntialiasingMode    = 0; // Legacy AntialiasingMode: FXAA for low, SMAA for medium and high.

        // Background video of the legacy video handler: BGRA8 frame, null when no background video plays.
        const unsigned char*                 VideoFrame  = nullptr;
        int                                  VideoWidth  = 0;
        int                                  VideoHeight = 0;
        const Structures::RendererSprite*    VideoSprite = nullptr; // Legacy sprite of particles showing the video.

        // Uniform block of the legacy atmospheric sky dome (CAtmosphericSkyBuffer), null when the dome is disabled.
        const void*  AtmosphericSkyUniforms     = nullptr;
        int          AtmosphericSkyUniformsSize = 0;

        // Stars, meteors and sun sprite of the classic sky, drawn additively between the sky layers and horizons.
        const std::vector<Structures::RendererSpriteToDraw>* SkySprites = nullptr;

        // Depth of field of the legacy renderer (DOFState).
        int          DofMode             = 0;
        float        DofDistance         = 0.0f;
        float        DofRange            = 0.0f;
        float        DofStrength         = 0.0f;

        // Lens flares of the legacy renderer, with colors already attenuated by clouds and horizon.
        Vector3      LensFlarePositions[LENS_FLARES_MAX] = {};
        Vector3      LensFlareColors[LENS_FLARES_MAX]    = {};
        int          LensFlareCount      = 0;

        // Shadows of the legacy renderer: shadow casting light (null when none) and blob spheres.
        const Structures::RendererLight* ShadowLight = nullptr;
        bool         IsShadowMapEnabled  = false;
        bool         IsPlayerShadowOnly  = false;
        int          ShadowMapSize       = 1024;
        Vector4      ShadowSpheres[SHADOW_SPHERES_MAX] = {};
        int          ShadowSphereCount   = 0;
    };

    struct NextFrameStats
    {
        float CpuTime   = 0.0f; // Milliseconds spent collecting, uploading and recording the frame.
        int   DrawCalls = 0;
    };

    // New RHI-based renderer. During the migration it draws into a separate preview window, fed by the
    // legacy renderer's culling results, so the two outputs can be compared side by side.
    // Blend states of the legacy DX11 renderer, per blend mode.
    RHI::BlendState GetBlendState(BlendMode blendMode);

    class NextRenderer
    {
    private:
        // Atlas page with the maps of the legacy material system; missing maps use the default textures.
        struct RoomTexture
        {
            std::unique_ptr<RHI::ITexture> ColorMap    = nullptr;
            std::unique_ptr<RHI::ITexture> NormalMap   = nullptr;
            std::unique_ptr<RHI::ITexture> ORSHMap     = nullptr;
            std::unique_ptr<RHI::ITexture> EmissiveMap = nullptr;
        };

        // Instance waiting to be grouped with the other instances of its mesh.
        struct QueuedMeshInstance
        {
            const Structures::RendererMesh* Mesh       = nullptr;
            GpuObjectInstance               Instance   = {};
            int                             RoomNumber = NO_VALUE;
        };

        enum class DebrisPass
        {
            Opaque,
            Additive,
            Blended
        };

        // Debris triangles sharing texture page and blend mode.
        struct DebrisDraw
        {
            const std::vector<RoomTexture>* Pages       = nullptr;
            int                             Page        = 0;
            BlendMode                       BlendMode   = BlendMode::Opaque;
            int                             FirstVertex = 0;
            int                             VertexCount = 0;
        };

        // Per draw values of bucket draws: room state for room buckets, animated set override for objects.
        struct BucketDrawInfo
        {
            unsigned int IsWaterRoom         = 0;
            unsigned int IsCausticsRoom      = 0;
            unsigned int DecalOffset         = 0;
            unsigned int DecalCount          = 0;
            int          AnimatedSetOverride = NO_VALUE;
            bool         IsAlphaEdgePass     = false;
            bool         IsForcedAlphaTest   = false;
            bool         IsAlphaTestDisabled = false;
        };

        // Draw state of consecutive bucket draws; pushes and binds are skipped when unchanged.
        struct BucketBindState
        {
            DrawPixelUniforms  PixelUniforms  = { -1.0f };
            DrawVertexUniforms VertexUniforms = { INT_MIN };
            const RoomTexture* Page           = nullptr;
            bool               IsPageBound    = false;
        };

        enum class MeshSource
        {
            Statics,
            Moveables
        };

        // Instanced draw of one mesh bucket; instances are contiguous in the frame instance buffer.
        // Source of an instance, for mirrors that reflect only some categories of the mirror room.
        enum class InstanceCategory
        {
            None,
            Player,
            Moveable,
            Static
        };

        struct InstanceOrigin
        {
            int              RoomNumber = NO_VALUE;
            InstanceCategory Category   = InstanceCategory::None;
        };

        struct ObjectDraw
        {
            MeshSource                        Source        = MeshSource::Statics;
            const Structures::RendererBucket* Bucket        = nullptr;
            int                               FirstInstance = 0;
            int                               InstanceCount = 0;
            int                               AnimatedSetOverride = NO_VALUE; // Legacy waterfall scrolling.
        };

        // GPU buffer rewritten every frame from CPU data through the upload ring.
        struct FrameStorageBuffer
        {
            std::unique_ptr<RHI::IBuffer> Buffer   = nullptr;
            int                           Capacity = 0;
        };

        enum class SceneShader
        {
            Rooms,
            Objects,
            Sky,
            Sprites,
            RoomsPrepass,
            ObjectsPrepass,
            ObjectsShadow,
            ObjectsDebris,
            ObjectsMirrored, // Reflected by a mirror plane: inverted culling.
            SkyParaboloid,   // Sky into the dual paraboloid map of skybox reflective materials.
            Inventory,       // Objects of the 2D layer: depth written by every blend mode, as in the legacy menus.

            Count
        };

        enum class ScenePass
        {
            Prepass,
            Opaque,
            AlphaEdges, // Legacy second pass of alpha tested buckets: blended texels below the threshold.
            Additive,
            Distortion  // Payload accumulated in the distortion target, applied by the post-process.
        };

        // Instanced draw of consecutive sprites sharing texture and blend mode.
        struct SpriteDraw
        {
            RHI::ITexture* Texture        = nullptr;
            BlendMode      BlendMode      = BlendMode::Additive;
            int            FirstSprite    = 0;
            int            SpriteCount    = 0;
            bool           IsSoftParticle = false;
            int            RenderType     = 0;
        };

        // Run of consecutive sorted polygons sharing one draw group, drawn with one call. Sprite runs
        // use the sprite draw instead of the index range.
        struct SortedDraw
        {
            MeshSource                        Source     = MeshSource::Statics;
            bool                              IsRoom     = false;
            bool                              IsSprite   = false;
            bool                              IsWaterRoom = false;
            bool                              IsCausticsRoom = false;
            bool                              IsForcedAlphaTest = false; // Legacy DrawMoveableAsStaticSorted.
            const Structures::RendererBucket* Bucket     = nullptr;
            BlendMode                         BlendMode  = BlendMode::AlphaBlend;
            int                               FirstIndex = 0;
            int                               IndexCount = 0;
            int                               Instance   = 0;
            SpriteDraw                        Sprites    = {};
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
        std::unique_ptr<RHI::IShader>           _skyVertexShader     = nullptr;
        std::unique_ptr<RHI::IShader>           _skyPixelShader      = nullptr;
        std::unique_ptr<RHI::IBuffer>           _skyVertexBuffer     = nullptr;
        std::unique_ptr<RHI::IBuffer>           _skyIndexBuffer      = nullptr;
        std::unique_ptr<RHI::ITexture>          _skyTexture          = nullptr;
        std::unique_ptr<RHI::IShader>           _spritesVertexShader = nullptr;
        std::unique_ptr<RHI::IShader>           _spritesPixelShader  = nullptr;
        std::unique_ptr<RHI::IBuffer>           _spriteCornerBuffer  = nullptr;
        std::unique_ptr<RHI::ISampler>          _linearClampSampler  = nullptr;

        // Post-process (NextRendererPostProcess.cpp). The scene renders into _sceneTarget, post-process into _colorTarget.
        std::unique_ptr<RHI::IShader>           _postProcessVertexShader = nullptr;
        std::unique_ptr<RHI::IShader>           _postProcessPixelShader  = nullptr;
        std::unique_ptr<RHI::IGraphicsPipeline> _postProcessPipeline     = nullptr;
        std::unique_ptr<RHI::ITexture>          _sceneTarget             = nullptr;
        std::unique_ptr<RHI::IShader>           _depthCopyPixelShader    = nullptr;
        std::unique_ptr<RHI::IShader>           _fxaaPixelShader         = nullptr;
        std::unique_ptr<RHI::IGraphicsPipeline> _fxaaPipeline            = nullptr;
        std::unique_ptr<RHI::ITexture>          _antialiasingSource      = nullptr; // Post-processed image before FXAA.
        std::unique_ptr<RHI::IGraphicsPipeline> _depthCopyPipeline       = nullptr;
        std::unique_ptr<RHI::ITexture>          _sceneDepthCopy          = nullptr; // R32 float copy of the prepass depth.
        std::unique_ptr<RHI::ITexture>          _distortionTarget        = nullptr;
        std::unique_ptr<RHI::ITexture>          _emissiveTarget          = nullptr; // Prepass emissive, source of the glow.
        std::unique_ptr<RHI::ITexture>          _glowTargets[2]          = {};
        std::unique_ptr<RHI::IShader>           _glowVertexShader        = nullptr;
        std::unique_ptr<RHI::IShader>           _glowDownscaleShader     = nullptr;
        std::unique_ptr<RHI::IShader>           _glowBlurShader          = nullptr;
        std::unique_ptr<RHI::IGraphicsPipeline> _glowDownscalePipeline   = nullptr;
        std::unique_ptr<RHI::IGraphicsPipeline> _glowBlurPipeline        = nullptr;
        std::unique_ptr<RHI::IShader>           _glowCombineShader       = nullptr;
        std::unique_ptr<RHI::IGraphicsPipeline> _glowCombinePipeline     = nullptr; // Menu frames only.
        bool                                    _hasEmissiveMaps         = false;
        std::unique_ptr<RHI::ITexture>          _reflectionTarget        = nullptr; // Half resolution scene of the previous frame.
        bool                                    _hasReflectiveMaterials  = false;
        std::unique_ptr<RHI::ITexture>          _skyReflectionTarget     = nullptr; // Dual paraboloid sky, one hemisphere per layer.
        bool                                    _hasSkyReflectiveMaterials = false;

        static constexpr int DOF_PASS_COUNT   = 4;
        static constexpr int DOF_TARGET_COUNT = 3;

        std::unique_ptr<RHI::IShader>           _dofVertexShader                   = nullptr;
        std::unique_ptr<RHI::IShader>           _dofPixelShaders[DOF_PASS_COUNT]   = {};
        std::unique_ptr<RHI::IGraphicsPipeline> _dofPipelines[DOF_PASS_COUNT]      = {};
        std::unique_ptr<RHI::ITexture>          _dofTargets[DOF_TARGET_COUNT]      = {}; // Near blur, far blur, dilated.
        bool                                    _isDofActive                       = false;

        static constexpr int SMAA_PRESET_COUNT = 2; // Medium and ultra.
        static constexpr int SMAA_PASS_COUNT   = 3;

        struct SmaaPreset
        {
            std::unique_ptr<RHI::IShader>           VertexShader                  = nullptr;
            std::unique_ptr<RHI::IShader>           PixelShaders[SMAA_PASS_COUNT] = {};
            std::unique_ptr<RHI::IGraphicsPipeline> Pipelines[SMAA_PASS_COUNT]    = {};
        };

        SmaaPreset                              _smaaPresets[SMAA_PRESET_COUNT] = {};
        std::unique_ptr<RHI::ITexture>          _smaaAreaTexture                = nullptr;
        std::unique_ptr<RHI::ITexture>          _smaaSearchTexture              = nullptr;
        std::unique_ptr<RHI::ITexture>          _smaaEdgesTarget                = nullptr;
        std::unique_ptr<RHI::ITexture>          _smaaWeightsTarget              = nullptr;
        bool                                    _hasDistortion           = false;
        float                                   _nearPlane               = 1.0f;
        float                                   _farPlane                = 1.0f;
        float                                   _time                    = 0.0f;
        Matrix                                  _viewProjection          = Matrix::Identity;
        Matrix                                  _view                    = Matrix::Identity;
        Matrix                                  _projection              = Matrix::Identity;
        std::unique_ptr<RHI::IShader>           _atmosphericSkyVertexShader = nullptr;
        std::unique_ptr<RHI::IShader>           _atmosphericSkyPixelShader  = nullptr;
        std::unique_ptr<RHI::IGraphicsPipeline> _atmosphericSkyPipeline     = nullptr;
        const void*                             _atmosphericSkyUniforms     = nullptr;

        // Same layout as the glyphs of a DirectXTK .spritefont file.
        struct FontGlyph
        {
            unsigned int Character = 0;
            int          Left      = 0;
            int          Top       = 0;
            int          Right     = 0;
            int          Bottom    = 0;
            float        XOffset   = 0.0f;
            float        YOffset   = 0.0f;
            float        XAdvance  = 0.0f;
        };

        struct FontData
        {
            std::unordered_map<unsigned int, FontGlyph> Glyphs           = {};
            float                                        LineSpacing      = 0.0f;
            unsigned int                                 DefaultCharacter = 0;
            std::unique_ptr<RHI::ITexture>               Texture          = nullptr;
        };

        struct OverlayVertex
        {
            Vector2      Position = Vector2::Zero; // Pixels from the top left corner of the target.
            Vector2      UV       = Vector2::Zero;
            unsigned int Color    = 0;
        };

        // Same layout as BarUniforms in Overlay.hlsl.
        struct OverlayBarUniforms
        {
            Vector2      BarStartUV   = Vector2::Zero;
            Vector2      BarScale     = Vector2::One;
            float        Percent      = 1.0f;
            float        PoisonFactor = 0.0f;
            unsigned int IsInner      = 0;
            float        Padding      = 0.0f;
        };

        // Quad run, or with IsObjects a range of object draws rendered in their own pass with depth: FirstVertex and
        // VertexCount then index _overlayObjectDraws.
        struct OverlayDraw
        {
            RHI::ITexture*     Texture     = nullptr;
            int                FirstVertex = 0;
            int                VertexCount = 0;
            bool               IsBar       = false;
            OverlayBarUniforms Bar         = {};
            BlendMode          BlendMode   = BlendMode::PremultipliedAlphaBlend;
            bool               IsObjects   = false;
        };

        struct OverlayObjectDraw
        {
            const Structures::RendererBucket* Bucket        = nullptr;
            int                               Instance      = 0;
            int                               Object        = 0; // Index in UiFrame::Objects.
            BlendMode                         BlendMode     = BlendMode::Opaque;
            bool                              IsAlphaTested = false;
        };

        FontData                                _font                       = {};
        std::unique_ptr<RHI::IShader>           _overlayVertexShader        = nullptr;
        std::unique_ptr<RHI::IShader>           _overlayPixelShader         = nullptr;
        std::unique_ptr<RHI::IGraphicsPipeline> _overlayPipeline            = nullptr;
        std::unique_ptr<RHI::IShader>           _overlayBarPixelShader      = nullptr;
        std::unique_ptr<RHI::IGraphicsPipeline> _overlayBarPipeline         = nullptr;
        std::unique_ptr<RHI::IGraphicsPipeline> _overlayPipelines[16]       = {}; // Per legacy blend mode, created on first use.
        RHI::GraphicsPipelineDesc               _overlayPipelineDesc        = {};
        std::vector<unsigned char>              _overlayVertexData          = {};
        std::vector<OverlayDraw>                _overlayDraws               = {};
        std::vector<OverlayObjectDraw>          _overlayObjectDraws         = {};
        std::vector<unsigned char>              _overlayInstanceData        = {};
        std::vector<unsigned char>              _overlayBoneData            = {};
        std::unique_ptr<RHI::IShader>           _inventoryVertexShader      = nullptr;
        std::unique_ptr<RHI::IShader>           _inventoryPixelShader       = nullptr;
        std::unordered_map<std::string, std::unique_ptr<RHI::ITexture>> _overlayImages = {}; // By file path, null when not decodable.
        int                                     _atmosphericSkyUniformsSize = 0;

        // Shadow map (NextRendererShadows.cpp).
        std::unique_ptr<RHI::IShader>           _objectsShadowPixelShader = nullptr;
        std::unique_ptr<RHI::ITexture>          _shadowMap                = nullptr;
        std::unique_ptr<RHI::ITexture>          _dummyShadowMap           = nullptr;
        std::unique_ptr<RHI::ITexture>          _shadowDepth              = nullptr;
        std::vector<ObjectDraw>                 _shadowDraws              = {};
        GpuShadowUniforms                       _shadowUniforms           = {};
        bool                                    _isShadowMapActive        = false;
        bool                                    _isPlayerShadowOnly       = false;
        bool                                    _isRenderingShadowMap     = false;
        RHI::ITexture*                          _causticsTexture          = nullptr; // Atlas page of the current caustics frame.

        // Depth and normals prepass and screen space ambient occlusion (NextRendererSSAO.cpp).
        std::unique_ptr<RHI::IShader>           _roomsPrepassPixelShader    = nullptr;
        std::unique_ptr<RHI::IShader>           _objectsPrepassPixelShader  = nullptr;
        std::unique_ptr<RHI::IShader>           _ssaoVertexShader           = nullptr;
        std::unique_ptr<RHI::IShader>           _ssaoPixelShader            = nullptr;
        std::unique_ptr<RHI::IShader>           _ssaoBlurHorizontalShader   = nullptr;
        std::unique_ptr<RHI::IShader>           _ssaoBlurVerticalShader     = nullptr;
        std::unique_ptr<RHI::IGraphicsPipeline> _ssaoPipeline               = nullptr;
        std::unique_ptr<RHI::IGraphicsPipeline> _ssaoBlurHorizontalPipeline = nullptr;
        std::unique_ptr<RHI::IGraphicsPipeline> _ssaoBlurVerticalPipeline   = nullptr;
        std::unique_ptr<RHI::ITexture>          _ssaoNoiseTexture           = nullptr;
        std::unique_ptr<RHI::ITexture>          _normalTarget               = nullptr;
        std::unique_ptr<RHI::ITexture>          _ssaoTarget                 = nullptr;
        std::unique_ptr<RHI::ITexture>          _ssaoBlurTarget             = nullptr;
        std::unique_ptr<RHI::ISampler>          _pointClampSampler          = nullptr;
        std::unique_ptr<RHI::ISampler>          _pointWrapSampler           = nullptr;
        Vector4                                 _ssaoKernel[SSAO_KERNEL_SIZE] = {};
        bool                                    _isAmbientOcclusionActive   = false;

        std::unique_ptr<RHI::ISampler>          _anisotropicSampler  = nullptr;
        std::unique_ptr<RHI::ITexture>          _whiteTexture        = nullptr;
        std::unique_ptr<RHI::ITexture>          _flatNormalTexture   = nullptr;
        std::unique_ptr<RHI::ITexture>          _defaultORSHTexture  = nullptr;
        std::unique_ptr<RHI::ITexture>          _blackTexture        = nullptr;

        std::unique_ptr<RHI::IBuffer> _roomVertexBuffer = nullptr;
        std::unique_ptr<RHI::IBuffer> _roomIndexBuffer  = nullptr;
        std::vector<RoomTexture>      _roomTextures     = {};
        std::vector<RoomTexture>      _animatedTextures = {};
        std::unique_ptr<RHI::IBuffer> _animatedFramesBuffer = nullptr;
        std::unique_ptr<RHI::IBuffer> _animatedSetsBuffer   = nullptr;
        std::unordered_map<int, int>  _waterfallSets        = {}; // Object ID to animated set.
        std::unordered_map<int, int>  _videoSets            = {}; // Video animated set to its normalized UV set.
        std::unique_ptr<RHI::ITexture>    _videoTexture  = nullptr;
        bool                              _isVideoActive = false;
        const Structures::RendererSprite* _videoSprite   = nullptr;
        RHI::UploadRing                   _uploadRing    = {}; // Per-frame uploads of every copy pass.
        RHI::TextureReadback              _readback      = {}; // Screenshots, written when the GPU has rendered them.

        GeometryBuffers               _staticGeometry   = {};
        GeometryBuffers               _moveableGeometry = {};
        std::vector<RoomTexture>      _staticTextures   = {};
        std::vector<RoomTexture>      _moveableTextures = {};
        std::vector<RoomTexture>      _spriteTextures   = {};
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
        std::vector<QueuedMeshInstance> _queuedMeshInstances = {};
        FrameStorageBuffer            _decalBuffer         = {};
        FrameStorageBuffer            _debrisVertexBuffer   = {};
        FrameStorageBuffer            _overlayVertexBuffer  = {};
        FrameStorageBuffer            _overlayInstanceBuffer = {};
        FrameStorageBuffer            _overlayBoneBuffer    = {};
        FrameStorageBuffer            _debrisInstanceBuffer = {};
        std::vector<unsigned char>    _debrisVertexData     = {};
        std::vector<unsigned char>    _debrisInstanceData   = {};
        std::vector<DebrisDraw>       _debrisDraws          = {};
        std::vector<ObjectDraw>       _gunFlashDraws        = {};
        std::vector<ObjectDraw>       _mirrorDraws          = {};
        std::vector<InstanceOrigin>   _instanceOrigins      = {}; // One per instance.
        InstanceOrigin                _currentOrigin        = {}; // Assigned to the instances being added.
        std::vector<unsigned char>    _decalData           = {};
        std::unordered_map<int, std::pair<int, int>> _roomDecalSlices = {};
        std::vector<std::pair<int, int>> _roomLightSlices    = {};
        std::vector<int>              _roomLightSliceList  = {};
        FrameStorageBuffer            _sortedIndexBuffer   = {};
        std::vector<unsigned char>    _sortedIndexData     = {};
        std::vector<SortedDraw>       _sortedDraws         = {};
        std::vector<SortedDraw>       _mirrorSortedDraws   = {};
        std::vector<SpriteDraw>       _mirrorSpriteDraws   = {};
        std::vector<SpriteDraw>       _skySpriteDraws      = {};
        std::vector<const Structures::RendererSpriteToDraw*> _spriteSources = {}; // One per frame sprite.
        std::unordered_map<const void*, int> _sortedInstances = {};
        std::array<int, 2>            _hairInstances       = { NO_VALUE, NO_VALUE }; // Primary and secondary hair unit instances.
        FrameStorageBuffer            _materialBuffer      = {};
        std::vector<unsigned char>    _materialData        = {};
        FrameStorageBuffer            _spriteBuffer        = {};
        std::vector<unsigned char>    _spriteData          = {};
        std::vector<SpriteDraw>       _spriteDraws         = {};
        int                           _roomLightCount      = 0;
        NextFrameStats                _frameStats          = {};
        bool                          _isLevelLoaded    = false;

        // Scene frame in _colorTarget; with the 2D layer in _finalTarget, handed from the render thread to the window thread.
        std::unique_ptr<RHI::ITexture> _colorTarget  = nullptr;
        std::unique_ptr<RHI::ITexture> _finalTarget  = nullptr;
        bool                           _hasScene     = false;
        bool                           _isFramePending = false; // Scene rendered and not yet finished: stats add up.
        std::unique_ptr<RHI::ITexture> _depthTarget  = nullptr;
        std::mutex                     _frameMutex   = {};
        std::atomic<bool>              _isFrameReady = false;
        std::atomic<int>               _windowWidth  = 0;
        std::atomic<int>               _windowHeight = 0;
        std::string                    _screenshotPath = {};

        bool CreatePipelines();
        bool EnsureRenderTargets(int width, int height);
        std::unique_ptr<RHI::ITexture> CreateLevelTexture(const std::vector<unsigned char>& encodedData, const std::string& name, bool allowMips = true);
        void LoadOverlayImages(const UiFrame& ui);
        RHI::ITexture* GetOverlayImageTexture(const std::string& path) const;
        std::unique_ptr<RHI::ITexture> CreateSolidTexture(const unsigned char color[4], const std::string& name);
        void UploadTexture(RHI::ITexture* texture, const void* data, int size);
        void CollectMaterials(float interpolationFactor);
        void BindTexturePage(RHI::IRenderPass& renderPass, const RoomTexture* page, RHI::ITexture* colorOverride = nullptr, RHI::ITexture* emissiveOverride = nullptr);
        void BindBucket(RHI::ICommandBuffer& commandBuffer, RHI::IRenderPass& renderPass, const Structures::RendererBucket& bucket, const std::vector<RoomTexture>& pages,
                        BlendMode blendMode, unsigned int lightCount, const BucketDrawInfo& room, BucketBindState& state);
        BucketDrawInfo GetBucketDrawInfo(int roomNumber, bool hasDecals);
        void CollectDecals(const RenderView& view);
        bool IsCausticsRoom(int roomNumber) const;
        void PrepareCaustics(FramePixelUniforms& uniforms);
        void BindScenePixelStorage(RHI::IRenderPass& renderPass);
        void UploadBuffer(RHI::IBuffer* buffer, const void* data, int size);
        void LoadAnimatedTextures(const LegacyLevelGeometry& geometry);
        void AddWaterfallSets(const LegacyLevelGeometry& geometry, std::vector<GpuAnimatedFrame>& frames, std::vector<GpuAnimatedSet>& sets);
        RHI::IGraphicsPipeline* GetScenePipeline(SceneShader shader, BlendMode blendMode);
        void LoadTextureSet(const std::vector<::TEXTURE>& source, std::vector<RoomTexture>& destination, const std::string& name);
        void CreateGeometryBuffers(GeometryBuffers& buffers, const std::vector<Graphics::Vertices::Vertex>& vertices, const std::vector<int>& indices, const std::string& name);
        void CollectObjects(const RenderView& view, float interpolationFactor);
        void CollectPlayer(float interpolationFactor);
        void CollectPlayerHair(const Structures::RendererItem& item, const Vector4& ambientLight, int lightOffset, int lightCount, float interpolationFactor);
        int  AddInstance(const Matrix& world, const Vector4& color, const Vector4& ambientLight, LightMode lightMode, int lightOffset, int lightCount, int boneOffset, int skinningMode);
        int  AddBones(const Matrix* bones, int count);
        void AddMeshDraws(const Structures::RendererMesh& mesh, int firstInstance, int instanceCount = 1);
        void RemoveFadedDraws(int firstDraw, float alpha);

        // Mesh effects, gun shells and swarms (NextRendererEffects.cpp).
        void GetRoomLightSlice(int roomNumber, float interpolationFactor, int& lightOffset, int& lightCount);
        void QueueMeshInstance(const Structures::RendererMesh& mesh, const Matrix& world, const Vector4& color, const Vector4& ambientLight,
                               LightMode lightMode, int lightOffset, int lightCount, int roomNumber);
        void FlushMeshInstances();
        void CollectEffectMeshes(const RenderView& view, float interpolationFactor);
        static bool IsMirrored(const InstanceOrigin& origin, const Structures::RendererMirror& mirror);
        int  AddMirroredInstance(int instanceIndex, const Structures::RendererMirror& mirror);
        void CollectMirrors(const RenderView& view);
        void CollectSkySprites(const RenderView& view, const FrameEnvironment& environment);
        int  AddMirroredSprites(int firstSprite, int count, const Structures::RendererMirror& mirror, const RenderView& view);
        void CollectMirroredSorted(const RenderView& view);
        void AddGunFlash(GAME_OBJECT_ID flashObjectID, const Matrix& world, const Vector4& color, const Vector4& ambientLight);
        void CollectGunFlashes(const RenderView& view);
        void DrawGunFlashes(RHI::ICommandBuffer& commandBuffer, RHI::IRenderPass& renderPass);

        // Debris (NextRendererDebris.cpp).
        void CollectDebris(float interpolationFactor);
        void DrawDebris(RHI::ICommandBuffer& commandBuffer, RHI::IRenderPass& renderPass, DebrisPass pass);
        int  AddLights(const std::vector<Structures::RendererLight*>& lights, float interpolationFactor, int roomNumber, int prevRoomNumber, float fade, int& lightOffset);
        static GpuLight ToGpuLight(const Structures::RendererLight& light, float interpolationFactor, float fade);
        void UploadFrameStorage(FrameStorageBuffer& storage, const std::vector<unsigned char>& data, int stride, unsigned int usage, const char* name);
        void UploadFrameData(RHI::ICommandBuffer& commandBuffer, const FrameEnvironment& environment);
        void CollectSortedFaces(const RenderView& view, float interpolationFactor);
        int  GetSortedObjectInstance(const Structures::RendererSortableObject& object, float interpolationFactor);
        void DrawSortedFaces(RHI::ICommandBuffer& commandBuffer, RHI::IRenderPass& renderPass, bool isMirrored = false);
        void LogGpuTimings();
        static FogBulbUniforms GetFogBulbUniforms(const RenderView& view);
        bool CreateSkyGeometry();
        void DrawSky(RHI::ICommandBuffer& commandBuffer, RHI::IRenderPass& renderPass, const RenderView& view, float interpolationFactor, int hemisphere = 0);
        bool CreateSkyReflectionResources();
        bool CreateAtmosphereResources();
        bool LoadFont(const std::string& path);
        bool CreateOverlayResources(const std::string& fontPath);
        void AddOverlayQuad(RHI::ITexture* texture, const Vector2& position, const Vector2& size, const Vector2& uvMin, const Vector2& uvMax, const Vector4& color,
                            BlendMode blendMode = BlendMode::PremultipliedAlphaBlend);
        void AddOverlayCorners(RHI::ITexture* texture, const Vector2* corners, const Vector2* uvs, const Vector4& color, BlendMode blendMode);
        void AddOverlaySprite(const OverlaySprite& sprite);
        void AddScreenOverlays(const UiFrame& ui);
        RHI::IGraphicsPipeline* GetOverlayPipeline(BlendMode blendMode);
        void CollectOverlay(const UiFrame& ui);
        void AddOverlayText(const OverlayText& text, const Vector2& scale);
        void AddOverlayBar(const OverlayBar& bar);
        void AddOverlayObjects(const UiFrame& ui, const OverlayLayer& layer);
        void UploadOverlayData(bool hasScene);
        int  DrawOverlayQuads(RHI::ICommandBuffer& commandBuffer, int firstDraw, bool isFirstPass);
        void DrawOverlayObjects(RHI::ICommandBuffer& commandBuffer, const UiFrame& ui, const OverlayDraw& draw, bool isFirstPass, bool isFirstObjectPass);
        void RenderOverlay(RHI::ICommandBuffer& commandBuffer, const UiFrame& ui);
        void RenderMenuPostProcess(RHI::ICommandBuffer& commandBuffer, const UiFrame& ui);
        void DrawAtmosphericSky(RHI::ICommandBuffer& commandBuffer, RHI::IRenderPass& renderPass);
        void RenderSkyReflections(RHI::ICommandBuffer& commandBuffer, const RenderView& view, float interpolationFactor);
        void DrawObjects(RHI::ICommandBuffer& commandBuffer, RHI::IRenderPass& renderPass, ScenePass pass, bool isMirrored = false);
        static BlendMode GetPassBlendMode(ScenePass pass);
        static bool IsBucketInPass(const Structures::RendererBucket& bucket, ScenePass pass);

        // Post-process (NextRendererPostProcess.cpp).
        std::unique_ptr<RHI::IGraphicsPipeline> CreateFullscreenPipeline(RHI::IShader* vertexShader, RHI::IShader* pixelShader, RHI::PixelFormat format, const char* name);
        bool CreatePostProcessResources();
        void RenderPostProcess(RHI::ICommandBuffer& commandBuffer, const FrameEnvironment& environment);
        void CopySceneDepth(RHI::ICommandBuffer& commandBuffer);
        bool HasDistortionDraws(const RenderView& view) const;
        void RenderDistortion(RHI::ICommandBuffer& commandBuffer, const RenderView& view);

        // Shadow map (NextRendererShadows.cpp).
        bool         CreateShadowResources();
        bool         EnsureShadowMap(int size);
        bool         IsShadowCaster(int objectID) const;
        unsigned int GetShadowReceiverMask(int objectID) const;
        void         MarkShadowCasterDraws(int firstDraw);
        void         PrepareShadows(const FrameEnvironment& environment, float interpolationFactor);
        void         RenderShadowMap(RHI::ICommandBuffer& commandBuffer, const FrameVertexUniforms& cameraUniforms);

        // Prepass and ambient occlusion (NextRendererSSAO.cpp).
        bool  CreateSSAOResources();
        bool  EnsureSSAOTargets(int width, int height);
        bool  CreateDOFResources();
        bool  EnsureDOFTargets(int width, int height);
        static bool IsDOFActive(const FrameEnvironment& environment);
        void  RenderDOF(RHI::ICommandBuffer& commandBuffer, const FrameEnvironment& environment);
        void  AddVideoSets(const LegacyLevelGeometry& geometry, std::vector<GpuAnimatedFrame>& frames, std::vector<GpuAnimatedSet>& sets);
        void  UploadVideoFrame(const unsigned char* frame, int width, int height);
        int   GetVideoSet(const Structures::RendererBucket& bucket) const;
        bool  CreateGlowResources();
        bool  EnsureGlowTargets(int width, int height);
        void  RenderGlow(RHI::ICommandBuffer& commandBuffer);
        void  ClearSampledTargets();
        void  DownscaleTexture(RHI::ICommandBuffer& commandBuffer, RHI::ITexture* source, RHI::ITexture* target, const char* name);
        float GetAmbientOcclusionExponent() const;
        void  RenderDepthPrepass(RHI::ICommandBuffer& commandBuffer, const RenderView& view);
        void  RenderSSAO(RHI::ICommandBuffer& commandBuffer, const Matrix& projection);
        void  RenderFullscreenPass(RHI::ICommandBuffer& commandBuffer, RHI::IGraphicsPipeline* pipeline, RHI::ITexture* target, RHI::ITexture* source,
                                   RHI::ISampler* sourceSampler, const char* name);
        void  RenderFullscreenPass(RHI::ICommandBuffer& commandBuffer, RHI::IGraphicsPipeline* pipeline, RHI::ITexture* target,
                                   const RHI::TextureSamplerBinding* bindings, int bindingCount, const char* name, RHI::LoadOp load = RHI::LoadOp::DontCare);
        bool  CreateSMAAResources();
        bool  EnsureSMAATargets(int width, int height);
        void  RenderAntialiasing(RHI::ICommandBuffer& commandBuffer, int antialiasingMode, RHI::ITexture* target);

        // Sprites (NextRendererSprites.cpp).
        bool           CreateSpriteResources();
        RHI::ITexture* GetSpriteTexture(const Structures::RendererSprite* sprite) const;
        bool           AddSprite(const Structures::RendererSpriteToDraw& sprite, const RenderView& view, const Structures::RendererMirror* mirror = nullptr);
        void           CollectSprites(const RenderView& view);
        void           DrawSpriteRun(RHI::ICommandBuffer& commandBuffer, RHI::IRenderPass& renderPass, const SpriteDraw& draw, const RHI::IGraphicsPipeline*& lastPipeline, const RHI::ITexture*& lastTexture);
        void           DrawSprites(RHI::ICommandBuffer& commandBuffer, RHI::IRenderPass& renderPass, ScenePass pass, bool isMirrored = false);
        void DrawRooms(RHI::ICommandBuffer& commandBuffer, RHI::IRenderPass& renderPass, const RenderView& view, ScenePass pass);

    public:
        bool Initialize(SDL_Window* window, RHI::Backend backend, const std::string& gameDirectory, bool preferLowPowerGpu);
        void Shutdown();
        bool IsActive() const;

        void LoadLevel(const LegacyLevelGeometry& geometry);
        void RenderFrame(const RenderView& view, const FrameEnvironment& environment);
        void FinishFrame(const UiFrame& ui);
        void PresentOnWindowThread();
        void RequestScreenshot(const std::string& path);

        const NextFrameStats& GetFrameStats() const;
    };

    extern NextRenderer g_NextRenderer;
}
