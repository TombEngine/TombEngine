#pragma once

#include "Renderer/ConstantBuffers/VolumetricCloudBuffer.h"

// Sky effect draws that the legacy renderer records and the next renderer replays.

namespace TEN::Renderer::Next
{
    // Volumetric cloud layer draw of the legacy DrawHorizonAndSky, in draw order. The uniforms are those of the
    // legacy draw, so the cloud animation advanced by the legacy renderer is replayed unchanged.
    struct CloudLayerDraw
    {
        int  Layer           = 0;     // 0 for cloud layer A, 1 for cloud layer B.
        bool IsCleared       = false; // Inactive layer: its target is cleared and nothing is drawn.
        bool IsHistoryReset  = false; // Cloud type change: the history target is cleared first.
        bool IsRaymarched    = false;
        bool IsHistoryUsed   = false; // Temporal reprojection: the target is copied to the history before the raymarch.
        bool IsAfterHorizons = false; // Bleed overlay drawn over the horizon meshes.

        ConstantBuffers::CVolumetricCloudBuffer Uniforms = {};
    };

    // Atmospheric sky passes the legacy renderer drew this frame, with the uniform block it filled for them.
    struct SkyEffectFlags
    {
        bool IsDomeDrawn            = false;
        bool IsSunMoonDiscDrawn     = false;
        bool IsAuroraDrawn          = false;
        bool IsAuroraHalfResolution = false; // Low sky quality: the aurora is drawn at half resolution and upscaled.
    };

    // Tileable cloud noise generated once by the legacy renderer: 3D RGBA8 noise and 2D RG8 Worley noise.
    struct CloudNoiseData
    {
        const unsigned char* Noise3D      = nullptr;
        int                  Noise3DSize  = 0;
        const unsigned char* Worley2D     = nullptr;
        int                  Worley2DSize = 0;
    };
}
