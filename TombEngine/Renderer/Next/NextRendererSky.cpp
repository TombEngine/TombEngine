#include "framework.h"
#include "Renderer/Next/NextRenderer.h"

#include "Renderer/RenderView.h"
#include "Renderer/RHI/ShaderLibrary.h"

using namespace TEN::Renderer::RHI;

namespace TEN::Renderer::Next
{
    // Same layout as CloudCameraUniforms in Shaders/RHI/VolumetricClouds.hlsl.
    struct CloudCameraUniforms
    {
        Matrix  InverseView       = Matrix::Identity;
        Matrix  InverseProjection = Matrix::Identity;
        Vector4 CameraPosition    = Vector4::Zero;
    };

    bool NextRenderer::CreateCloudResources()
    {
        _cloudVertexShader = LoadShader(*_device, _shaderDirectory, "VolumetricClouds", "VS");
        _cloudRaymarchPixelShader = LoadShader(*_device, _shaderDirectory, "VolumetricClouds", "PS");
        _cloudCompositePixelShader = LoadShader(*_device, _shaderDirectory, "VolumetricClouds", "PSCloudComposite");
        if (_cloudVertexShader == nullptr || _cloudRaymarchPixelShader == nullptr || _cloudCompositePixelShader == nullptr)
            return false;

        // Both passes are opaque fullscreen triangles: the composite shader blends with its copy of the scene.
        auto desc = GraphicsPipelineDesc{};
        desc.VertexShader = _cloudVertexShader.get();
        desc.PixelShader = _cloudRaymarchPixelShader.get();
        desc.Rasterizer.Cull = RHI::CullMode::None;
        desc.DepthStencil.EnableDepthTest = false;
        desc.DepthStencil.EnableDepthWrite = false;
        desc.NumColorTargets = 1;
        desc.ColorTargets[0].Format = PixelFormat::RGBA16Float;
        desc.Name = "Clouds.Raymarch";
        _cloudRaymarchPipeline = _device->CreateGraphicsPipeline(desc);

        desc.PixelShader = _cloudCompositePixelShader.get();
        desc.ColorTargets[0].Format = _device->GetSwapchainFormat();
        desc.Name = "Clouds.Composite";
        _cloudCompositePipeline = _device->CreateGraphicsPipeline(desc);

        // Cloud noise tiles.
        auto samplerDesc = SamplerDesc{};
        samplerDesc.Name = "LinearWrap";
        _linearWrapSampler = _device->CreateSampler(samplerDesc);

        return (_cloudRaymarchPipeline != nullptr && _cloudCompositePipeline != nullptr && _linearWrapSampler != nullptr);
    }

    // The noise is generated once by the legacy renderer; it is uploaded on the first frame that provides it.
    void NextRenderer::EnsureCloudNoise(const CloudNoiseData& noise)
    {
        if (_cloudNoise3D != nullptr || noise.Noise3D == nullptr || noise.Worley2D == nullptr)
            return;

        auto desc = TextureDesc{};
        desc.Type = TextureType::Texture3D;
        desc.Width = desc.Height = desc.DepthOrLayers = noise.Noise3DSize;
        desc.Format = PixelFormat::RGBA8Unorm;
        desc.Name = "Clouds.Noise3D";
        auto noise3D = _device->CreateTexture(desc);

        desc.Type = TextureType::Texture2D;
        desc.Width = desc.Height = noise.Worley2DSize;
        desc.DepthOrLayers = 1;
        desc.Format = PixelFormat::RG8Unorm;
        desc.Name = "Clouds.Worley2D";
        auto worley2D = _device->CreateTexture(desc);

        if (noise3D == nullptr || worley2D == nullptr)
            return;

        int noise3DSize = noise.Noise3DSize;
        UploadTexture(noise3D.get(), noise.Noise3D, noise3DSize * noise3DSize * noise3DSize * 4);
        UploadTexture(worley2D.get(), noise.Worley2D, noise.Worley2DSize * noise.Worley2DSize * 2);
        _cloudNoise3D = std::move(noise3D);
        _cloudWorley2D = std::move(worley2D);
    }

    // Cloud layer targets and their histories, at the cloud resolution of the legacy uniforms. New targets are
    // cleared, as the legacy targets start transparent.
    bool NextRenderer::EnsureCloudTargets(ICommandBuffer& commandBuffer, int width, int height)
    {
        if (_cloudTargets[0] != nullptr && _cloudTargets[0]->GetDesc().Width == width && _cloudTargets[0]->GetDesc().Height == height)
            return true;

        auto desc = TextureDesc{};
        desc.Width = width;
        desc.Height = height;
        desc.Format = PixelFormat::RGBA16Float;
        desc.Usage = TextureUsage::ColorTarget | TextureUsage::Sampler;
        for (int layer = 0; layer < CLOUD_LAYER_COUNT; layer++)
        {
            desc.Name = "Clouds.Layer" + std::to_string(layer);
            _cloudTargets[layer] = _device->CreateTexture(desc);
            desc.Name += ".History";
            _cloudHistoryTargets[layer] = _device->CreateTexture(desc);
            if (_cloudTargets[layer] == nullptr || _cloudHistoryTargets[layer] == nullptr)
                return false;

            ClearColorTarget(commandBuffer, _cloudTargets[layer].get());
            ClearColorTarget(commandBuffer, _cloudHistoryTargets[layer].get());
        }

        return true;
    }

    void NextRenderer::ClearColorTarget(ICommandBuffer& commandBuffer, ITexture* texture)
    {
        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = texture;
        colorTarget.Load = LoadOp::Clear;
        commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
        commandBuffer.EndRenderPass();
    }

    bool NextRenderer::HasCloudDraws(bool isAfterHorizons) const
    {
        const auto* draws = _frameEnvironment->CloudDraws;
        if (draws == nullptr || _cloudNoise3D == nullptr)
            return false;

        for (const auto& draw : *draws)
        {
            if (draw.IsAfterHorizons == isAfterHorizons)
                return true;
        }

        return false;
    }

    // Replays the legacy cloud draws before or after the horizon meshes, outside of the sky render pass.
    void NextRenderer::DrawCloudLayers(ICommandBuffer& commandBuffer, bool isAfterHorizons)
    {
        const auto& cameraPosition = _frameEnvironment->CameraPosition;
        auto cameraUniforms = CloudCameraUniforms{ _view.Invert(), _projection.Invert(), Vector4(cameraPosition.x, cameraPosition.y, cameraPosition.z, 1.0f) };

        commandBuffer.PushDebugGroup("Clouds");
        for (const auto& draw : *_frameEnvironment->CloudDraws)
        {
            if (draw.IsAfterHorizons != isAfterHorizons || draw.Layer < 0 || draw.Layer >= CLOUD_LAYER_COUNT)
                continue;

            // Cleared layers carry no uniforms; their targets only exist once a layer was drawn.
            if (draw.IsCleared)
            {
                if (_cloudTargets[draw.Layer] != nullptr)
                    ClearColorTarget(commandBuffer, _cloudTargets[draw.Layer].get());

                continue;
            }

            int width = std::max((int)draw.Uniforms.CloudRenderSize.x, 1);
            int height = std::max((int)draw.Uniforms.CloudRenderSize.y, 1);
            if (!EnsureCloudTargets(commandBuffer, width, height))
                break;

            commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 2, &draw.Uniforms, sizeof(draw.Uniforms));
            commandBuffer.PushUniforms(RHI::ShaderStage::Pixel, 3, &cameraUniforms, sizeof(cameraUniforms));

            if (draw.IsHistoryReset)
                ClearColorTarget(commandBuffer, _cloudHistoryTargets[draw.Layer].get());

            if (draw.IsRaymarched)
                RaymarchCloudLayer(commandBuffer, draw);

            CompositeCloudLayer(commandBuffer, draw.Layer);
        }

        commandBuffer.PopDebugGroup();
    }

    // Legacy pass 1: the current target becomes the history of the temporal reprojection, then the layer is raymarched.
    void NextRenderer::RaymarchCloudLayer(ICommandBuffer& commandBuffer, const CloudLayerDraw& draw)
    {
        auto* target = _cloudTargets[draw.Layer].get();
        auto* history = _cloudHistoryTargets[draw.Layer].get();
        if (draw.IsHistoryUsed)
            commandBuffer.BlitTexture(target, history);

        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = target;
        colorTarget.Load = LoadOp::Clear;

        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
        renderPass->BindPipeline(_cloudRaymarchPipeline.get());

        TextureSamplerBinding bindings[3] =
        {
            { history, _linearClampSampler.get() },
            { _cloudNoise3D.get(), _linearWrapSampler.get() },
            { _cloudWorley2D.get(), _linearWrapSampler.get() }
        };
        renderPass->BindPixelSamplers(0, bindings, 3);

        _frameStats.DrawCalls++;
        renderPass->Draw(3, 1, 0, 0);
        commandBuffer.EndRenderPass();
    }

    // Legacy pass 2: the composite shader blends the layer with a copy of the scene and writes the coverage to alpha.
    void NextRenderer::CompositeCloudLayer(ICommandBuffer& commandBuffer, int layer)
    {
        auto* sceneCopy = _targetPool.Acquire(_sceneTarget->GetDesc());
        if (sceneCopy == nullptr)
            return;

        commandBuffer.BlitTexture(_sceneTarget.get(), sceneCopy);

        auto colorTarget = ColorTargetInfo{};
        colorTarget.Texture = _sceneTarget.get();
        colorTarget.Load = LoadOp::Load;

        auto* renderPass = commandBuffer.BeginRenderPass(&colorTarget, 1, nullptr);
        renderPass->BindPipeline(_cloudCompositePipeline.get());

        TextureSamplerBinding bindings[2] =
        {
            { _cloudTargets[layer].get(), _linearClampSampler.get() },
            { sceneCopy, _linearClampSampler.get() }
        };
        renderPass->BindPixelSamplers(0, bindings, 2);

        _frameStats.DrawCalls++;
        renderPass->Draw(3, 1, 0, 0);
        commandBuffer.EndRenderPass();
        _targetPool.Release(sceneCopy);
    }

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
    // Cloud layers composite over the sky background and, for the bleed overlay, over the horizon meshes.
    void NextRenderer::RenderSkyPass(ICommandBuffer& commandBuffer)
    {
        const auto& view = *_frameView;

        auto* renderPass = BeginSkyRenderPass(commandBuffer, LoadOp::Clear);
        if (!IsSkyVisible(view))
        {
            commandBuffer.EndRenderPass();
            return;
        }

        DrawSkyBackground(commandBuffer, *renderPass, view, 0);
        if (HasCloudDraws(false) || HasSkyDiscsOrAurora())
        {
            commandBuffer.EndRenderPass();
            if (HasCloudDraws(false))
                DrawCloudLayers(commandBuffer, false);

            DrawSkyDiscsAndAurora(commandBuffer);
            renderPass = BeginSkyRenderPass(commandBuffer, LoadOp::Load);
        }

        DrawSkyHorizons(commandBuffer, *renderPass, view, _frameEnvironment->InterpolationFactor, 0);
        commandBuffer.EndRenderPass();

        if (HasCloudDraws(true))
            DrawCloudLayers(commandBuffer, true);
    }
}
