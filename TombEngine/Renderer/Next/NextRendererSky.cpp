#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Renderer/RenderView.h"

using namespace TEN::Renderer::RHI;

namespace TEN::Renderer::Next
{
    // Scene target with the depth of the prepass, which the sky pipelines neither test nor write.
    IRenderPass* NextRenderer::BeginSkyRenderPass(ICommandBuffer& commandBuffer, LoadOp load)
    {
        const auto& fogColor = _frameEnvironment->FogColor;

        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = _sceneTarget.get();
        colorTarget.Load = load;
        colorTarget.ClearColor[0] = fogColor.x;
        colorTarget.ClearColor[1] = fogColor.y;
        colorTarget.ClearColor[2] = fogColor.z;
        colorTarget.ClearColor[3] = 1.0f;

        auto depthTarget = DepthTargetInfo{};
        depthTarget.Texture = _depthTarget.get();
        depthTarget.Load = LoadOp::Load;

        return commandBuffer.BeginRenderPass(&colorTarget, 1, &depthTarget);
    }

    // Sky of the view before the scene pass, which loads the result, in the order of the legacy DrawHorizonAndSky.
    void NextRenderer::RenderSkyPass(ICommandBuffer& commandBuffer)
    {
        const auto& view = *_frameView;

        auto* renderPass = BeginSkyRenderPass(commandBuffer, LoadOp::Clear);
        if (IsSkyVisible(view))
        {
            DrawSkyBackground(commandBuffer, *renderPass, view, 0);
            DrawSkyHorizons(commandBuffer, *renderPass, view, _frameEnvironment->InterpolationFactor, 0);
        }

        commandBuffer.EndRenderPass();
    }
}
