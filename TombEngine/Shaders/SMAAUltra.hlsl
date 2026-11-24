// TEN_SHADER: Vertex VS samplers=0 storage_textures=0 storage_buffers=0 uniform_buffers=0
// TEN_SHADER: Pixel PSEdgeDetection samplers=1 storage_textures=0 storage_buffers=0 uniform_buffers=3
// TEN_SHADER: Pixel PSBlendingWeight samplers=3 storage_textures=0 storage_buffers=0 uniform_buffers=3
// TEN_SHADER: Pixel PSNeighborhoodBlending samplers=2 storage_textures=0 storage_buffers=0 uniform_buffers=3

// SMAA for the high antialiasing mode.

// Ultra preset: edge threshold 0.05, 32 search steps and 16 diagonal steps, with corner rounding.
#define SMAA_PRESET_ULTRA
#include "./SMAACommon.hlsli"
