#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

namespace TEN::Renderer::Backend
{
    // Light clusters split the view frustum into screen tiles and logarithmic depth slices. Each cluster holds a mask
    // of the room lights whose sphere reaches it, so room pixels evaluate only the lights near them instead of every
    // light of the view. The grid is placed in world space, so every view can read it; points outside it evaluate
    // every light. Built on the CPU each frame by CollectObjects, read by LightClusters.hlsli.

    // View depths in world units: slices are logarithmic from the near to the far depth; the first one starts at the
    // minimum depth.
    constexpr auto LIGHT_CLUSTER_NEAR_DEPTH = 256.0f;
    constexpr auto LIGHT_CLUSTER_FAR_DEPTH  = 65536.0f;
    constexpr auto LIGHT_CLUSTER_MIN_DEPTH  = 1.0f;
    constexpr auto LIGHT_CLUSTER_COUNT      = LIGHT_CLUSTER_TILES_X * LIGHT_CLUSTER_TILES_Y * LIGHT_CLUSTER_SLICES;
    constexpr auto LIGHT_CLUSTER_MASK_WORDS = LIGHT_CLUSTER_LIGHTS_MAX / 32;

    static float GetClusterSliceScale()
    {
        return LIGHT_CLUSTER_SLICES / std::log2(LIGHT_CLUSTER_FAR_DEPTH / LIGHT_CLUSTER_NEAR_DEPTH);
    }

    // Same formula as the room shader. The first slice reaches the camera and the last one has no end.
    static int GetClusterSlice(float depth)
    {
        if (depth <= LIGHT_CLUSTER_NEAR_DEPTH)
            return 0;

        return std::clamp((int)std::floor(std::log2(depth / LIGHT_CLUSTER_NEAR_DEPTH) * GetClusterSliceScale()), 0, LIGHT_CLUSTER_SLICES - 1);
    }

    // View depth range of a slice, the inverse of GetClusterSlice.
    static float GetClusterSliceStart(int slice)
    {
        if (slice == 0)
            return LIGHT_CLUSTER_MIN_DEPTH;

        return LIGHT_CLUSTER_NEAR_DEPTH * std::exp2(slice / GetClusterSliceScale());
    }

    static float GetClusterSliceEnd(int slice)
    {
        if (slice == (LIGHT_CLUSTER_SLICES - 1))
            return FLT_MAX;

        return GetClusterSliceStart(slice + 1);
    }

    // Tile of a normalized device coordinate along one screen axis, clamped to the grid.
    static int GetClusterTile(float ndc, int tileCount)
    {
        return std::clamp((int)std::floor((ndc * 0.5f + 0.5f) * tileCount), 0, tileCount - 1);
    }

    // Grid placement read by the shaders: the frame view projection and the slice parameters, and the dynamic light slice
    // of the view, which CollectObjects adds first to the light buffer.
    void SceneRenderer::SetLightClusterUniforms(FramePixelUniforms& uniforms) const
    {
        uniforms.ClusterNearDepth = LIGHT_CLUSTER_NEAR_DEPTH;
        uniforms.ClusterSliceScale = GetClusterSliceScale();
        uniforms.ClusterViewProjection = _viewProjection;
        uniforms.ClusterLightOffset = 0;
        uniforms.ClusterLightCount = (unsigned int)(_roomLightCount & LIGHT_COUNT_MASK);
    }

    // Lights are the room light slice of the frame light buffer; mask bit N selects its light N. Without a perspective
    // projection every cluster holds every light. More lights than LIGHT_CLUSTER_LIGHTS_MAX leave the masks empty: the
    // shaders then loop over every room light. Needs _view and _projection of the frame.
    void SceneRenderer::BuildLightClusters(int lightOffset, int lightCount)
    {
        _lightClusterData.assign(LIGHT_CLUSTER_COUNT * LIGHT_CLUSTER_MASK_WORDS * sizeof(unsigned int), 0);
        if (lightCount <= 0 || lightCount > LIGHT_CLUSTER_LIGHTS_MAX)
            return;

        const auto* lights = (const GpuLight*)_lightData.data() + lightOffset;
        if (_projection._34 != 0.0f)
        {
            for (int i = 0; i < lightCount; i++)
                AddClusterLight(lights[i], i);

            return;
        }

        auto* masks = (unsigned int*)_lightClusterData.data();
        for (int i = 0; i < lightCount; i++)
        {
            for (int cluster = 0; cluster < LIGHT_CLUSTER_COUNT; cluster++)
                masks[cluster * LIGHT_CLUSTER_MASK_WORDS + (i >> 5)] |= 1u << (i & 31);
        }
    }

    // The light sphere is bounded per depth slice by its view space box, cut to the slice depths, whose projected
    // corners give the covered tiles. Clip w is the view depth: z * _34 + _44 for a perspective projection.
    void SceneRenderer::AddClusterLight(const GpuLight& light, int lightIndex)
    {
        if (light.Type != (unsigned int)LightType::Point && light.Type != (unsigned int)LightType::Spot)
            return;

        auto center = Vector4::Transform(Vector4(light.Position.x, light.Position.y, light.Position.z, 1.0f), _view);
        float radius = light.Out;
        float centerDepth = (center.z * _projection._34) + _projection._44;
        float minDepth = centerDepth - radius;
        float maxDepth = centerDepth + radius;
        if (maxDepth <= LIGHT_CLUSTER_MIN_DEPTH)
            return;

        auto* masks = (unsigned int*)_lightClusterData.data();
        unsigned int bit = 1u << (lightIndex & 31);
        int word = lightIndex >> 5;

        int lastSlice = GetClusterSlice(maxDepth);
        for (int slice = GetClusterSlice(std::max(minDepth, 0.0f)); slice <= lastSlice; slice++)
        {
            float depths[2] = { std::max(GetClusterSliceStart(slice), minDepth), std::min(GetClusterSliceEnd(slice), maxDepth) };
            depths[0] = std::max(depths[0], LIGHT_CLUSTER_MIN_DEPTH);
            if (depths[0] > depths[1])
                continue;

            auto rectMin = Vector2(FLT_MAX);
            auto rectMax = Vector2(-FLT_MAX);
            for (int corner = 0; corner < 8; corner++)
            {
                float viewZ = (depths[(corner >> 2) & 1] - _projection._44) / _projection._34;
                auto point = Vector4(center.x + ((corner & 1) ? radius : -radius), center.y + ((corner & 2) ? radius : -radius), viewZ, 1.0f);
                auto clip = Vector4::Transform(point, _projection);
                auto ndc = Vector2(clip.x / clip.w, clip.y / clip.w);
                rectMin = Vector2::Min(rectMin, ndc);
                rectMax = Vector2::Max(rectMax, ndc);
            }

            if (rectMax.x < -1.0f || rectMin.x > 1.0f || rectMax.y < -1.0f || rectMin.y > 1.0f)
                continue;

            int firstX = GetClusterTile(rectMin.x, LIGHT_CLUSTER_TILES_X);
            int lastX = GetClusterTile(rectMax.x, LIGHT_CLUSTER_TILES_X);
            int firstY = GetClusterTile(rectMin.y, LIGHT_CLUSTER_TILES_Y);
            int lastY = GetClusterTile(rectMax.y, LIGHT_CLUSTER_TILES_Y);
            for (int y = firstY; y <= lastY; y++)
            {
                for (int x = firstX; x <= lastX; x++)
                {
                    int cluster = ((slice * LIGHT_CLUSTER_TILES_Y) + y) * LIGHT_CLUSTER_TILES_X + x;
                    masks[cluster * LIGHT_CLUSTER_MASK_WORDS + word] |= bit;
                }
            }
        }
    }
}
