// Water surfaces and underwater fog, drawn inside the scene pass (RenderScenePass), which they split after the opaque
// draws: the surfaces refract and reflect a copy of the opaque scene (reflections traced at half resolution), then
// the fullscreen underwater fog absorbs the light of the scene when the camera is underwater.
// At level load, surface grids are generated over the horizontal portals between water rooms and the dry rooms above.
// This computation lives in the engine temporarily and will move to Tomb Editor in the future.

#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

#include "Renderer/RHI/ShaderLibrary.h"
#include "Renderer/Scene/RenderView.h"
#include "Renderer/Structures/RendererDoor.h"
#include "Renderer/Structures/RendererRoom.h"
#include "Specific/level.h"

using namespace TEN::Renderer::RHI;
using namespace TEN::Renderer::Structures;

namespace TEN::Renderer::Backend
{
    // Fixed water parameters, to be made configurable per level.
    constexpr auto WATER_COLOR               = Vector3(0.03f, 0.11f, 0.11f); // Scattered light, lit by the room ambient light.
    constexpr auto WATER_ABSORPTION          = Vector3(0.9f, 0.35f, 0.25f); // Per block of water.
    constexpr auto WATER_REFRACTION_STRENGTH = 0.025f;                        // Screen UV offset of the steepest waves.
    constexpr auto WATER_REFLECTION_STRENGTH = 1.0f;
    constexpr auto WATER_FLOW_DIRECTION      = Vector2(0.6f, 0.8f);
    constexpr auto WATER_FLOW_SPEED          = 0.025f;                        // Wave tiles per second.
    constexpr auto WATER_WAVE_TILE           = 2048.0f;
    constexpr auto WATER_WAVE_STRENGTH       = 0.3f;
    constexpr auto WATER_SPECULAR            = 2.0f;

    constexpr auto WAVE_NORMAL_MAP_SIZE   = 256;
    constexpr auto WAVE_NORMAL_MAP_MIPS   = 9;
    constexpr auto WAVE_COUNT             = 32;
    constexpr auto WAVE_FREQUENCY_MAX     = 12;
    constexpr auto PORTAL_FLATNESS_MAX    = 1.0f;
    constexpr auto SWELL_HEIGHT_MAX       = 24.0f;  // Sum of the swell amplitudes of Water.hlsl.
    constexpr auto SWELL_EDGE_FADE        = 512.0f;
    constexpr auto SURFACE_CELL_SIZE      = 256.0f;
    constexpr auto SURFACE_CELL_COUNT_MAX = 128;    // Per side: larger surfaces get larger cells.

    // Surface, reflection trace and underwater fog pipelines, and the wave normal map.
    bool SceneRenderer::CreateWaterResources()
    {
        _waterVertexShader = LoadShader(*_device, _shaderDirectory, "Water", "VS");
        _waterPixelShader = LoadShader(*_device, _shaderDirectory, "Water", "PS");
        _waterReflectionShader = LoadShader(*_device, _shaderDirectory, "Water", "PSReflection");
        _underwaterFogVertexShader = LoadShader(*_device, _shaderDirectory, "Water", "VSFullscreen");
        _underwaterFogPixelShader = LoadShader(*_device, _shaderDirectory, "Water", "PSUnderwaterFog");
        if (_waterVertexShader == nullptr || _waterPixelShader == nullptr || _waterReflectionShader == nullptr ||
            _underwaterFogVertexShader == nullptr || _underwaterFogPixelShader == nullptr)
        {
            return false;
        }

        // Both faces are drawn, so the surface is seen from below. Depth is tested but not written: the transparent
        // draws that follow are not hidden by the water.
        auto desc = GraphicsPipelineDesc{};
        desc.VertexShader = _waterVertexShader.get();
        desc.PixelShader = _waterPixelShader.get();
        desc.Layout.NumBuffers = 1;
        desc.Layout.Buffers[0] = VertexBufferLayout{ 0, (int)sizeof(Vector4), VertexInputRate::Vertex };
        desc.Layout.Attributes[0] = VertexAttribute{ 0, 0, VertexElementFormat::Float4, 0 };
        desc.Layout.NumAttributes = 1;
        desc.Rasterizer.Cull = RHI::CullMode::None;
        desc.DepthStencil.EnableDepthWrite = false;
        desc.DepthFormat = PixelFormat::D32Float;
        desc.NumColorTargets = 1;
        desc.ColorTargets[0].Format = _device->GetSwapchainFormat();
        desc.ColorTargets[0].Blend = GetBlendState(BlendMode::Opaque);
        desc.Name = "Water";
        _waterPipeline = _device->CreateGraphicsPipeline(desc);

        // The reflection trace has no depth target: hidden surfaces are traced too, but never shown.
        desc.PixelShader = _waterReflectionShader.get();
        desc.DepthStencil.EnableDepthTest = false;
        desc.DepthFormat = PixelFormat::Unknown;
        desc.Name = "WaterReflection";
        _waterReflectionPipeline = _device->CreateGraphicsPipeline(desc);

        _underwaterFogPipeline = CreateFullscreenPipeline(_underwaterFogVertexShader.get(), _underwaterFogPixelShader.get(), _device->GetSwapchainFormat(), "UnderwaterFog");

        return (_waterPipeline != nullptr && _waterReflectionPipeline != nullptr && _underwaterFogPipeline != nullptr && CreateWaveNormalTexture());
    }

    // Slopes of a height field made of sine waves with whole periods across the map, so the map tiles. Amplitudes fall
    // with frequency as in an ocean spectrum. Slopes are stored in RG, scaled to the steepest texel.
    bool SceneRenderer::CreateWaveNormalTexture()
    {
        struct Wave
        {
            float FrequencyX = 0.0f;
            float FrequencyY = 0.0f;
            float Amplitude  = 0.0f;
            float Phase      = 0.0f;
        };

        // Random wave set from a fixed seed; the lowest frequencies are skipped.
        auto generator = std::default_random_engine{};
        auto frequencies = std::uniform_int_distribution<int>(-WAVE_FREQUENCY_MAX, WAVE_FREQUENCY_MAX);
        auto phases = std::uniform_real_distribution<float>(0.0f, PI_MUL_2);

        auto waves = std::vector<Wave>{};
        while ((int)waves.size() < WAVE_COUNT)
        {
            int frequencyX = frequencies(generator);
            int frequencyY = frequencies(generator);
            float frequency = sqrt((float)(frequencyX * frequencyX + frequencyY * frequencyY));
            if (frequency < 2.0f)
                continue;

            waves.push_back(Wave{ (float)frequencyX, (float)frequencyY, 1.0f / pow(frequency, 1.8f), phases(generator) });
        }

        // Analytic slopes of the summed waves at each texel.
        auto slopes = std::vector<Vector2>(WAVE_NORMAL_MAP_SIZE * WAVE_NORMAL_MAP_SIZE);
        float slopeMax = 0.0f;
        for (int y = 0; y < WAVE_NORMAL_MAP_SIZE; y++)
        {
            for (int x = 0; x < WAVE_NORMAL_MAP_SIZE; x++)
            {
                float u = (float)x / WAVE_NORMAL_MAP_SIZE;
                float v = (float)y / WAVE_NORMAL_MAP_SIZE;

                auto slope = Vector2::Zero;
                for (const auto& wave : waves)
                {
                    float derivative = wave.Amplitude * PI_MUL_2 * cos(PI_MUL_2 * (wave.FrequencyX * u + wave.FrequencyY * v) + wave.Phase);
                    slope += Vector2(wave.FrequencyX, wave.FrequencyY) * derivative;
                }

                slopes[y * WAVE_NORMAL_MAP_SIZE + x] = slope;
                slopeMax = std::max(slopeMax, std::max(abs(slope.x), abs(slope.y)));
            }
        }

        // Encode to RGBA8, then upload and build the mip chain, waiting for the device (load time only).
        auto pixels = std::vector<unsigned char>(slopes.size() * 4);
        for (int i = 0; i < (int)slopes.size(); i++)
        {
            auto slope = slopes[i] / std::max(slopeMax, EPSILON);
            pixels[i * 4 + 0] = (unsigned char)std::clamp((int)((slope.x * 0.5f + 0.5f) * 255.0f + 0.5f), 0, 255);
            pixels[i * 4 + 1] = (unsigned char)std::clamp((int)((slope.y * 0.5f + 0.5f) * 255.0f + 0.5f), 0, 255);
            pixels[i * 4 + 2] = 255;
            pixels[i * 4 + 3] = 255;
        }

        auto desc = TextureDesc{};
        desc.Width = WAVE_NORMAL_MAP_SIZE;
        desc.Height = WAVE_NORMAL_MAP_SIZE;
        desc.MipLevels = WAVE_NORMAL_MAP_MIPS;
        desc.Usage = TextureUsage::Sampler | TextureUsage::ColorTarget;
        desc.Name = "Water.WaveNormals";
        _waveNormalTexture = _device->CreateTexture(desc);
        if (_waveNormalTexture == nullptr)
            return false;

        UploadTexture(_waveNormalTexture.get(), pixels.data(), (int)pixels.size());

        auto* commandBuffer = _device->AcquireCommandBuffer();
        _device->GenerateMipmaps(commandBuffer, _waveNormalTexture.get());
        commandBuffer->Submit();
        _device->WaitIdle();
        return true;
    }

    // Classic levels have no water surface geometry: the surfaces are the horizontal portals of water rooms leading
    // to the dry room above. Quicksand stays opaque. Builds one static vertex buffer for all surfaces at level load.
    void SceneRenderer::LoadWaterSurfaces(const LevelGeometry& geometry)
    {
        _waterSurfaces.clear();
        _waterDraws.clear();
        _waterVertexBuffer = nullptr;
        _roomWaterSurfaces.assign(geometry.Rooms->size(), std::vector<int>{});
        _waterCoveredBuckets.clear();

        for (const auto& room : *geometry.Rooms)
        {
            int flags = g_Level.Rooms[room.RoomNumber].flags;
            if (!(flags & ENV_FLAG_WATER) || (flags & ENV_FLAG_SWAMP))
                continue;

            for (const auto& portal : room.Doors)
            {
                if (portal.RoomNumber < 0 || portal.RoomNumber >= (int)geometry.Rooms->size())
                    continue;

                if (g_Level.Rooms[portal.RoomNumber].flags & (ENV_FLAG_WATER | ENV_FLAG_SWAMP))
                    continue;

                // The dry room is above the portal: world Y points down.
                if ((*geometry.Rooms)[portal.RoomNumber].BoundingBox.Center.y >= portal.AbsoluteVertices[0].y)
                    continue;

                AddWaterSurface(portal, room.RoomNumber);
            }
        }

        if (_waterSurfaces.empty())
            return;

        // Grids are built after all surfaces exist, since the swell weights look for neighboring surfaces.
        auto vertices = std::vector<Vector4>{};
        for (auto& surface : _waterSurfaces)
            AddWaterSurfaceGrid(surface, vertices);

        int size = (int)(vertices.size() * sizeof(Vector4));
        _waterVertexBuffer = _device->CreateBuffer(BufferDesc{ size, 0, BufferUsage::Vertex, "Water.Vertices" });
        if (_waterVertexBuffer == nullptr)
        {
            _waterSurfaces.clear();
            return;
        }

        UploadBuffer(_waterVertexBuffer.get(), vertices.data(), size);
        HideWaterSurfaceBuckets(geometry);
        TENLog(fmt::format("Scene renderer: {} water surfaces generated, {} water face buckets hidden.", _waterSurfaces.size(), _waterCoveredBuckets.size()), LogLevel::Info);
    }

    // Builders lay blended faces on the water portals to fake a surface; the generated surface replaces them.
    void SceneRenderer::HideWaterSurfaceBuckets(const LevelGeometry& geometry)
    {
        for (const auto& surface : _waterSurfaces)
        {
            for (int roomNumber : { surface.WaterRoom, surface.DryRoom })
            {
                for (const auto& bucket : (*geometry.Rooms)[roomNumber].Buckets)
                {
                    if (IsWaterSurfaceBucket(bucket, surface))
                        _waterCoveredBuckets.insert(&bucket);
                }
            }
        }
    }

    // A non opaque, flat bucket at the surface height whose bounds overlap the surface.
    bool SceneRenderer::IsWaterSurfaceBucket(const RendererBucket& bucket, const WaterSurface& surface)
    {
        if (bucket.BlendMode == BlendMode::Opaque || bucket.BlendMode == BlendMode::AlphaTest)
            return false;

        if (abs(bucket.BoundsMin.y - surface.BoundsMin.y) > PORTAL_FLATNESS_MAX || abs(bucket.BoundsMax.y - surface.BoundsMin.y) > PORTAL_FLATNESS_MAX)
            return false;

        return (bucket.BoundsMin.x < surface.BoundsMax.x && bucket.BoundsMax.x > surface.BoundsMin.x &&
                bucket.BoundsMin.z < surface.BoundsMax.z && bucket.BoundsMax.z > surface.BoundsMin.z);
    }

    // Portals leading up are flat: their corners share the height of the water room ceiling. Sloped portals are
    // skipped. The surface is registered with both of its rooms.
    void SceneRenderer::AddWaterSurface(const RendererDoor& portal, int waterRoom)
    {
        Vector3 corners[4];
        for (int i = 0; i < 4; i++)
            corners[i] = Vector3(portal.AbsoluteVertices[i].x, portal.AbsoluteVertices[i].y, portal.AbsoluteVertices[i].z);

        for (int i = 1; i < 4; i++)
        {
            if (abs(corners[i].y - corners[0].y) > PORTAL_FLATNESS_MAX)
                return;
        }

        auto surface = WaterSurface{};
        surface.WaterRoom = waterRoom;
        surface.DryRoom = portal.RoomNumber;
        surface.BoundsMin = Vector3::Min(Vector3::Min(corners[0], corners[1]), Vector3::Min(corners[2], corners[3]));
        surface.BoundsMax = Vector3::Max(Vector3::Max(corners[0], corners[1]), Vector3::Max(corners[2], corners[3]));

        int index = (int)_waterSurfaces.size();
        _waterSurfaces.push_back(surface);
        _roomWaterSurfaces[waterRoom].push_back(index);
        _roomWaterSurfaces[portal.RoomNumber].push_back(index);
    }

    // The swells displace a grid over the portal; W of each vertex weighs them. Cells are SURFACE_CELL_SIZE wide, up to
    // SURFACE_CELL_COUNT_MAX per side; the grid is emitted as a non indexed triangle list.
    void SceneRenderer::AddWaterSurfaceGrid(WaterSurface& surface, std::vector<Vector4>& vertices) const
    {
        constexpr int TRIANGLE_CORNERS[6][2] = { { 0, 0 }, { 1, 0 }, { 1, 1 }, { 0, 0 }, { 1, 1 }, { 0, 1 } };

        auto size = surface.BoundsMax - surface.BoundsMin;
        int cellsX = std::clamp((int)ceil(size.x / SURFACE_CELL_SIZE), 1, SURFACE_CELL_COUNT_MAX);
        int cellsZ = std::clamp((int)ceil(size.z / SURFACE_CELL_SIZE), 1, SURFACE_CELL_COUNT_MAX);

        auto points = std::vector<Vector4>((cellsX + 1) * (cellsZ + 1));
        for (int z = 0; z <= cellsZ; z++)
        {
            for (int x = 0; x <= cellsX; x++)
            {
                float pointX = surface.BoundsMin.x + size.x * x / cellsX;
                float pointZ = surface.BoundsMin.z + size.z * z / cellsZ;
                points[z * (cellsX + 1) + x] = Vector4(pointX, surface.BoundsMin.y, pointZ, GetSwellWeight(surface, pointX, pointZ));
            }
        }

        surface.FirstVertex = (int)vertices.size();
        for (int z = 0; z < cellsZ; z++)
        {
            for (int x = 0; x < cellsX; x++)
            {
                for (const auto& corner : TRIANGLE_CORNERS)
                    vertices.push_back(points[(z + corner[1]) * (cellsX + 1) + x + corner[0]]);
            }
        }

        surface.VertexCount = (int)vertices.size() - surface.FirstVertex;
    }

    // Swells fade toward the edges that no other surface continues, so the raised water leaves no gap above the floor
    // beyond them.
    float SceneRenderer::GetSwellWeight(const WaterSurface& surface, float x, float z) const
    {
        constexpr auto EDGE_PROBE = 1.0f;

        float distances[4] = { x - surface.BoundsMin.x, surface.BoundsMax.x - x, z - surface.BoundsMin.z, surface.BoundsMax.z - z };
        Vector2 probes[4] =
        {
            Vector2(surface.BoundsMin.x - EDGE_PROBE, z),
            Vector2(surface.BoundsMax.x + EDGE_PROBE, z),
            Vector2(x, surface.BoundsMin.z - EDGE_PROBE),
            Vector2(x, surface.BoundsMax.z + EDGE_PROBE)
        };

        float weight = 1.0f;
        for (int i = 0; i < 4; i++)
        {
            if (distances[i] < SWELL_EDGE_FADE && !IsWaterSurfaceAt(probes[i], surface.BoundsMin.y))
                weight = std::min(weight, distances[i] / SWELL_EDGE_FADE);
        }

        return (weight * weight * (3.0f - 2.0f * weight));
    }

    // True when a surface at the given height covers the XZ point.
    bool SceneRenderer::IsWaterSurfaceAt(const Vector2& point, float height) const
    {
        for (const auto& surface : _waterSurfaces)
        {
            if (abs(surface.BoundsMin.y - height) <= PORTAL_FLATNESS_MAX &&
                point.x >= surface.BoundsMin.x && point.x <= surface.BoundsMax.x &&
                point.y >= surface.BoundsMin.z && point.y <= surface.BoundsMax.z)
            {
                return true;
            }
        }

        return false;
    }

    // Value of the IsWaterRoom uniform: rooms on the other side of the water than the camera get a refraction wobble.
    // Water surfaces refract the scene themselves, so levels with water surfaces draw every room as seen from the
    // camera side.
    unsigned int SceneRenderer::GetWaterRoomFlag(int roomNumber) const
    {
        if (!_waterSurfaces.empty())
            return _isCameraUnderwater ? 1 : 0;

        return (g_Level.Rooms[roomNumber].flags & ENV_FLAG_WATER) ? 1 : 0;
    }

    // Surfaces of the visible rooms, on either side of the water, between active rooms and inside the view frustum.
    // The frame counter keeps a surface shared by two visible rooms from being collected twice.
    void SceneRenderer::CollectWaterSurfaces(const RenderView& view)
    {
        _waterDraws.clear();
        if (_waterSurfaces.empty())
            return;

        _waterFrame++;
        for (const auto* room : view.RoomsToDraw)
        {
            if (room->RoomNumber < 0 || room->RoomNumber >= (int)_roomWaterSurfaces.size())
                continue;

            for (int index : _roomWaterSurfaces[room->RoomNumber])
            {
                auto& surface = _waterSurfaces[index];
                if (surface.Frame == _waterFrame)
                    continue;

                surface.Frame = _waterFrame;

                // Flipped rooms share their portals with the rooms they replace.
                if (!g_Level.Rooms[surface.WaterRoom].Active() || !g_Level.Rooms[surface.DryRoom].Active())
                    continue;

                auto swellBounds = Vector3(0.0f, SWELL_HEIGHT_MAX, 0.0f);
                if (_frustum.AABBInFrustum(surface.BoundsMin - swellBounds, surface.BoundsMax + swellBounds))
                    _waterDraws.push_back(index);
            }
        }
    }

    // Uniforms shared by the surfaces and the underwater fog; the ambient light is set per draw.
    WaterPixelUniforms SceneRenderer::GetWaterUniforms() const
    {
        constexpr auto FRAMES_PER_SECOND = 30.0f;

        auto uniforms = WaterPixelUniforms{};
        uniforms.WaterViewProjection = _viewProjection;
        uniforms.WaterColor = WATER_COLOR;
        uniforms.WaterTime = _frameEnvironment->InterpolatedFrame / FRAMES_PER_SECOND;
        uniforms.WaterAbsorption = WATER_ABSORPTION;
        uniforms.WaterRefractionStrength = WATER_REFRACTION_STRENGTH;
        uniforms.WaterReflectionStrength = WATER_REFLECTION_STRENGTH;
        uniforms.WaterFlowDirection = WATER_FLOW_DIRECTION;
        uniforms.WaterFlowSpeed = WATER_FLOW_SPEED;
        uniforms.WaterWaveTile = WATER_WAVE_TILE;
        uniforms.WaterWaveStrength = WATER_WAVE_STRENGTH;
        uniforms.WaterSpecular = WATER_SPECULAR;
        uniforms.WaterNearPlane = _nearPlane;
        uniforms.WaterFarPlane = _farPlane;
        uniforms.WaterLightCount = (unsigned int)(_roomLightCount & LIGHT_COUNT_MASK);
        uniforms.IsWaterSkyReflected = _isWaterSkyReflected ? 1 : 0;
        return uniforms;
    }

    // Visible surfaces, each with the ambient light of its dry room.
    void SceneRenderer::DrawWaterSurfaceList(ICommandBuffer& commandBuffer, IRenderPass& renderPass, WaterPixelUniforms& uniforms)
    {
        auto vertexBinding = BufferBinding{ _waterVertexBuffer.get(), 0 };
        renderPass.BindVertexBuffers(0, &vertexBinding, 1);

        for (int index : _waterDraws)
        {
            const auto& surface = _waterSurfaces[index];
            const auto& ambientLight = (*_levelGeometry.Rooms)[surface.DryRoom].AmbientLight;
            uniforms.WaterAmbient = Vector3(ambientLight.x, ambientLight.y, ambientLight.z);
            commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 1, &uniforms, sizeof(uniforms));

            CountDraw(DrawCategory::Water, surface.VertexCount / 3);
            renderPass.Draw(surface.VertexCount, 1, surface.FirstVertex, 0);
        }
    }

    // The scene target holds the opaque scene: a copy of it is what the water refracts and reflects. Reflections are
    // traced at half resolution, then the scene pass resumes on its targets and shades the surfaces.
    IRenderPass* SceneRenderer::DrawWaterSurfaces(ICommandBuffer& commandBuffer)
    {
        commandBuffer.PushDebugGroup("Water");

        auto* sceneCopy = CopySceneTarget(commandBuffer);

        const auto& sceneDesc = sceneCopy->GetDesc();
        auto traceDesc = sceneDesc;
        traceDesc.Width = std::max(sceneDesc.Width / 2, 1);
        traceDesc.Height = std::max(sceneDesc.Height / 2, 1);
        auto* reflectionTrace = _targetPool.Acquire(traceDesc);

        // The dummy shadow map array stands in for the sky reflection map when the sky is not reflected.
        TextureSamplerBinding bindings[5] =
        {
            { sceneCopy, _linearClampSampler.get() },
            { _sceneDepthCopy.get(), _pointClampSampler.get() },
            { _waveNormalTexture.get(), _linearWrapSampler.get() },
            { _isWaterSkyReflected ? _skyReflectionTarget.get() : _dummyShadowMap.get(), _linearClampSampler.get() },
            { reflectionTrace, _linearClampSampler.get() }
        };

        auto uniforms = GetWaterUniforms();

        // Reflection trace into the half resolution target; it reads only the first four bindings.
        auto traceTarget = ColorTargetInfo{};
        traceTarget.Texture = reflectionTrace;
        auto* renderPass = commandBuffer.BeginRenderPass(&traceTarget, 1, nullptr);
        renderPass->BindPipeline(_waterReflectionPipeline.get());
        renderPass->BindPixelSamplers(0, bindings, 4);
        DrawWaterSurfaceList(commandBuffer, *renderPass, uniforms);
        commandBuffer.EndRenderPass();

        // Shading, back in the scene pass, with the trace result and the clustered lights.
        renderPass = BeginScenePass(commandBuffer, true);
        renderPass->BindPipeline(_waterPipeline.get());
        renderPass->BindPixelSamplers(0, bindings, 5);

        IBuffer* storageBuffers[2] = { _lightBuffer.Buffer.get(), _lightClusterBuffer.Buffer.get() };
        renderPass->BindPixelStorageBuffers(0, storageBuffers, 2);
        DrawWaterSurfaceList(commandBuffer, *renderPass, uniforms);

        _targetPool.Release(reflectionTrace);
        _targetPool.Release(sceneCopy);
        RestoreSceneUniforms(commandBuffer);

        commandBuffer.PopDebugGroup();
        return renderPass;
    }

    // Pooled copy of the scene target; the caller releases it. Must run outside of a render pass.
    ITexture* SceneRenderer::CopySceneTarget(ICommandBuffer& commandBuffer)
    {
        const auto& sceneDesc = _sceneTarget->GetDesc();
        auto* sceneCopy = _targetPool.Acquire(sceneDesc);

        auto source = TextureRegion{ _sceneTarget.get() };
        source.Width = sceneDesc.Width;
        source.Height = sceneDesc.Height;
        auto destination = source;
        destination.Texture = sceneCopy;

        auto* copyPass = commandBuffer.BeginCopyPass();
        copyPass->CopyTexture(source, destination);
        commandBuffer.EndCopyPass();
        return sceneCopy;
    }

    // Later scene draws read the per draw slots with their own sizes, and the shadow uniforms in pixel slot 2.
    void SceneRenderer::RestoreSceneUniforms(ICommandBuffer& commandBuffer)
    {
        auto drawVertexUniforms = DrawVertexUniforms{ NO_VALUE };
        auto drawPixelUniforms = DrawPixelUniforms{};
        commandBuffer.PushUniforms(RHI::ShaderStage::Vertex, 1, &drawVertexUniforms, sizeof(drawVertexUniforms));
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 1, &drawPixelUniforms, sizeof(drawPixelUniforms));
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &_shadowUniforms, sizeof(_shadowUniforms));
    }

    // The water the camera is in: the lowest surface above the camera, of its room or straight above it, ends the fog
    // of the rays rising out of the water, and its dry room lights the water. Without one, the camera room lights it.
    void SceneRenderer::PrepareUnderwaterFog(const RenderView& view)
    {
        constexpr auto NO_SURFACE_HEIGHT = -1.0e9f;

        _underwaterSurfaceHeight = NO_SURFACE_HEIGHT;
        _underwaterAmbient = Vector3::One;
        if (!_isCameraUnderwater)
            return;

        const auto& camera = view.Camera.WorldPosition;
        const WaterSurface* waterSurface = nullptr;
        for (const auto& surface : _waterSurfaces)
        {
            if (surface.BoundsMin.y > camera.y || (waterSurface != nullptr && surface.BoundsMin.y < waterSurface->BoundsMin.y))
                continue;

            bool isAbove = (camera.x >= surface.BoundsMin.x && camera.x <= surface.BoundsMax.x &&
                            camera.z >= surface.BoundsMin.z && camera.z <= surface.BoundsMax.z);
            if (isAbove || surface.WaterRoom == view.Camera.RoomNumber)
                waterSurface = &surface;
        }

        int ambientRoom = (waterSurface != nullptr) ? waterSurface->DryRoom : view.Camera.RoomNumber;
        if (ambientRoom >= 0 && ambientRoom < (int)_levelGeometry.Rooms->size())
        {
            const auto& ambientLight = (*_levelGeometry.Rooms)[ambientRoom].AmbientLight;
            _underwaterAmbient = Vector3(ambientLight.x, ambientLight.y, ambientLight.z);
        }

        if (waterSurface != nullptr)
            _underwaterSurfaceHeight = waterSurface->BoundsMin.y;
    }

    // The fog absorbs the light of the opaque scene and of the water surfaces, before the transparent draws. It reads
    // a copy of the scene target and writes the target back, then resumes the scene pass for the transparent draws.
    IRenderPass* SceneRenderer::DrawUnderwaterFog(ICommandBuffer& commandBuffer)
    {
        auto* sceneCopy = CopySceneTarget(commandBuffer);

        auto waterUniforms = GetWaterUniforms();
        waterUniforms.WaterAmbient = _underwaterAmbient;
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 1, &waterUniforms, sizeof(waterUniforms));

        auto fogUniforms = UnderwaterFogUniforms{};
        fogUniforms.UnderwaterInverseViewProjection = _viewProjection.Invert();
        fogUniforms.UnderwaterSurfaceHeight = _underwaterSurfaceHeight;
        commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &fogUniforms, sizeof(fogUniforms));

        TextureSamplerBinding bindings[2] =
        {
            { sceneCopy, _pointClampSampler.get() },
            { _sceneDepthCopy.get(), _pointClampSampler.get() }
        };
        RenderFullscreenPass(commandBuffer, _underwaterFogPipeline.get(), _sceneTarget.get(), bindings, 2, "Underwater fog");

        _targetPool.Release(sceneCopy);
        RestoreSceneUniforms(commandBuffer);
        return BeginScenePass(commandBuffer, true);
    }
}
