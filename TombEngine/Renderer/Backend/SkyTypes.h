#pragma once

#include "Renderer/ConstantBuffers/DustStormBuffer.h"
#include "Renderer/ConstantBuffers/GodRayBuffer.h"
#include "Renderer/ConstantBuffers/VolumetricCloudBuffer.h"

// Sky effect draws that the frontend records and the scene renderer replays. They travel in FrameEnvironment and are
// drawn by SceneRendererSky.cpp and SceneRendererAtmosphere.cpp.

namespace TEN::Renderer::Backend
{
    // Volumetric cloud layer draw collected by the frontend, in draw order. The uniforms are the ones the frontend
    // filled, so the cloud animation it advanced is replayed unchanged.
    struct CloudLayerDraw
    {
        int  Layer           = 0;     // 0 for cloud layer A, 1 for cloud layer B.
        bool IsCleared       = false; // Inactive layer: its target is cleared and nothing is drawn.
        bool IsHistoryReset  = false; // Cloud type change: the history target is cleared first.
        bool IsRaymarched    = false;
        bool IsHistoryUsed   = false; // Temporal reprojection: the target is copied to the history before the raymarch.
        bool IsAfterHorizons = false; // Bleed overlay drawn over the horizon meshes.
        bool IsOcclusionQueried = false; // The sun transmittance through the layer is read back for the lens flares.

        ConstantBuffers::CVolumetricCloudBuffer Uniforms = {};
    };

    // Atmospheric sky passes the frontend drew this frame, with the uniform block it filled for them.
    struct SkyEffectFlags
    {
        bool IsDomeDrawn            = false;
        bool IsSunMoonDiscDrawn     = false;
        bool IsAuroraDrawn          = false;
        bool IsAuroraHalfResolution = false; // Low sky quality: the aurora is drawn at half resolution and upscaled.
    };

    // God rays, drawn after the sky when IsDrawn is set.
    struct GodRayDraw
    {
        bool                           IsDrawn  = false;
        ConstantBuffers::CGodRayBuffer Uniforms = {};
    };

    // Horizon layer, with the alpha and gradients the cloud presets gave it.
    struct HorizonLayerDraw
    {
        bool  IsDrawn         = false;
        float Alpha           = 1.0f;
        float GradientFade    = 0.0f;
        float GradientRise    = 0.0f;
        float MeshWorldYMin   = 0.0f; // Camera relative top of the mesh.
        float MeshWorldYRange = 1.0f;
    };

    // Dust storm, drawn over the scene when IsDrawn is set.
    struct DustStormDraw
    {
        bool                              IsDrawn  = false;
        ConstantBuffers::CDustStormBuffer Uniforms = {};
    };

    // Tileable cloud noise generated once by the frontend: 3D RGBA8 noise and 2D RG8 Worley noise.
    struct CloudNoiseData
    {
        const unsigned char* Noise3D      = nullptr;
        int                  Noise3DSize  = 0;
        const unsigned char* Worley2D     = nullptr;
        int                  Worley2DSize = 0;
    };
}
