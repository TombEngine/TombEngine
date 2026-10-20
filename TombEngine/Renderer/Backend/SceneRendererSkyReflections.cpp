#include "framework.h"
#include "Renderer/Backend/SceneRenderer.h"

#include "Renderer/Scene/RenderView.h"

using namespace TEN::Renderer::RHI;

namespace TEN::Renderer::Backend
{
    constexpr auto SKY_REFLECTION_MAP_SIZE = 512;

    bool SceneRenderer::CreateSkyReflectionResources()
    {
        auto desc = TextureDesc{};
        desc.Type = TextureType::Texture2DArray;
        desc.Width = SKY_REFLECTION_MAP_SIZE;
        desc.Height = SKY_REFLECTION_MAP_SIZE;
        desc.DepthOrLayers = 2;
        desc.Format = PixelFormat::RGBA8Unorm;
        desc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        desc.Name = "SkyReflections";
        _skyReflectionTarget = _device->CreateTexture(desc);
        return (_skyReflectionTarget != nullptr);
    }

    // Sky layers and horizons into both hemispheres of the dual paraboloid map, refreshed every frame while
    // skybox reflective materials are in use.
    void SceneRenderer::RenderSkyReflections(ICommandBuffer& commandBuffer)
    {
        for (int layer = 0; layer < 2; layer++)
        {
            auto colorTarget = ColorTargetInfo{};
            colorTarget.Texture = _skyReflectionTarget.get();
            colorTarget.Layer = layer;
            colorTarget.ClearColor[3] = 1.0f;

            auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
            DrawSky(commandBuffer, *renderPass, *_frameView, _frameEnvironment->InterpolationFactor, (layer == 0) ? -1 : 1);
            commandBuffer.EndRenderPass();
        }
    }
}
