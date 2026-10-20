#ifndef TEN_LIGHT_CLUSTERS
#define TEN_LIGHT_CLUSTERS

// Light clusters of the room lights (SceneRendererLightClusters.cpp). The including shader declares the frame pixel
// uniforms and the LightClusters buffer.

// Mask of the room lights reaching the light cluster of a position; false outside the cluster grid.
bool GetClusterLightMask(float3 position, out uint2 mask)
{
    mask = uint2(0, 0);
    if (ClusterSliceScale <= 0.0f)
        return false;

    float4 clip = mul(float4(position, 1.0f), ClusterViewProjection);
    if (clip.w <= 0.0f)
        return false;

    float2 ndc = clip.xy / clip.w;
    if (any(abs(ndc) > 1.0f))
        return false;

    uint2 tile = min((uint2)((ndc * 0.5f + 0.5f) * float2(LIGHT_CLUSTER_TILES_X, LIGHT_CLUSTER_TILES_Y)), uint2(LIGHT_CLUSTER_TILES_X - 1, LIGHT_CLUSTER_TILES_Y - 1));
    uint slice = (uint)clamp(floor(log2(max(clip.w / ClusterNearDepth, 1.0f)) * ClusterSliceScale), 0.0f, LIGHT_CLUSTER_SLICES - 1.0f);
    uint cluster = ((slice * LIGHT_CLUSTER_TILES_Y) + tile.y) * LIGHT_CLUSTER_TILES_X + tile.x;
    mask = TEN_LOAD_RAW(uint2, LightClusters, cluster);
    return true;
}

#endif // TEN_LIGHT_CLUSTERS
