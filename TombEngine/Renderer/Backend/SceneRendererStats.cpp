#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

#include "Renderer/Scene/RenderView.h"

using namespace TEN::Renderer::RHI;

namespace TEN::Renderer::Backend
{
    float GetMilliseconds(std::chrono::steady_clock::time_point start, std::chrono::steady_clock::time_point end)
    {
        return std::chrono::duration<float, std::milli>(end - start).count();
    }

    const char* GetDrawCategoryName(DrawCategory category)
    {
        constexpr const char* NAMES[] =
        {
            "Rooms", "Statics", "Moveables", "Sprites", "Sorted rooms", "Sorted statics", "Sorted moveables", "Sorted sprites",
            "Shadow map", "Debris", "Sky", "Fullscreen passes", "2D layer", "Debug primitives"
        };

        static_assert(sizeof(NAMES) / sizeof(NAMES[0]) == (int)DrawCategory::Count, "Draw category names.");
        return NAMES[std::clamp((int)category, 0, (int)DrawCategory::Count - 1)];
    }

    // Counters restart with each scene frame; the pass list keeps its capacity.
    void SceneRenderer::ResetFrameStats()
    {
        auto passes = std::move(_frameStats.Passes);
        passes.clear();
        _frameStats = SceneFrameStats{};
        _frameStats.Passes = std::move(passes);
    }

    void SceneRenderer::CountDraw(DrawCategory category, int triangles)
    {
        _frameStats.DrawCalls++;
        _frameStats.CategoryDrawCalls[(int)category]++;
        _frameStats.CategoryTriangles[(int)category] += triangles;
    }

    // Counts of the collected frame data, taken after the passes are recorded.
    void SceneRenderer::CollectFrameStats(const RenderView& view, const FrameEnvironment& environment)
    {
        auto& stats = _frameStats;
        stats.RoomsDrawn = (int)view.RoomsToDraw.size();
        stats.RoomDraws = (int)_roomDraws.size();
        stats.ObjectDraws = (int)_objectDraws.size();
        stats.ShadowDraws = (int)_shadowDraws.size();
        stats.Instances = (int)(_instanceData.size() / sizeof(GpuObjectInstance));
        stats.Bones = (int)(_boneData.size() / sizeof(Matrix));
        stats.Lights = (int)(_lightData.size() / sizeof(GpuLight));
        stats.Decals = (int)(_decalData.size() / sizeof(GpuDecal));
        stats.Materials = (int)(_materialData.size() / sizeof(GpuMaterial));
        stats.Sprites = (int)(_spriteData.size() / sizeof(GpuSprite));
        stats.SpriteRuns = (int)_spriteDraws.size();
        stats.SkySprites = 0;
        for (const auto& draw : _skySpriteDraws)
            stats.SkySprites += draw.SpriteCount;

        stats.SortedDraws = (int)_sortedDraws.size();
        stats.SortedIndices = (int)(_sortedIndexData.size() / sizeof(int));
        stats.DebrisTriangles = (int)(_debrisVertexData.size() / sizeof(Structures::Vertex) / 3);
        stats.Mirrors = (int)view.Mirrors.size();
        stats.GunFlashes = (int)_gunFlashDraws.size();
        stats.FogBulbs = (int)view.FogBulbsToDraw.size();
        stats.CloudDraws = (environment.CloudDraws != nullptr) ? (int)environment.CloudDraws->size() : 0;
        stats.DebugVertices = _debugLineVertexCount + _debugTriangleVertexCount;
    }

    SceneRendererInfo SceneRenderer::GetInfo() const
    {
        auto info = SceneRendererInfo{};
        if (_device == nullptr)
            return info;

        constexpr const char* BACKEND_NAMES[] = { "None", "DirectX 11", "Vulkan", "Direct3D 12", "OpenGL" };
        int backend = (int)_device->GetBackend();
        info.Backend = (backend >= 0 && backend < (int)std::size(BACKEND_NAMES)) ? BACKEND_NAMES[backend] : "Unknown";
        info.DeviceName = _device->GetDeviceName();
        info.Width = (_sceneTarget != nullptr) ? _sceneTarget->GetDesc().Width : 0;
        info.Height = (_sceneTarget != nullptr) ? _sceneTarget->GetDesc().Height : 0;
        info.RoomVertices = _geometrySections[(int)MeshSource::Rooms].VertexCount;
        info.ObjectVertices = _geometrySections[(int)MeshSource::Statics].VertexCount + _geometrySections[(int)MeshSource::Moveables].VertexCount;
        info.TextureArrayGroups = (int)_textureArrayGroups.size();
        info.TexturePages = (int)(_roomTextures.size() + _staticTextures.size() + _moveableTextures.size() + _animatedTextures.size());
        info.PooledTargets = _targetPool.GetCount();
        info.UploadRingSize = _uploadRing.GetCapacity();

        for (const auto& shaderPipelines : _scenePipelines)
        {
            for (const auto& modePipelines : shaderPipelines)
            {
                for (const auto& pipeline : modePipelines)
                    info.ScenePipelines += (pipeline != nullptr) ? 1 : 0;
            }
        }

        return info;
    }

    // GPU pass timings averaged over a second; SDL GPU measures them only while requested.
    const std::vector<GpuTiming>& SceneRenderer::SampleGpuTimings()
    {
        constexpr auto SAMPLE_INTERVAL = 60;

        _isGpuTimingRequested = true;

        if (_device != nullptr && ++_gpuTimingFrames >= SAMPLE_INTERVAL)
        {
            auto timings = _device->ConsumeGpuTimings();
            if (!timings.empty())
                _gpuTimings = std::move(timings);

            _gpuTimingFrames = 0;
        }

        return _gpuTimings;
    }
}
