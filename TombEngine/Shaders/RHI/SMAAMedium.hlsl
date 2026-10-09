// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=0
// TEN_SHADER: Pixel PSEdgeDetection samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3
// TEN_SHADER: Pixel PSBlendingWeight samplers=3 storage_textures=0 storage_buffers=0 uniform_buffers=3
// TEN_SHADER: Pixel PSNeighborhoodBlending samplers=2 storage_textures=0 storage_buffers=0 uniform_buffers=3

// SMAA for the medium antialiasing mode.

#define SMAA_PRESET_MEDIUM
#include "./SMAACommon.hlsli"
