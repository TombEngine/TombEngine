// ============================================================================
// RendererDualClouds.cpp — Dual volumetric cloud layer rendering integration
//
// This file provides the renderer-side entry points for the new layered
// sky/cloud system.  It is called from the main render pipeline after the
// legacy DrawHorizonAndSky() pass.
//
// Rendering order:
//   1. Legacy SkyLayer1  (bitmap, additive)           — DrawHorizonAndSky()
//   2. Legacy SkyLayer2  (bitmap, additive)           — DrawHorizonAndSky()
//   3. Volumetric Cloud Layer A (high/thin, alpha)    — DrawDualVolumetricClouds()
//   4. Volumetric Cloud Layer B (low/dense, alpha)    — DrawDualVolumetricClouds()
//
// The two volumetric passes reuse the same shader and constant buffer slot
// (register b9).  Each is rendered into its own half-res target, then
// composited back-to-front over the scene:
//   - Cloud A composited first (farther / higher)
//   - Cloud B composited second (closer / lower / denser)
//
// The draws replay the records of PrepareCloudLayers (RendererSkyEffects.cpp); the
// next renderer computes the lens flare occlusion of each layer.
// ============================================================================

#include "framework.h"
#include "Renderer/Renderer.h"

#include <DirectXPackedVector.h>
#include "Game/control/control.h"
#include "Game/camera.h"
#include "Game/Sky/SkyCloudSystem.h"
#include "Renderer/VolumetricCloud/VolumetricCloud.h"
#include "Renderer/ConstantBuffers/VolumetricCloudBuffer.h"
#include "Scripting/Include/Flow/ScriptInterfaceFlowHandler.h"
#include "Scripting/Include/ScriptInterfaceLevel.h"
#include "Scripting/Internal/TEN/Flow/Level/FlowLevel.h"
#include "Sound/sound.h"
#include "Sound/sound_effects.h"
#include "Specific/level.h"
#include "Specific/trutils.h"

using namespace TEN::Renderer::VolumetricCloud;
using namespace TEN::Renderer::ConstantBuffers;
using namespace TEN::Sky;

namespace TEN::Renderer
{
	// ========================================================================
	// Dual-layer cloud initialization
	// ========================================================================

	void Renderer::InitializeDualVolumetricClouds()
	{
		// CB is shared — already created by InitializeVolumetricClouds().
		// Create render targets for layer B.
		ResizeDualCloudTargets();
	}

	void Renderer::ResizeDualCloudTargets()
	{
		// Layer A targets (reuse existing _cloudRenderTarget / _cloudOcclusionTarget).

		// Layer B target — same format, same resolution as layer A.
		float scale = _cloudStateB.ActiveQuality.RenderResolutionScale;
		int w = std::max(1, (int)(_graphicsDevice->GetScreenWidth() * scale));
		int h = std::max(1, (int)(_graphicsDevice->GetScreenHeight() * scale));

		// Release previous Layer B targets/buffers before recreation.
		SAFE_DELETE(_cloudRenderTargetB);
		SAFE_DELETE(_cloudPrevFrameRTB);
		SAFE_DELETE(_cloudOcclusionTargetB);
		SAFE_DELETE(_cloudOcclusionReadbackB);

		_cloudRenderTargetB = _graphicsDevice->CreateRenderSurface2D(
			w, h,
			SurfaceFormat::SF_RGBA16_Float,
			false,
			DepthFormat::None);

		// Previous-frame RT for temporal checkerboard on layer B.
		_cloudPrevFrameRTB = _graphicsDevice->CreateRenderSurface2D(
			w, h,
			SurfaceFormat::SF_RGBA16_Float,
			false,
			DepthFormat::None);

		// 1x1 occlusion target for layer B (must match the source size of the
		// async readback so CopyResource succeeds — DX11 requires identical dims).
		_cloudOcclusionTargetB = _graphicsDevice->CreateRenderSurface2D(
			1, 1,
			SurfaceFormat::SF_R16_Float,
			false,
			DepthFormat::None);

		_cloudOcclusionReadbackB = _graphicsDevice->CreateGpuReadbackBuffer(
			1, 1, SurfaceFormat::SF_R16_Float);
	}

	// ========================================================================
	// Draw the volumetric cloud layers prepared by PrepareCloudLayers
	// ========================================================================

	// ========================================================================
	// Draw a single volumetric cloud layer into its render target
	// ========================================================================

}
